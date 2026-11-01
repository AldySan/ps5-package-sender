#include "LibraryModel.h"

#include <QUrl>
#include <QVariant>

#include "core/PkgInfo.h"
#include "util/Format.h"

namespace ps5 {

LibraryModel::LibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visible.size();
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_visible.size())
        return {};
    const Item &item = m_visible.at(index.row());
    switch (role) {
    case IdRole:
        return item.row.id;
    case TitleRole:
        return item.row.title;
    case TitleIdRole:
        return item.row.titleId;
    case VersionRole:
        return item.row.version;
    case PlatformRole:
        return item.row.platform;
    case FormatRole:
        return item.row.format;
    case SizeRole:
        return item.row.size;
    case SizeTextRole:
        return formatSize(item.row.size);
    case KindRole:
        return item.kind;
    case FamilyKeyRole:
        return item.familyKey;
    case IconUrlRole:
        return item.row.icon.isEmpty()
            ? QString()
            : QStringLiteral("image://pkgicon/%1").arg(item.row.id);
    case PathRole:
        return item.row.path;
    case SelectedRole:
        return item.selected;
    }
    return {};
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {TitleIdRole, "titleId"},
        {VersionRole, "version"},
        {PlatformRole, "platform"},
        {FormatRole, "format"},
        {SizeRole, "size"},
        {SizeTextRole, "sizeText"},
        {KindRole, "kind"},
        {FamilyKeyRole, "familyKey"},
        {IconUrlRole, "iconUrl"},
        {PathRole, "path"},
        {SelectedRole, "selected"},
    };
}

void LibraryModel::setSearchText(const QString &text)
{
    if (m_search == text)
        return;
    m_search = text;
    rebuild();
    emit filtersChanged();
}

void LibraryModel::setFormatFilter(const QString &filter)
{
    if (m_formatFilter == filter)
        return;
    m_formatFilter = filter;
    rebuild();
    emit filtersChanged();
}

void LibraryModel::setFamilyFilter(const QString &key)
{
    if (m_familyFilter == key)
        return;
    m_familyFilter = key;
    rebuild();
    emit filtersChanged();
}

void LibraryModel::reload()
{
    m_rows = Database::instance().libraryRows();
    rebuild();
}

void LibraryModel::rebuild()
{
    beginResetModel();
    m_visible.clear();
    for (const LibraryRow &row : std::as_const(m_rows)) {
        Item item;
        item.row = row;
        item.kind = PkgReader::isImageFile(row.format)
            ? QStringLiteral("Image")
            : (row.isDlc ? QStringLiteral("DLC") : QStringLiteral("Game"));
        item.familyKey = row.titleId.isEmpty() ? row.path : row.titleId;
        item.selected = m_selected.value(row.id, false);
        if (!m_familyFilter.isEmpty() && item.familyKey != m_familyFilter)
            continue;
        if (!m_formatFilter.isEmpty() && item.kind != m_formatFilter)
            continue;
        if (!matches(item, m_search))
            continue;
        m_visible.append(item);
    }
    endResetModel();
    emit countChanged();
    emit selectedCountChanged();
}

bool LibraryModel::matches(const Item &item, const QString &search)
{
    if (search.isEmpty())
        return true;
    return item.row.title.contains(search, Qt::CaseInsensitive)
        || item.row.titleId.contains(search, Qt::CaseInsensitive);
}

int LibraryModel::selectedCount() const
{
    int n = 0;
    for (const Item &item : m_visible)
        if (item.selected)
            n++;
    return n;
}

void LibraryModel::toggleSelected(qlonglong id)
{
    setSelected(id, !m_selected.value(id, false));
}

void LibraryModel::setSelected(qlonglong id, bool selected)
{
    m_selected.insert(id, selected);
    for (int i = 0; i < m_visible.size(); i++) {
        if (m_visible[i].row.id == id) {
            m_visible[i].selected = selected;
            const QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {SelectedRole});
            break;
        }
    }
    emit selectedCountChanged();
}

void LibraryModel::selectVisible(bool selected)
{
    for (const Item &item : m_visible)
        m_selected.insert(item.row.id, selected);
    if (!m_visible.isEmpty()) {
        const QModelIndex first = index(0);
        const QModelIndex last = index(m_visible.size() - 1);
        for (Item &item : m_visible)
            item.selected = selected;
        emit dataChanged(first, last, {SelectedRole});
    }
    emit selectedCountChanged();
}

QList<qlonglong> LibraryModel::selectedIds() const
{
    QList<qlonglong> out;
    for (const Item &item : m_visible)
        if (item.selected)
            out.append(item.row.id);
    return out;
}

void LibraryModel::clearSelection()
{
    m_selected.clear();
    if (!m_visible.isEmpty()) {
        for (Item &item : m_visible)
            item.selected = false;
        const QModelIndex first = index(0);
        const QModelIndex last = index(m_visible.size() - 1);
        emit dataChanged(first, last, {SelectedRole});
    }
    emit selectedCountChanged();
}

void LibraryModel::removeEntry(qlonglong id)
{
    Database::instance().removeLibraryEntries({id});
    reload();
}

}
