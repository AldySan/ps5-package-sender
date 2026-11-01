#include "Application.h"

#include <QCoreApplication>
#include <QDir>
#include <QClipboard>
#include <QGuiApplication>

#include <memory>
#include <QFileInfo>
#include <QUrl>

#include "core/PkgInfo.h"
#include "core/ScanPaths.h"
#include "db/Database.h"
#include "util/Format.h"

#include <QJsonArray>
#include <QJsonObject>

namespace ps5 {

namespace {

constexpr int PollIntervalMs = 1000;
constexpr int OfflineAfterMissedPolls = 3;

} // namespace

Application::Application(QObject *parent)
    : QObject(parent),
      m_version(QCoreApplication::applicationVersion()),
      m_title(QStringLiteral("PS5 Package Sender"))
{
    Database::instance().open();

    m_library = new LibraryModel(this);
    m_library->reload();

    m_scanner = new LibraryScanner(this);
    connect(m_scanner, &LibraryScanner::progress, this, [this](int found, int parsed) {
        m_scanStatus = tr("Scanning… %1 found, %2 new").arg(found).arg(parsed);
        emit scanStatusChanged();
    });
    connect(m_scanner, &LibraryScanner::finished, this, [this] {
        m_scanning = false;
        m_scanStatus = tr("Scan complete");
        emit scanningChanged();
        emit scanStatusChanged();
        pruneMissing();
        m_library->reload();
    });

    m_receiver = new ReceiverClient(this);
    m_psIp = Database::instance().setting(QStringLiteral("psIp"));
    m_pcIp = Database::instance().setting(QStringLiteral("pcIp"));
    m_receiver->setConsoleIp(m_psIp);

    m_discovery = new Discovery(this);
    connect(m_discovery, &Discovery::progressMessage, this, [this](const QString &msg) {
        m_discoveryStatus = msg;
        emit discoveringChanged();
    });
    connect(m_discovery, &Discovery::consolesFound, this, [this](const QStringList &ips) {
        m_discovering = false;
        m_discoveryStatus = tr("Done");
        emit discoveringChanged();
        probeConsoles(ips);
    });

    m_server = new RangeFileServer(this);
    m_server->setCatalogProvider([] {
        QJsonArray catalog;
        for (const LibraryRow &row : Database::instance().libraryRows()) {
            QJsonObject o;
            o.insert(QStringLiteral("id"), QString::number(row.id));
            o.insert(QStringLiteral("title"), row.title);
            o.insert(QStringLiteral("titleId"), row.titleId);
            o.insert(QStringLiteral("platform"), row.platform);
            o.insert(QStringLiteral("version"), row.version);
            o.insert(QStringLiteral("role"), PkgReader::isImageFile(row.format)
                ? QStringLiteral("Image")
                : (row.isDlc ? QStringLiteral("DLC") : QStringLiteral("Game")));
            o.insert(QStringLiteral("format"), row.format);
            o.insert(QStringLiteral("familyKey"),
                     row.titleId.isEmpty() ? row.path : row.titleId);
            o.insert(QStringLiteral("file"),
                     QFileInfo(row.path).fileName());
            o.insert(QStringLiteral("size"), double(row.size));
            o.insert(QStringLiteral("sizeText"), formatSize(row.size));
            o.insert(QStringLiteral("hasIcon"), !row.icon.isEmpty());
            catalog.append(o);
        }
        return catalog;
    });
    m_server->setIconProvider([](const QString &id) {
        return Database::instance().libraryIcon(id.toLongLong());
    });
    if (!m_server->start())
        emit notify(tr("Could not bind the PC file server port"));

    m_queue = new TransferQueue(m_receiver, m_server, this);
    m_queue->setPcIp(m_pcIp);
    m_queue->setDestination(Database::instance().setting(
        QStringLiteral("destination"), QStringLiteral("/data/homebrew")));
    connect(m_queue, &TransferQueue::notify, this, &Application::notify);
    connect(m_queue, &TransferQueue::queueChanged, this, [this] {
        m_historyRows = buildHistoryRows();
    refreshScanPaths();
        emit historyChanged();
    });
    m_historyRows = buildHistoryRows();

    refreshPcAddresses();

    m_pollTimer.setInterval(PollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &Application::pollReceiver);
    m_pollTimer.start();

    if (!Database::instance().scannedRoots().isEmpty())
        startScan(Database::instance().scannedRoots());
}

QString Application::lastPickFolder() const
{
    return Database::instance().setting(QStringLiteral("lastPickFolder"));
}

void Application::setLastPickFolder(const QString &folder)
{
    Database::instance().setSetting(QStringLiteral("lastPickFolder"), folder);
    emit lastPickFolderChanged();
}

void Application::setPsIp(const QString &ip)
{
    const QString trimmed = ip.trimmed();
    if (m_psIp == trimmed)
        return;
    m_psIp = trimmed;
    m_receiver->setConsoleIp(m_psIp);
    saveIpSettings();
    m_missedPolls = 0;
    pollReceiver();
    emit psIpChanged();
}

void Application::setPcIp(const QString &ip)
{
    const QString trimmed = ip.trimmed();
    if (m_pcIp == trimmed)
        return;
    m_pcIp = trimmed;
    saveIpSettings();
    emit pcIpChanged();
    if (m_queue)
        m_queue->setPcIp(m_pcIp);
    if (m_shareEnabled)
        m_server->setAnnouncing(true, m_pcIp);
}

bool Application::shareEnabled() const
{
    return m_shareEnabled;
}

QString Application::catalogUrl() const
{
    if (!m_shareEnabled || !m_server)
        return QString();
    return m_server->catalogUrl(m_pcIp);
}

void Application::setShareEnabled(bool on)
{
    if (m_shareEnabled == on || !m_server)
        return;
    m_shareEnabled = on;
    m_server->setAnnouncing(on, m_pcIp);
    emit shareEnabledChanged();
    if (on)
        emit notify(tr("Catalog shared at %1").arg(catalogUrl()));
}

void Application::saveIpSettings()
{
    Database::instance().setSetting(QStringLiteral("psIp"), m_psIp);
    Database::instance().setSetting(QStringLiteral("pcIp"), m_pcIp);
}

void Application::refreshPcAddresses()
{
    QStringList addresses;
    for (const LanNetwork &net : Discovery::lanNetworks())
        addresses.append(net.address.toString());
    if (m_pcAddresses != addresses) {
        m_pcAddresses = addresses;
        emit pcAddressesChanged();
        emit hasNetworkChanged();
    }
    if (!m_pcIp.isEmpty() && m_pcAddresses.contains(m_pcIp))
        return;
    const QString best = Discovery::bestPcIpFor(Discovery::lanNetworks(), m_psIp);
    if (!best.isEmpty())
        setPcIp(best);
}

void Application::pollReceiver()
{
    if (m_psIp.isEmpty()) {
        if (m_receiverOnline) {
            m_receiverOnline = false;
            emit receiverOnlineChanged();
        }
        return;
    }
    m_receiver->status([this](const ReceiverStatus &st) {
        if (!st.online) {
            m_missedPolls++;
            if (m_missedPolls == OfflineAfterMissedPolls && m_queue)
                m_queue->onReceiverOffline();
            if (m_receiverOnline && m_missedPolls >= OfflineAfterMissedPolls) {
                m_receiverOnline = false;
                emit receiverOnlineChanged();
            }
            return;
        }
        if (m_queue)
            m_queue->onReceiverStatus(st);
        const bool wasOnline = m_receiverOnline;
        m_missedPolls = 0;
        m_receiverOnline = true;
        m_receiverBusy = st.busy;
        if (!wasOnline) {
            m_receiver->version([this](bool ok, const QString &build) {
                m_receiverBuild = ok ? build : QString();
                emit receiverOnlineChanged();
            });
            m_receiver->space([this](const ReceiverSpace &sp) {
                m_freeSpaceText = sp.online
                    ? tr("%1 free of %2").arg(formatSize(sp.free)).arg(formatSize(sp.total))
                    : QString();
                emit receiverOnlineChanged();
            });
        }
        emit receiverOnlineChanged();
    });
}

void Application::detectConsoles()
{
    if (m_discovering)
        return;
    m_discovering = true;
    m_discoveryStatus = tr("Starting…");
    emit discoveringChanged();
    m_discovery->findConsoles();
}

void Application::probeConsoles(const QStringList &ips)
{
    m_consoles.clear();
    emit consolesChanged();
    if (ips.isEmpty()) {
        emit notify(tr("No console found — is the receiver ELF running?"));
        return;
    }

    const auto remaining = std::make_shared<int>(ips.size());
    for (const QString &ip : ips) {
        auto *client = new ReceiverClient(this);
        client->setConsoleIp(ip);
        client->status([this, client, ip, remaining](const ReceiverStatus &st) {
            client->deleteLater();
            QVariantMap entry;
            entry.insert(QStringLiteral("ip"), ip);
            entry.insert(QStringLiteral("verified"), st.online);
            entry.insert(QStringLiteral("busy"), st.busy);
            m_consoles.append(entry);
            emit consolesChanged();
            if (--(*remaining) == 0)
                emit notify(tr("%1 console(s) found").arg(m_consoles.size()));
        });
    }
}

QString Application::destination() const
{
    return m_queue ? m_queue->destination() : QStringLiteral("/data/homebrew");
}

void Application::setDestination(const QString &destination)
{
    if (!m_queue)
        return;
    m_queue->setDestination(destination);
    Database::instance().setSetting(QStringLiteral("destination"), destination);
    emit destinationChanged();
}

void Application::sendSelected()
{
    enqueueSelection(false);
}

void Application::copySelectedImages()
{
    enqueueSelection(true);
}

void Application::enqueueSelection(bool copy)
{
    const QList<qlonglong> ids = m_library->selectedIds();
    if (ids.isEmpty())
        return;
    if (m_psIp.isEmpty()) {
        emit notify(tr("Set the console IP first (Detect helps)"));
        return;
    }
    QList<LibraryRow> rows;
    for (const LibraryRow &row : Database::instance().libraryRows()) {
        if (!ids.contains(row.id))
            continue;
        const bool isImage = PkgReader::isImageFile(row.format);
        if (copy == isImage)
            rows.append(row);
    }
    if (rows.isEmpty())
        return;
    m_queue->enqueue(rows, copy);
    emit notify(copy ? tr("%1 image(s) queued for copy").arg(rows.size())
                     : tr("%1 package(s) queued for install").arg(rows.size()));
}

void Application::copyToClipboard(const QString &text)
{
    if (text.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(text);
    emit notify(tr("Copied %1").arg(text));
}

void Application::testConnection()
{
    if (m_psIp.isEmpty()) {
        emit notify(tr("Set the console IP first"));
        return;
    }
    m_receiver->version([this](bool ok, const QString &build) {
        if (ok)
            emit notify(tr("Receiver online (%1)").arg(build.isEmpty()
                ? tr("unknown build") : build));
        else
            emit notify(tr("Receiver not answering on %1").arg(m_psIp));
    });
}

void Application::useConsole(const QString &ip)
{
    setPsIp(ip);
    emit notify(tr("Console set to %1").arg(ip));
}

void Application::addFolder(const QUrl &folder)
{
    const QString path = QDir::toNativeSeparators(folder.toLocalFile());
    if (path.isEmpty() || !QDir(path).exists())
        return;
    Database::instance().addScannedRoot(path);
    startScan({path});
}

void Application::addDroppedUrls(const QList<QUrl> &urls)
{
    QStringList roots;
    QStringList files;
    for (const QUrl &url : urls) {
        const QString local = url.toLocalFile();
        if (local.isEmpty())
            continue;
        const QFileInfo fi(local);
        if (fi.isDir()) {
            Database::instance().addScannedRoot(fi.absoluteFilePath());
            roots.append(fi.absoluteFilePath());
        } else if (fi.isFile()) {
            files.append(fi.absoluteFilePath());
        }
    }
    if (!roots.isEmpty() || !files.isEmpty())
        startScan(roots + files);
}

void Application::rescanAll()
{
    const QStringList roots = Database::instance().scannedRoots();
    if (!roots.isEmpty())
        startScan(roots);
}

void Application::removeEntry(qlonglong id)
{
    m_library->removeEntry(id);
}

void Application::startScan(const QStringList &roots)
{
    if (m_scanning)
        return;
    m_scanning = true;
    m_scanStatus = tr("Scanning folders…");
    emit scanningChanged();
    emit scanStatusChanged();
    m_scanner->scan(roots);
}

QStringList Application::refreshScanPaths()
{
    m_scanPaths = ScanPaths::builtinPaths()
        + Database::instance().customScanPaths();
    emit scanPathsChanged();
    return m_scanPaths;
}

bool Application::destinationIsCustom() const
{
    return !ScanPaths::builtinPaths().contains(m_queue ? m_queue->destination()
                                                       : QString());
}

bool Application::addCustomScanPath(const QString &path)
{
    const QString normalized = ScanPaths::normalize(path);
    if (!ScanPaths::isAbsoluteRemotePath(normalized))
        return false;
    Database::instance().addCustomScanPath(normalized);
    refreshScanPaths();
    setDestination(normalized);
    emit notify(tr("Added %1 to scan paths").arg(normalized));
    return true;
}

void Application::removeCustomScanPath(const QString &path)
{
    Database::instance().removeCustomScanPath(path);
    refreshScanPaths();
    if (destination() == path)
        setDestination(QStringLiteral("/data/homebrew"));
}

void Application::reloadHistory()
{
    m_historyRows = buildHistoryRows();
    emit historyChanged();
}

void Application::clearHistory()
{
    Database::instance().clearHistory();
    m_historyRows = buildHistoryRows();
    emit historyChanged();
}

QVariantList Application::buildHistoryRows() const
{
    QVariantList out;
    for (const HistoryRow &row : Database::instance().historyRows(300)) {
        QVariantMap map;
        map.insert(QStringLiteral("kind"), row.kind);
        map.insert(QStringLiteral("name"), row.name);
        map.insert(QStringLiteral("remotePath"), row.remotePath);
        map.insert(QStringLiteral("sizeText"), formatSize(row.size));
        map.insert(QStringLiteral("ok"), row.ok);
        map.insert(QStringLiteral("message"), row.message);
        map.insert(QStringLiteral("time"),
                   QDateTime::fromSecsSinceEpoch(row.finished).toString(
                       QStringLiteral("yyyy-MM-dd hh:mm")));
        out.append(map);
    }
    return out;
}

void Application::pruneMissing()
{
    QList<qlonglong> stale;
    for (const LibraryRow &row : Database::instance().libraryRows()) {
        if (!QFileInfo::exists(row.path))
            stale.append(row.id);
    }
    if (!stale.isEmpty())
        Database::instance().removeLibraryEntries(stale);
}

}
