#include "LibraryScanner.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtConcurrent/QtConcurrentRun>

#include "core/PkgInfo.h"
#include "db/Database.h"

namespace ps5 {

void LibraryScanner::scan(const QStringList &roots)
{
    if (m_running)
        return;
    m_running = true;

    QHash<QString, QPair<qint64, qint64>> known;
    for (const LibraryRow &r : Database::instance().libraryRows())
        known.insert(r.path, qMakePair(r.mtime, r.size));

    (void)QtConcurrent::run([this, roots, known] {
        const QString connectionName = QStringLiteral("libraryscan");
        QString openError;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), connectionName);
            db.setDatabaseName(Database::instance().databasePath());
            if (db.open()) {
                QSqlQuery pragma(db);
                pragma.exec(QStringLiteral("PRAGMA busy_timeout=5000"));
                int found = 0;
                int parsed = 0;
                const auto processFile = [&](const QFileInfo &fi) {
                    if (!PkgReader::isLibraryFile(fi.suffix()))
                        return;
                    found++;
                    const auto hit = known.constFind(fi.absoluteFilePath());
                    if (hit != known.constEnd()
                        && hit->first == fi.lastModified().toSecsSinceEpoch()
                        && hit->second == fi.size()) {
                        return;
                    }
                    LibraryRow row;
                    row.path = fi.absoluteFilePath();
                    row.mtime = fi.lastModified().toSecsSinceEpoch();
                    const PkgData data = PkgReader::read(fi.absoluteFilePath());
                    row.title = data.title;
                    row.titleId = data.titleId;
                    row.contentId = data.contentId;
                    row.version = data.version;
                    row.platform = data.platform;
                    row.format = data.format;
                    row.isDlc = data.isDlc;
                    row.size = data.size;
                    row.digest = data.digest;
                    row.icon = data.icon;
                    if (row.title.isEmpty())
                        row.title = fi.completeBaseName();
                    Database::upsertLibraryEntryOn(db, row);
                    parsed++;
                    emit progress(found, parsed);
                };
                for (const QString &root : roots) {
                    const QFileInfo info(root);
                    if (info.isFile()) {
                        processFile(info);
                        continue;
                    }
                    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
                    while (it.hasNext())
                        processFile(it.nextFileInfo());
                }
            } else {
                // A scan that cannot write anything must not masquerade as a
                // normal, empty "complete" scan — surface the failure.
                openError = db.lastError().text();
                if (openError.isEmpty())
                    openError = QStringLiteral("could not open the database");
            }
        }
        QSqlDatabase::removeDatabase(connectionName);
        if (!openError.isEmpty())
            emit failed(openError);
        emit finished();
        QMetaObject::invokeMethod(this, [this] { m_running = false; },
                                  Qt::QueuedConnection);
    });
}

}
