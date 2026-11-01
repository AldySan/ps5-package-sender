#pragma once

#include <QList>
#include <QObject>
#include <QStringList>

#include "db/Database.h"

namespace ps5 {

class LibraryScanner : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    void scan(const QStringList &roots);

signals:
    void progress(int found, int parsed);
    void finished();

private:
    bool m_running = false;
};

}
