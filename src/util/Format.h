#pragma once

#include <QString>

namespace ps5 {

inline QString formatSize(qint64 bytes)
{
    constexpr double KB = 1024.0, MB = 1024.0 * 1024, GB = 1024.0 * 1024 * 1024;
    if (bytes >= qint64(GB))
        return QString::number(bytes / GB, 'f', 2) + QStringLiteral(" GB");
    if (bytes >= qint64(MB))
        return QString::number(bytes / MB, 'f', 1) + QStringLiteral(" MB");
    if (bytes >= qint64(KB))
        return QString::number(bytes / KB, 'f', 0) + QStringLiteral(" KB");
    return QString::number(bytes) + QStringLiteral(" B");
}

inline QString formatSpeed(double bytesPerSec)
{
    constexpr double MB = 1024.0 * 1024, GB = 1024.0 * 1024 * 1024;
    if (bytesPerSec >= GB)
        return QString::number(bytesPerSec / GB, 'f', 1) + QStringLiteral(" GB/s");
    if (bytesPerSec >= MB)
        return QString::number(bytesPerSec / MB, 'f', 1) + QStringLiteral(" MB/s");
    return QString::number(bytesPerSec / 1024.0, 'f', 0) + QStringLiteral(" KB/s");
}

inline QString formatEta(double seconds)
{
    if (seconds < 0 || seconds > 24 * 3600 * 7)
        return QStringLiteral("--");
    const int total = int(seconds);
    if (total >= 3600)
        return QStringLiteral("%1h %2m").arg(total / 3600).arg((total % 3600) / 60);
    if (total >= 60)
        return QStringLiteral("%1m %2s").arg(total / 60).arg(total % 60);
    return QStringLiteral("%1s").arg(total);
}

}
