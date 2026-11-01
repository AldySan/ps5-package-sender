#include "IconProvider.h"

#include <QImage>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>

#include "db/Database.h"

namespace ps5 {

namespace {

QByteArray readIcon(qlonglong id)
{
    QByteArray out;
    if (id <= 0)
        return out;
    const QString connectionName = QStringLiteral("icon-%1").arg(
        QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId())));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(Database::instance().databasePath());
        if (db.open()) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral("SELECT icon FROM library WHERE id = ?"));
            q.addBindValue(id);
            if (q.exec() && q.next())
                out = q.value(0).toByteArray();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return out;
}

} // namespace

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size,
                                    const QSize &requestedSize)
{
    const QByteArray bytes = readIcon(id.toLongLong());
    QImage img;
    if (!bytes.isEmpty())
        img.loadFromData(bytes, "PNG");
    if (img.isNull()) {
        QPixmap placeholder(64, 64);
        placeholder.fill(Qt::transparent);
        if (size)
            *size = QSize(64, 64);
        return placeholder;
    }
    if (requestedSize.isValid())
        img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (size)
        *size = img.size();
    return QPixmap::fromImage(img);
}

}
