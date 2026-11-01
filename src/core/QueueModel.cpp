#include "QueueModel.h"

#include <QDateTime>
#include <QFileInfo>
#include <QUrl>
#include <QVariant>

#include "util/Format.h"

namespace ps5 {

QueueModel::QueueModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int QueueModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant QueueModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_items.size())
        return {};
    const QueueItem &item = m_items.at(index.row());
    switch (role) {
    case IdRole:
        return item.id;
    case NameRole:
        return item.name;
    case KindRole:
        return item.isCopy ? QStringLiteral("copy") : QStringLiteral("install");
    case StateRole:
        return int(item.state);
    case StateTextRole:
        return stateText(item.state);
    case MessageRole:
        return item.message;
    case PercentRole: {
        if (item.size <= 0)
            return 100.0;
        return qMin(100.0, double(item.sent) * 100.0 / double(item.size));
    }
    case SizeTextRole:
        return formatSize(item.size);
    case SpeedTextRole:
    case EtaTextRole:
        return QString();
    case IconUrlRole:
        return item.hasIcon
            ? QStringLiteral("image://pkgicon/%1").arg(item.libraryId)
            : QString();
    case CanPauseRole:
        return item.isCopy && item.state == TransferState::Copying;
    case CanCancelRole:
        return (item.isCopy && item.state == TransferState::Copying)
            || item.state == TransferState::Queued
            || item.state == TransferState::Paused
            || item.state == TransferState::Pushing
            || item.state == TransferState::Downloading;
    case CanRetryRole:
        return item.state == TransferState::Failed
            || item.state == TransferState::Cancelled;
    case IsCopyRole:
        return item.isCopy;
    }
    return {};
}

QHash<int, QByteArray> QueueModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {KindRole, "kind"},
        {StateRole, "state"},
        {StateTextRole, "stateText"},
        {MessageRole, "message"},
        {PercentRole, "percent"},
        {SizeTextRole, "sizeText"},
        {SpeedTextRole, "speedText"},
        {EtaTextRole, "etaText"},
        {IconUrlRole, "iconUrl"},
        {CanPauseRole, "canPause"},
        {CanCancelRole, "canCancel"},
        {CanRetryRole, "canRetry"},
        {IsCopyRole, "isCopy"},
    };
}

int QueueModel::activeCount() const
{
    int n = 0;
    for (const QueueItem &item : m_items) {
        switch (item.state) {
        case TransferState::Queued:
        case TransferState::Pushing:
        case TransferState::Downloading:
        case TransferState::Installing:
        case TransferState::Copying:
        case TransferState::Paused:
            n++;
            break;
        default:
            break;
        }
    }
    return n;
}

int QueueModel::failedCount() const
{
    int n = 0;
    for (const QueueItem &item : m_items) {
        if (item.state == TransferState::Failed
            || item.state == TransferState::Cancelled)
            n++;
    }
    return n;
}

int QueueModel::indexOf(const QString &id) const
{
    for (int i = 0; i < m_items.size(); i++)
        if (m_items.at(i).id == id)
            return i;
    return -1;
}

QueueItem QueueModel::itemAt(int row) const
{
    return m_items.value(row);
}

void QueueModel::setItems(const QList<QueueItem> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
    emit countChanged();
}

void QueueModel::updateItem(const QueueItem &item)
{
    const int row = indexOf(item.id);
    if (row < 0)
        return;
    m_items[row] = item;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
    emit countChanged();
}

void QueueModel::removeAt(int row)
{
    if (row < 0 || row >= m_items.size())
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_items.removeAt(row);
    endRemoveRows();
    emit countChanged();
}

void QueueModel::move(int from, int to)
{
    if (from < 0 || from >= m_items.size() || to < 0 || to >= m_items.size()
        || from == to)
        return;
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to);
    m_items.move(from, to);
    endMoveRows();
    emit countChanged();
}

QString QueueModel::stateText(TransferState state)
{
    switch (state) {
    case TransferState::Queued:
        return QObject::tr("queued");
    case TransferState::Pushing:
        return QObject::tr("pushing");
    case TransferState::Downloading:
        return QObject::tr("downloading");
    case TransferState::Installing:
        return QObject::tr("installing");
    case TransferState::Copying:
        return QObject::tr("copying");
    case TransferState::Paused:
        return QObject::tr("paused");
    case TransferState::Done:
        return QObject::tr("done");
    case TransferState::Skipped:
        return QObject::tr("skipped");
    case TransferState::Failed:
        return QObject::tr("failed");
    case TransferState::Cancelled:
        return QObject::tr("cancelled");
    }
    return QString();
}

}
