#include "Database.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QVariant>

namespace ps5 {

namespace {

QString resolveDataDir()
{
    const QString overridden = qEnvironmentVariable("PS5PKGSENDER_DATA_DIR");
    if (!overridden.isEmpty()) {
        QDir().mkpath(overridden);
        return overridden;
    }
    const QString exeDir = QCoreApplication::applicationDirPath();
    QDir portable(exeDir);
    if (portable.mkpath(QStringLiteral("data"))) {
        const QString probe = portable.filePath(QStringLiteral("data/.write-probe"));
        QTemporaryFile f(probe);
        if (f.open())
            return portable.filePath(QStringLiteral("data"));
    }
    const QString fallback =
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath(QStringLiteral("ps5-package-sender"));
    QDir().mkpath(fallback);
    return fallback;
}

} // namespace

Database &Database::instance()
{
    static Database db;
    return db;
}

bool Database::open()
{
    const QString dir = resolveDataDir();
    m_path = QDir(dir).filePath(QStringLiteral("store.db"));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(m_path);
    if (!db.open())
        return false;

    QSqlQuery q;
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS settings ("
                          "key TEXT PRIMARY KEY, value TEXT)"));
    q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS library ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "path TEXT UNIQUE NOT NULL,"
                          "title TEXT, title_id TEXT, content_id TEXT, version TEXT,"
                          "platform TEXT, format TEXT, is_dlc INTEGER DEFAULT 0,"
                          "size INTEGER DEFAULT 0, mtime INTEGER DEFAULT 0,"
                          "digest TEXT, icon BLOB)"));
    q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS scanned_roots ("
                          "path TEXT PRIMARY KEY)"));
    q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS custom_scan_paths ("
                          "path TEXT PRIMARY KEY)"));
    q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS history ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "started INTEGER, finished INTEGER, kind TEXT, name TEXT,"
                          "remote_path TEXT, size INTEGER, ok INTEGER, message TEXT)"));
    return true;
}

QString Database::setting(const QString &key, const QString &fallback)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    q.addBindValue(key);
    if (!q.exec() || !q.next())
        return fallback;
    const QVariant v = q.value(0);
    return v.isNull() ? fallback : v.toString();
}

void Database::setSetting(const QString &key, const QString &value)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO settings (key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    q.addBindValue(key);
    q.addBindValue(value);
    q.exec();
}

qlonglong Database::upsertLibraryEntryOn(const QSqlDatabase &db, const LibraryRow &row)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO library (path, title, title_id, content_id, version, platform,"
        " format, is_dlc, size, mtime, digest, icon)"
        " VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"
        " ON CONFLICT(path) DO UPDATE SET"
        " title=excluded.title, title_id=excluded.title_id,"
        " content_id=excluded.content_id, version=excluded.version,"
        " platform=excluded.platform, format=excluded.format,"
        " is_dlc=excluded.is_dlc, size=excluded.size, mtime=excluded.mtime,"
        " digest=excluded.digest, icon=excluded.icon,"
        " id=id RETURNING id"));
    q.addBindValue(row.path);
    q.addBindValue(row.title);
    q.addBindValue(row.titleId);
    q.addBindValue(row.contentId);
    q.addBindValue(row.version);
    q.addBindValue(row.platform);
    q.addBindValue(row.format);
    q.addBindValue(row.isDlc ? 1 : 0);
    q.addBindValue(row.size);
    q.addBindValue(row.mtime);
    q.addBindValue(row.digest);
    q.addBindValue(row.icon);
    if (q.exec() && q.next())
        return q.value(0).toLongLong();

    QSqlQuery sel(db);
    sel.prepare(QStringLiteral("SELECT id FROM library WHERE path = ?"));
    sel.addBindValue(row.path);
    if (sel.exec() && sel.next())
        return sel.value(0).toLongLong();
    return 0;
}

qlonglong Database::upsertLibraryEntry(const LibraryRow &row)
{
    return upsertLibraryEntryOn(QSqlDatabase::database(), row);
}

void Database::removeLibraryEntries(const QList<qlonglong> &ids)
{
    for (qlonglong id : ids) {
        QSqlQuery q;
        q.prepare(QStringLiteral("DELETE FROM library WHERE id = ?"));
        q.addBindValue(id);
        q.exec();
    }
}

QList<LibraryRow> Database::libraryRows()
{
    QList<LibraryRow> out;
    QSqlQuery q;
    q.exec(QStringLiteral(
        "SELECT id, path, title, title_id, content_id, version, platform, format,"
        " is_dlc, size, mtime, digest, icon FROM library ORDER BY title COLLATE NOCASE"));
    while (q.next()) {
        LibraryRow r;
        r.id = q.value(0).toLongLong();
        r.path = q.value(1).toString();
        r.title = q.value(2).toString();
        r.titleId = q.value(3).toString();
        r.contentId = q.value(4).toString();
        r.version = q.value(5).toString();
        r.platform = q.value(6).toString();
        r.format = q.value(7).toString();
        r.isDlc = q.value(8).toInt() != 0;
        r.size = q.value(9).toLongLong();
        r.mtime = q.value(10).toLongLong();
        r.digest = q.value(11).toString();
        r.icon = q.value(12).toByteArray();
        out.append(r);
    }
    return out;
}

QByteArray Database::libraryIcon(qlonglong id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT icon FROM library WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return q.value(0).toByteArray();
    return {};
}

void Database::addScannedRoot(const QString &path)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO scanned_roots (path) VALUES (?)"));
    q.addBindValue(path);
    q.exec();
}

void Database::removeScannedRoot(const QString &path)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM scanned_roots WHERE path = ?"));
    q.addBindValue(path);
    q.exec();
}

QStringList Database::scannedRoots()
{
    QStringList out;
    QSqlQuery q;
    q.exec(QStringLiteral("SELECT path FROM scanned_roots"));
    while (q.next())
        out.append(q.value(0).toString());
    return out;
}

void Database::addCustomScanPath(const QString &path)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("INSERT OR IGNORE INTO custom_scan_paths (path) VALUES (?)"));
    q.addBindValue(path);
    q.exec();
}

void Database::removeCustomScanPath(const QString &path)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM custom_scan_paths WHERE path = ?"));
    q.addBindValue(path);
    q.exec();
}

QStringList Database::customScanPaths()
{
    QStringList out;
    QSqlQuery q;
    q.exec(QStringLiteral("SELECT path FROM custom_scan_paths ORDER BY path"));
    while (q.next())
        out.append(q.value(0).toString());
    return out;
}

void Database::addHistory(const HistoryRow &row)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO history (started, finished, kind, name, remote_path, size, ok, message)"
        " VALUES (?,?,?,?,?,?,?,?)"));
    q.addBindValue(row.started);
    q.addBindValue(row.finished);
    q.addBindValue(row.kind);
    q.addBindValue(row.name);
    q.addBindValue(row.remotePath);
    q.addBindValue(row.size);
    q.addBindValue(row.ok ? 1 : 0);
    q.addBindValue(row.message);
    q.exec();
}

QList<HistoryRow> Database::historyRows(int limit)
{
    QList<HistoryRow> out;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT id, started, finished, kind, name, remote_path, size, ok, message"
        " FROM history ORDER BY id DESC LIMIT ?"));
    q.addBindValue(limit);
    q.exec();
    while (q.next()) {
        HistoryRow r;
        r.id = q.value(0).toLongLong();
        r.started = q.value(1).toLongLong();
        r.finished = q.value(2).toLongLong();
        r.kind = q.value(3).toString();
        r.name = q.value(4).toString();
        r.remotePath = q.value(5).toString();
        r.size = q.value(6).toLongLong();
        r.ok = q.value(7).toInt() != 0;
        r.message = q.value(8).toString();
        out.append(r);
    }
    return out;
}

void Database::clearHistory()
{
    QSqlQuery q;
    q.exec(QStringLiteral("DELETE FROM history"));
}

}
