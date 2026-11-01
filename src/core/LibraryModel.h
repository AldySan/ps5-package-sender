#pragma once

#include <QAbstractListModel>
#include <QList>

#include "db/Database.h"

namespace ps5 {

class LibraryModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY filtersChanged)
    Q_PROPERTY(QString formatFilter READ formatFilter WRITE setFormatFilter NOTIFY filtersChanged)
    Q_PROPERTY(QString familyFilter READ familyFilter WRITE setFamilyFilter NOTIFY filtersChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)
    Q_PROPERTY(int selectedPkgCount READ selectedPkgCount NOTIFY selectedCountChanged)
    Q_PROPERTY(int selectedImageCount READ selectedImageCount
                   NOTIFY selectedCountChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        TitleIdRole,
        VersionRole,
        PlatformRole,
        FormatRole,
        SizeRole,
        SizeTextRole,
        KindRole,
        FamilyKeyRole,
        IconUrlRole,
        PathRole,
        SelectedRole,
    };

    explicit LibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString searchText() const { return m_search; }
    void setSearchText(const QString &text);
    QString formatFilter() const { return m_formatFilter; }
    void setFormatFilter(const QString &filter);
    QString familyFilter() const { return m_familyFilter; }
    void setFamilyFilter(const QString &key);

    int selectedCount() const;
    int selectedPkgCount() const;
    int selectedImageCount() const;
    int count() const { return m_visible.size(); }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void toggleSelected(qlonglong id);
    Q_INVOKABLE void setSelected(qlonglong id, bool selected);
    Q_INVOKABLE void selectVisible(bool selected);
    Q_INVOKABLE QList<qlonglong> selectedIds() const;
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void removeEntry(qlonglong id);

signals:
    void filtersChanged();
    void selectedCountChanged();
    void countChanged();

private:
    struct Item {
        LibraryRow row;
        QString kind;
        QString familyKey;
        bool selected = false;
    };

    void rebuild();
    static bool matches(const Item &item, const QString &search);

    QList<LibraryRow> m_rows;
    QList<Item> m_visible;
    QString m_search;
    QString m_formatFilter;
    QString m_familyFilter;
    QHash<qlonglong, bool> m_selected;
};

}
