#include "ScanPaths.h"

namespace ps5::ScanPaths {

QStringList builtinPaths()
{
    QStringList paths;
    paths << QStringLiteral("/data/homebrew");
    paths << QStringLiteral("/data/etaHEN/games");

    const QStringList extRoms = { QStringLiteral("/mnt/ext0"), QStringLiteral("/mnt/ext1") };
    for (const QString &ext : extRoms) {
        paths << ext + QStringLiteral("/homebrew");
        paths << ext + QStringLiteral("/etaHEN/games");
    }
    for (int usb = 0; usb <= 7; usb++) {
        const QString base = QStringLiteral("/mnt/usb%1").arg(usb);
        paths << base + QStringLiteral("/homebrew");
        paths << base + QStringLiteral("/etaHEN/games");
    }
    for (int usb = 0; usb <= 7; usb++)
        paths << QStringLiteral("/mnt/usb%1").arg(usb);
    paths << QStringLiteral("/mnt/ext0");
    paths << QStringLiteral("/mnt/ext1");
    paths << QStringLiteral("/mnt/shadowmnt/pfsc");
    paths << QStringLiteral("/mnt/shadowmnt");
    return paths;
}

bool isAbsoluteRemotePath(const QString &path)
{
    const QString p = path.trimmed();
    return p.startsWith(QLatin1Char('/')) && !p.contains(QStringLiteral(".."))
        && p.size() > 1;
}

QString normalize(const QString &path)
{
    QString p = path.trimmed();
    while (p.size() > 1 && p.endsWith(QLatin1Char('/')))
        p.chop(1);
    return p;
}

}
