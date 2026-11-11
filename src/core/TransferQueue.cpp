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

bool localFileMissing(const QueueItem &item)
{
    return !QFileInfo::exists(item.localPath);
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
    const int firstNew = m_items.size();
    int missing = 0;
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
        if (localFileMissing(item))
            missing++;
        m_items.append(item);
    }
    m_model->setItems(m_items);
    emit queueChanged();
    // A file deleted after it was scanned would otherwise push a dead URL to
    // the console and fail minutes later with a cryptic install error — fail
    // the row up front, in the queue, with a message that names the problem.
    for (int i = firstNew; i < m_items.size(); i++) {
        if (localFileMissing(m_items[i]))
            finishItem(m_items[i], TransferState::Failed,
                       tr("file no longer exists on the PC"));
    }
    if (missing > 0)
        emit notify(tr("Skipped %1 item(s): the file(s) no longer exist on the PC")
                        .arg(missing));
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
    if (localFileMissing(item)) {
        // Covers the retry path: the file was deleted while the item sat in
        // the queue (or after a failure).
        finishItem(item, TransferState::Failed,
                   tr("file no longer exists on the PC"));
        pump();
        return;
    }
    item.state = TransferState::Pushing;
    item.message = tr("pushing to console…");
    item.sent = 0;
    item.staged = false;
    item.serverId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_server->registerFile(item.serverId, item.localPath);
    m_server->resetServed(item.serverId);
    m_activePushId = item.id;
    m_model->updateItem(item);
    emit queueChanged();

    const QString pkgUrl = m_server->pkgUrl(m_pcIp, item.serverId);
    // Remember the exact URL we pushed: when the console later reports the
    // outcome of the install it ran, the queue matches it against this.
    item.pushedUrl = pkgUrl;
    QString iconUrl;
    if (item.hasIcon)
        iconUrl = m_server->iconUrl(m_pcIp, item.serverId);

    // Pre-flight space check: an install keeps two copies of the package on
    // the console at once — its own download plus the staged copy the
    // receiver keeps when the console makes it re-serve the file locally —
    // and both the console (0x80B21104) and the receiver's own gate refuse
    // below that. The push still goes out (the console is the authority);
    // this only says it now, not after a multi-gigabyte transfer.
    m_receiver->space([this, id = item.id, size = item.size](const ReceiverSpace &space) {
        QueueItem *it = mutableItem(id);
        if (!it || terminalState(it->state))
            return;
        if (!space.online || space.free < 0 || size <= 0)
            return;
        if (space.free < 2 * size)
            emit notify(tr("%1: only %2 free on the console. An install "
                           "keeps two copies at once and needs about %3, "
                           "so it may be refused")
                            .arg(it->name, formatSize(space.free),
                                 formatSize(2 * size)));
    });

    m_receiver->install(pkgUrl, item.name, iconUrl,
                        [this, id = item.id](bool ok, const QString &reply) {
        QueueItem *it = mutableItem(id);
        if (!it)
            return;
        // The item may have been cancelled while this reply was in flight and
        // another push may already own the slot; only release what is ours.
        if (m_activePushId == id)
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
    if (localFileMissing(item)) {
        finishItem(item, TransferState::Failed,
                   tr("file no longer exists on the PC"));
        pump();
        return;
    }
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
            if (m_activeCopyId == id)
                m_activeCopyId.clear();
            pump();
            return;
        }
        if (!ok) {
            finishItem(*it, TransferState::Failed, tr("console not reachable"));
            if (m_activeCopyId == id)
                m_activeCopyId.clear();
            pump();
            return;
        }
        if (exists && size > 0 && size == it->size) {
            finishItem(*it, TransferState::Skipped, tr("already on console"));
            if (m_activeCopyId == id)
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
                if (m_activeCopyId == id)
                    m_activeCopyId.clear();
                pump();
                return;
            }
            if (!ok) {
                finishItem(*item2, TransferState::Failed,
                           reply.size() > 80 ? reply.left(80) : reply);
                if (m_activeCopyId == id)
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

        // The console stopped while this row was still counting download
        // bytes: the receiver is idle and its last-install report is about
        // the exact URL this row pushed. Either the console's downloader
        // gave up part-way (a space or network error, in its own words) or
        // it finished the install from fewer bytes than we saw served (a
        // resumed download). Both used to leave the row in "console is
        // downloading…" forever with the answer one poll away.
        if (!item.isCopy && item.state == TransferState::Downloading
            && m_consoleIdle && status.hasLast
            && status.lastUrl == item.pushedUrl) {
            if (status.lastOk) {
                // The console took the whole package, so a staged delivery
                // (two copies) is complete: the finished bar reads full.
                if (item.staged)
                    item.sent = 2 * item.size;
                finishItem(item, TransferState::Done, tr("installed"));
            } else {
                finishItem(item, TransferState::Failed,
                           status.lastErr.isEmpty()
                               ? tr("install failed on the console")
                               : tr("console: %1").arg(status.lastErr));
            }
            continue;
        }

        if (!item.isCopy && item.state == TransferState::Downloading
            && item.size > 0) {
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            // The receiver reports which leg of OUR install is moving: a
            // staged install delivers the package twice on the console
            // (it stages the file, then the console's own downloader
            // pulls it back from the receiver's loopback server). Our
            // served counter can only see the staging leg — the whole
            // console-side copy used to be invisible and the row sat at a
            // dead full bar for as long as the download took. Count both
            // legs against twice the size so one bar keeps growing from
            // the first byte served to the last byte the console copies.
            const bool jobOurs = status.instPhase > 0
                && status.instUrl == item.pushedUrl;
            if (jobOurs && status.instPhase == 2) {
                item.staged = true;
                item.sent = qBound<qint64>(0, status.instGot, item.size);
                item.progressMs = nowMs;
                item.message = tr("console is preparing the download…");
                m_model->updateItem(item);
            } else if (jobOurs && status.instPhase == 3) {
                item.staged = true;
                item.sent = item.size
                    + qBound<qint64>(0, status.instGot, item.size);
                item.progressMs = nowMs;
                item.message = tr("console is downloading…");
                m_model->updateItem(item);
            } else if (!item.staged) {
                const qint64 served = m_server->servedFor(item.serverId);
                const qint64 previous = item.sent;
                item.sent = served;
                if (item.sent != previous || item.progressMs == 0)
                    item.progressMs = nowMs;
                if (served + DownloadTolerance >= item.size) {
                    item.state = TransferState::Installing;
                    item.message = tr("installing on console…");
                    item.sent = item.size;
                    m_model->updateItem(item);
                    emit queueChanged();
                } else {
                    m_model->updateItem(item);
                }
            } else {
                // Between the receiver's phase reports the bytes hold
                // still; the row already tells the truth about both legs.
                m_model->updateItem(item);
            }
            // A receiver that restarted between two polls comes back
            // online idle, with no job, no report and no bytes ever
            // coming: nothing else in this loop will move or explain the
            // row again. Once the download has been silent past the
            // stall window on an idle console that is not running our
            // job, fail it with a reason instead of leaving
            // "console is downloading…" on screen forever.
            if (m_consoleIdle && !jobOurs && item.progressMs > 0
                && nowMs - item.progressMs >= m_stallFailMs) {
                finishItem(item, TransferState::Failed,
                           tr("the console stopped downloading without "
                              "reporting an error (the receiver may have "
                              "restarted); send it again"));
            }
            continue;
        }

        if (!item.isCopy && item.state == TransferState::Installing && m_consoleIdle) {
            // An idle console alone proved nothing: a failed install also
            // ends idle, and both outcomes used to be reported as
            // "installed". The receiver publishes the result of the last
            // install it ran together with the exact URL it queued; when
            // that one is ours, fail the row with the console's own words
            // instead of claiming success.
            if (status.hasLast && status.lastUrl == item.pushedUrl
                && !status.lastOk) {
                finishItem(item, TransferState::Failed,
                           status.lastErr.isEmpty()
                               ? tr("install failed on the console")
                               : tr("console: %1").arg(status.lastErr));
            } else if (status.hasLast && status.lastUrl == item.pushedUrl) {
                finishItem(item, TransferState::Done, tr("installed"));
            } else {
                // Idle, but no report for this URL: the receiver restarted
                // mid-install (its report lives in memory) or another
                // job's report already replaced ours before we polled.
                // Claiming a confirmed install here is how a dead install
                // masquerades as a win, so say what actually happened.
                finishItem(item, TransferState::Done,
                           tr("installed (outcome unreported)"));
            }
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
        if (m_activeCopyId == id)
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
            item.message = tr("console went offline (will resume)");
            item.state = TransferState::Queued;
            // Another push may already be in flight; leave its marker alone.
            if (m_activePushId == item.id)
                m_activePushId.clear();
            m_server->forget(item.serverId);
            m_model->updateItem(item);
            emit queueChanged();
        }
    }
}

void TransferQueue::updateProgress(const QString &serverId)
{
    for (QueueItem &item : m_items) {
        if (item.serverId != serverId
            || item.state != TransferState::Downloading
            || item.staged)
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
    // Terminal items never serve bytes again — drop the server registration
    // (and its served counter) so they do not pile up for the whole session.
    if (m_server && !item.serverId.isEmpty())
        m_server->forget(item.serverId);
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
    if (!Database::instance().addHistory(row))
        emit notify(tr("Could not save this transfer to the history"));
    emit historyChanged();
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
            // Only release what is ours — another copy may already run.
            if (m_activeCopyId == id)
                m_activeCopyId.clear();
            finishItem(*it, TransferState::Cancelled, tr("cancelled (partial kept)"));
            pump();
        });
        return;
    }
    if (item->isCopy && item->state == TransferState::Pushing) {
        if (m_activeCopyId == id)
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
        if (m_activePushId == id)
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
        // A late reply must not resurrect an item cancelled while the
        // request was in flight.
        if (!it || terminalState(it->state))
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
            // A late reply must not resurrect an item cancelled while the
            // request was in flight.
            if (!it || terminalState(it->state))
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
