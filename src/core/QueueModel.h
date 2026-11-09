#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QObject>

#include "core/ReceiverClient.h"
#include "core/RangeFileServer.h"

namespace ps5 {

enum class TransferState {
    Queued,
    Pushing,
    Downloading,
    Installing,
    Copying,
    Paused,
    Done,
    Skipped,
    Failed,
    Cancelled,
};

struct QueueItem {
    QString id;
    qlonglong libraryId = 0;
    QString name;
    QString localPath;
    QString fileName;
    QString format;
    bool isCopy = false;
    qint64 size = 0;
    TransferState state = TransferState::Queued;
    QString message;
    qint64 sent = 0;
    qint64 started = 0;
    QString serverId;
    QString pushedUrl;
    bool resumeOnRetry = false;
    bool pullWasActive = false;
    bool verifying = false;
    bool hasIcon = false;
    int pullGrace = 0;
};

class QueueModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY countChanged)
    Q_PROPERTY(int failedCount READ failedCount NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        KindRole,
        StateRole,
        StateTextRole,
        MessageRole,
        PercentRole,
        SizeTextRole,
        IconUrlRole,
        CanPauseRole,
        CanCancelRole,
        CanRetryRole,
        IsCopyRole,
    };

    explicit QueueModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_items.size(); }
    int activeCount() const;
    int failedCount() const;

    int indexOf(const QString &id) const;
    QueueItem itemAt(int row) const;
    void setItems(const QList<QueueItem> &items);
    void updateItem(const QueueItem &item);
    void removeAt(int row);
    void move(int from, int to);

signals:
    void countChanged();

private:
    static QString stateText(TransferState state);

    QList<QueueItem> m_items;
};

}
