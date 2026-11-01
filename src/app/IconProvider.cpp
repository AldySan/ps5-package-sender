#include "IconProvider.h"

#include <QImage>

#include "db/Database.h"

namespace ps5 {

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size,
                                    const QSize &requestedSize)
{
    const QByteArray bytes = Database::instance().libraryIcon(id.toLongLong());
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
