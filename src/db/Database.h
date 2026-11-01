#pragma once

#include <QByteArray>
#include <QSqlDatabase>
#include <QList>
#include <QPair>
#include <QString>

namespace ps5 {

struct LibraryRow {
    qlonglong id = 0;
    QString path;
    QString title;
    QString titleId;
    QString contentId;
    QString version;
    QString platform;
    QString format;
    bool isDlc = false;
    qint64 size = 0;
    qint64 mtime = 0;
    QString digest;
    bool hasIcon = false;
    QByteArray icon;
};

struct HistoryRow {
    qlonglong id = 0;
    qint64 started = 0;
    qint64 finished = 0;
    QString kind;
    QString name;
    QString remotePath;
    qint64 size = 0;
    bool ok = false;
    QString message;
};

class Database {
public:
    static Database &instance();

    static qlonglong upsertLibraryEntryOn(const QSqlDatabase &db, const LibraryRow &row);

    bool open();
    QString databasePath() const { return m_path; }

    QString setting(const QString &key, const QString &fallback = {});
    void setSetting(const QString &key, const QString &value);

    qlonglong upsertLibraryEntry(const LibraryRow &row);
    void removeLibraryEntries(const QList<qlonglong> &ids);
    QList<LibraryRow> libraryRows();
    QByteArray libraryIcon(qlonglong id);

    void addScannedRoot(const QString &path);
    void removeScannedRoot(const QString &path);
    QStringList scannedRoots();

    void addCustomScanPath(const QString &path);
    void removeCustomScanPath(const QString &path);
    QStringList customScanPaths();

    void addHistory(const HistoryRow &row);
    QList<HistoryRow> historyRows(int limit = 500);
    void clearHistory();

private:
    QString m_path;
};

}
