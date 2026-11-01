#pragma once

#include <QString>
#include <QStringList>

namespace ps5::ScanPaths {

QStringList builtinPaths();

bool isAbsoluteRemotePath(const QString &path);
QString normalize(const QString &path);

}
