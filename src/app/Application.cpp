#include "Application.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

#include "db/Database.h"

namespace ps5 {

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

    if (!Database::instance().scannedRoots().isEmpty())
        startScan(Database::instance().scannedRoots(), false);
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

void Application::addFolder(const QUrl &folder)
{
    const QString path = QDir::toNativeSeparators(folder.toLocalFile());
    if (path.isEmpty() || !QDir(path).exists())
        return;
    Database::instance().addScannedRoot(path);
    startScan({path}, false);
}

void Application::addDroppedUrls(const QList<QUrl> &urls)
{
    QStringList roots;
    for (const QUrl &url : urls) {
        const QString local = url.toLocalFile();
        if (local.isEmpty())
            continue;
        const QFileInfo fi(local);
        if (fi.isDir()) {
            Database::instance().addScannedRoot(fi.absoluteFilePath());
            roots.append(fi.absoluteFilePath());
        } else if (fi.isFile()) {
            QDir dir = fi.absoluteDir();
            const QString dirPath = dir.absolutePath();
            Database::instance().addScannedRoot(dirPath);
            if (!roots.contains(dirPath))
                roots.append(dirPath);
        }
    }
    if (!roots.isEmpty())
        startScan(roots, false);
}

void Application::rescanAll()
{
    const QStringList roots = Database::instance().scannedRoots();
    if (!roots.isEmpty())
        startScan(roots, false);
}

void Application::removeEntry(qlonglong id)
{
    m_library->removeEntry(id);
}

void Application::startScan(const QStringList &roots, bool rememberRoots)
{
    if (m_scanning)
        return;
    Q_UNUSED(rememberRoots);
    m_scanning = true;
    m_scanStatus = tr("Scanning folders…");
    emit scanningChanged();
    emit scanStatusChanged();
    m_scanner->scan(roots);
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
