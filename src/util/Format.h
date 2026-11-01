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

}
