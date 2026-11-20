#pragma once

#include <QObject>
#include <QTimer>

#include "core/QueueModel.h"
#include "core/ReceiverClient.h"
#include "core/RangeFileServer.h"
#include "db/Database.h"

namespace ps5 {

class TransferQueue : public QObject {
    Q_OBJECT
    Q_PROPERTY(ps5::QueueModel *model READ model CONSTANT)

public:
    static constexpr qint64 DownloadTolerance = 1024 * 1024;
    // How long a download row may sit without a single byte of progress
    // on an idle console that is not running our install job before the
    // queue gives up on it (see onReceiverStatus).
    static constexpr qint64 DefaultStallFailMs = 5 * 60 * 1000;

    explicit TransferQueue(ReceiverClient *receiver, RangeFileServer *server,
                            QObject *parent = nullptr);

    QueueModel *model() const { return m_model; }

    void setDestination(const QString &destination);
    QString destination() const { return m_destination; }

    // Test hook: shorten the download-stall window instead of waiting
    // five real minutes.
    void setStallFailMs(qint64 ms) { m_stallFailMs = ms; }

    void setPcIp(const QString &pcIp) { m_pcIp = pcIp; }
    QString pcIp() const { return m_pcIp; }

    void enqueue(const QList<LibraryRow> &rows, bool copy);

    Q_INVOKABLE void retryRow(const QString &id);
    Q_INVOKABLE void retryFailed();
    Q_INVOKABLE void removeRow(const QString &id);
    Q_INVOKABLE void cancelRow(const QString &id);
    Q_INVOKABLE void pauseRow(const QString &id);
    Q_INVOKABLE void resumeRow(const QString &id);
    Q_INVOKABLE void moveRow(const QString &id, int delta);
    Q_INVOKABLE void clearFinished();

    void onReceiverStatus(const ReceiverStatus &status);
    void onReceiverOffline();

signals:
    void queueChanged();
    void notify(const QString &message);
    void historyChanged();

private:
    void pump();
    void startInstall(QueueItem &item);
    void startCopy(QueueItem &item);
    void verifyCopy(QueueItem &item);
    void finishItem(QueueItem &item, TransferState state, const QString &message);
    void updateProgress(const QString &id);
    QueueItem *mutableItem(const QString &id);
    void writeHistory(const QueueItem &item, bool ok, const QString &message);
    QString remotePathFor(const QueueItem &item) const;

    ReceiverClient *m_receiver = nullptr;
    RangeFileServer *m_server = nullptr;
    QueueModel *m_model = nullptr;
    QList<QueueItem> m_items;
    QString m_destination = QStringLiteral("/data/homebrew");
    QString m_pcIp;
    QString m_activePushId;
    QString m_activeCopyId;
    bool m_consoleIdle = true;
    qint64 m_stallFailMs = DefaultStallFailMs;
};

}
