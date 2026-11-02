#include "TransferQueue.h"

#include <QDateTime>
#include <QFileInfo>
#include <QTimer>
#include <QUuid>
#include <QVariant>

#include "util/Format.h"

namespace ps5 {

namespace {

bool terminalState(TransferState state)
{
    switch (state) {
    case TransferState::Done:
    case TransferState::Skipped:
    case TransferState::Failed:
    case TransferState::Cancelled:
        return true;
    default:
        return false;
    }
}

} // namespace

TransferQueue::TransferQueue(ReceiverClient *receiver, RangeFileServer *server,
                             QObject *parent)
    : QObject(parent), m_receiver(receiver), m_server(server)
{
    m_model = new QueueModel(this);

    connect(m_server, &RangeFileServer::bytesServed, this,
            [this](const QString &id, qint64) { updateProgress(id); });
}

void TransferQueue::setDestination(const QString &destination)
{
    const QString trimmed = destination.trimmed();
    if (!trimmed.isEmpty())
        m_destination = trimmed;
}

void TransferQueue::enqueue(const QList<LibraryRow> &rows, bool copy)
{
    for (const LibraryRow &row : rows) {
        QueueItem item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.libraryId = row.id;
        item.name = row.title;
        item.localPath = row.path;
        item.fileName = QFileInfo(row.path).fileName();
        item.format = row.format;
        item.isCopy = copy;
        item.size = row.size;
        item.started = QDateTime::currentSecsSinceEpoch();
        item.hasIcon = row.hasIcon;
        m_items.append(item);
    }
    m_model->setItems(m_items);
    emit queueChanged();
    pump();
}

QueueItem *TransferQueue::mutableItem(const QString &id)
{
    for (QueueItem &item : m_items) {
        if (item.id == id)
            return &item;
    }
    return nullptr;
}

void TransferQueue::pump()
{
    for (QueueItem &item : m_items) {
        if (item.state != TransferState::Queued)
            continue;
        if (item.isCopy) {
            if (!m_activeCopyId.isEmpty())
                return;
            startCopy(item);
            return;
        }
    }
    for (QueueItem &item : m_items) {
        if (item.state != TransferState::Queued || item.isCopy)
            continue;
        if (!m_activePushId.isEmpty())
            return;
        startInstall(item);
        return;
    }
}

void TransferQueue::startInstall(QueueItem &item)
{
    item.state = TransferState::Pushing;
    item.message = tr("pushing to console…");
    item.sent = 0;
    item.serverId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_server->registerFile(item.serverId, item.localPath);
    m_server->resetServed(item.serverId);
    m_activePushId = item.id;
    m_model->updateItem(item);
    emit queueChanged();

    const QString pkgUrl = m_server->pkgUrl(m_pcIp, item.serverId);
    QString iconUrl;
    if (!Database::instance().libraryIcon(item.libraryId).isEmpty())
        iconUrl = m_server->iconUrl(m_pcIp, item.serverId);

    m_receiver->install(pkgUrl, item.name, iconUrl,
                        [this, id = item.id](bool ok, const QString &reply) {
        QueueItem *it = mutableItem(id);
        if (!it)
            return;
        m_activePushId.clear();
        if (terminalState(it->state))
            return;
        if (!ok) {
            finishItem(*it, TransferState::Failed,
                       reply.size() > 80 ? reply.left(80) : reply);
            pump();
            return;
        }
        it->state = TransferState::Downloading;
        it->message = tr("console is downloading…");
        m_model->updateItem(*it);
        emit queueChanged();
        pump();
    });
}

void TransferQueue::startCopy(QueueItem &item)
{
    item.state = TransferState::Pushing;
    item.message = tr("checking console…");
    item.sent = 0;
    item.pullWasActive = false;
    item.pullGrace = 2;
    item.serverId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_server->registerFile(item.serverId, item.localPath);
    m_server->resetServed(item.serverId);
    m_activeCopyId = item.id;
    m_model->updateItem(item);
    emit queueChanged();

    const QString remotePath = remotePathFor(item);

    m_receiver->stat(remotePath,
                     [this, id = item.id, remotePath](bool ok, bool exists, qint64 size) {
        QueueItem *it = mutableItem(id);
        if (!it)
            return;
        if (terminalState(it->state)) {
            m_activeCopyId.clear();
            pump();
            return;
        }
        if (!ok) {
            finishItem(*it, TransferState::Failed, tr("console not reachable"));
            m_activeCopyId.clear();
            pump();
            return;
        }
        if (exists && size > 0 && size == it->size) {
            finishItem(*it, TransferState::Skipped, tr("already on console"));
            m_activeCopyId.clear();
            pump();
            return;
        }
        it->resumeOnRetry = exists && size > 0 && size < it->size;
        it->state = TransferState::Copying;
        it->message = tr("copying…");
        m_model->updateItem(*it);
        emit queueChanged();

        const QString url = m_server->pkgUrl(m_pcIp, it->serverId);
        m_receiver->pull(url, remotePath, it->resumeOnRetry,
                         [this, id](bool ok, const QString &reply) {
            QueueItem *item2 = mutableItem(id);
            if (!item2)
                return;
            if (terminalState(item2->state)) {
                m_activeCopyId.clear();
                pump();
                return;
            }
            if (!ok) {
                finishItem(*item2, TransferState::Failed,
                           reply.size() > 80 ? reply.left(80) : reply);
                m_activeCopyId.clear();
                pump();
            }
        });
    });
}

void TransferQueue::onReceiverStatus(const ReceiverStatus &status)
{
    m_consoleIdle = status.online && !status.busy && status.active == 0;

    for (QueueItem &item : m_items) {
        if (item.isCopy && item.id == m_activeCopyId && !item.verifying) {
            if (status.pull && status.pullName == item.fileName) {
                item.pullWasActive = true;
                item.pullGrace = 3;
                if (status.pullWant > 0)
                    item.sent = status.pullGot;
                item.state = status.pullPaused ? TransferState::Paused
                                               : TransferState::Copying;
                if (status.pullPaused)
                    item.message = tr("paused on console");
                m_model->updateItem(item);
                continue;
            }
            item.pullGrace--;
            if (item.pullGrace <= 0)
                verifyCopy(item);
            continue;
        }

        if (!item.isCopy && item.state == TransferState::Downloading
            && item.size > 0) {
            const qint64 served = m_server->servedFor(item.serverId);
            item.sent = served;
            if (served + DownloadTolerance >= item.size) {
                item.state = TransferState::Installing;
                item.message = tr("installing on console…");
                item.sent = item.size;
                m_model->updateItem(item);
                emit queueChanged();
            } else {
                m_model->updateItem(item);
            }
            continue;
        }

        if (!item.isCopy && item.state == TransferState::Installing && m_consoleIdle) {
            finishItem(item, TransferState::Done, tr("installed"));
        }
    }
}

void TransferQueue::verifyCopy(QueueItem &item)
{
    item.verifying = true;
    const QString remotePath = remotePathFor(item);
    m_receiver->stat(remotePath,
                     [this, id = item.id](bool ok, bool exists, qint64 size) {
        QueueItem *it = mutableItem(id);
        if (!it)
            return;
        it->verifying = false;
        m_activeCopyId.clear();
        if (terminalState(it->state)) {
            pump();
            return;
        }
        if (ok && exists && size == it->size) {
            it->sent = it->size;
            finishItem(*it, TransferState::Done, tr("copied"));
        } else if (ok && exists && size < it->size) {
            it->resumeOnRetry = true;
            finishItem(*it, TransferState::Failed,
                       tr("copy incomplete (%1 of %2)")
                           .arg(formatSize(size))
                           .arg(formatSize(it->size)));
        } else {
            finishItem(*it, TransferState::Failed, tr("copy did not complete"));
        }
        pump();
    });
}

void TransferQueue::onReceiverOffline()
{
    for (QueueItem &item : m_items) {
        if (terminalState(item.state))
            continue;
        if (item.state == TransferState::Queued || item.state == TransferState::Paused
            || (item.isCopy
                && (item.state == TransferState::Copying
                    || item.state == TransferState::Pushing))) {
            item.message = tr("waiting for console…");
            m_model->updateItem(item);
            continue;
        }
        if (!item.isCopy && item.state == TransferState::Downloading) {
            item.message = tr("console went offline — will resume");
            item.state = TransferState::Queued;
            m_activePushId.clear();
            m_server->revoke(item.serverId);
            m_model->updateItem(item);
            emit queueChanged();
        }
    }
}

void TransferQueue::updateProgress(const QString &serverId)
{
    for (QueueItem &item : m_items) {
        if (item.serverId != serverId
            || item.state != TransferState::Downloading)
            continue;
        item.sent = m_server->servedFor(serverId);
        m_model->updateItem(item);
    }
}

void TransferQueue::finishItem(QueueItem &item, TransferState state,
                               const QString &message)
{
    item.state = state;
    item.message = message;
    m_model->updateItem(item);
    emit queueChanged();
    writeHistory(item, state == TransferState::Done || state == TransferState::Skipped,
                 message);
}

void TransferQueue::writeHistory(const QueueItem &item, bool ok, const QString &message)
{
    HistoryRow row;
    row.started = item.started;
    row.finished = QDateTime::currentSecsSinceEpoch();
    row.kind = item.isCopy ? QStringLiteral("copy") : QStringLiteral("install");
    row.name = item.name;
    row.remotePath = item.isCopy ? remotePathFor(item) : QString();
    row.size = item.size;
    row.ok = ok;
    row.message = message;
    Database::instance().addHistory(row);
}

QString TransferQueue::remotePathFor(const QueueItem &item) const
{
    QString base = m_destination;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    return base + QLatin1Char('/') + item.fileName;
}

void TransferQueue::retryRow(const QString &id)
{
    QueueItem *item = mutableItem(id);
    if (!item || !terminalState(item->state))
        return;
    item->state = TransferState::Queued;
    item->message = tr("requeued");
    item->sent = 0;
    m_model->updateItem(*item);
    emit queueChanged();
    pump();
}

void TransferQueue::retryFailed()
{
    int n = 0;
    for (QueueItem &item : m_items) {
        if (item.state == TransferState::Failed
            || item.state == TransferState::Cancelled) {
            item.state = TransferState::Queued;
            item.message = tr("requeued");
            item.sent = 0;
            n++;
            m_model->updateItem(item);
        }
    }
    if (n > 0) {
        emit queueChanged();
        emit notify(tr("%1 item(s) requeued").arg(n));
        pump();
    }
}

void TransferQueue::removeRow(const QString &id)
{
    for (int i = 0; i < m_items.size(); i++) {
        if (m_items.at(i).id != id)
            continue;
        if (!terminalState(m_items.at(i).state)) {
            cancelRow(id);
            return;
        }
        m_items.removeAt(i);
        m_model->removeAt(i);
        emit queueChanged();
        return;
    }
}

void TransferQueue::cancelRow(const QString &id)
{
    QueueItem *item = mutableItem(id);
    if (!item)
        return;
    if (item->isCopy
        && (item->state == TransferState::Copying
            || item->state == TransferState::Paused)) {
        m_receiver->pullCancel([this, id](bool, const QString &) {
            QueueItem *it = mutableItem(id);
            if (!it || terminalState(it->state))
                return;
            m_activeCopyId.clear();
            finishItem(*it, TransferState::Cancelled, tr("cancelled (partial kept)"));
            pump();
        });
        return;
    }
    if (item->isCopy && item->state == TransferState::Pushing) {
        m_activeCopyId.clear();
        finishItem(*item, TransferState::Cancelled, tr("cancelled"));
        pump();
        return;
    }
    if (item->state == TransferState::Queued || item->state == TransferState::Paused) {
        finishItem(*item, TransferState::Cancelled, tr("cancelled"));
        if (m_activePushId == id)
            m_activePushId.clear();
        if (m_activeCopyId == id)
            m_activeCopyId.clear();
        pump();
        return;
    }
    if (!item->isCopy
        && (item->state == TransferState::Downloading
            || item->state == TransferState::Pushing)) {
        m_server->revoke(item->serverId);
        finishItem(*item, TransferState::Cancelled, tr("cancelled"));
        m_activePushId.clear();
        pump();
    }
}

void TransferQueue::pauseRow(const QString &id)
{
    QueueItem *item = mutableItem(id);
    if (!item || !item->isCopy || item->state != TransferState::Copying)
        return;
    m_receiver->pullPause(true, [this, id](bool, const QString &) {
        QueueItem *it = mutableItem(id);
        if (!it)
            return;
        it->state = TransferState::Paused;
        it->message = tr("paused on console");
        m_model->updateItem(*it);
    });
}

void TransferQueue::resumeRow(const QString &id)
{
    QueueItem *item = mutableItem(id);
    if (!item || !item->isCopy)
        return;
    if (item->state == TransferState::Paused) {
        m_receiver->pullPause(false, [this, id](bool, const QString &) {
            QueueItem *it = mutableItem(id);
            if (!it)
                return;
            it->state = TransferState::Copying;
            it->message = tr("copying…");
            m_model->updateItem(*it);
        });
    }
}

void TransferQueue::moveRow(const QString &id, int delta)
{
    const int row = m_model->indexOf(id);
    if (row < 0)
        return;
    const int target = row + delta;
    if (target < 0 || target >= m_items.size())
        return;
    if (terminalState(m_items.at(row).state)
        || terminalState(m_items.at(target).state))
        return;
    m_items.move(row, target);
    m_model->move(row, target);
    emit queueChanged();
}

void TransferQueue::clearFinished()
{
    QList<QueueItem> kept;
    for (const QueueItem &item : m_items) {
        if (terminalState(item.state))
            continue;
        kept.append(item);
    }
    if (kept.size() == m_items.size())
        return;
    m_items = kept;
    m_model->setItems(m_items);
    emit queueChanged();
}

}
