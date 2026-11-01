#pragma once

#include <QByteArray>
#include <QString>

namespace ps5 {

struct PkgData {
    QString title;
    QString contentId;
    QString titleId;
    QString version;
    QString platform;
    QString format;
    QString category;
    bool isDlc = false;
    qint64 size = 0;
    QString digest;
    QByteArray icon;
    bool isBackport = false;
};

namespace PkgReader {

PkgData read(const QString &path);
bool isImageFile(const QString &suffix);
QString titleIdFromName(const QString &fileName);

}

}
