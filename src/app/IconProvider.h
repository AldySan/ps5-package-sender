#pragma once

#include <QQuickImageProvider>

namespace ps5 {

class IconProvider : public QQuickImageProvider {
public:
    IconProvider();

    QPixmap requestPixmap(const QString &id, QSize *size,
                          const QSize &requestedSize) override;
};

}
