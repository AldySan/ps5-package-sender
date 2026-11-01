#pragma once

#include <QObject>
#include <QString>

namespace ps5 {

class Application : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT)

public:
    explicit Application(QObject *parent = nullptr);

    QString version() const { return m_version; }
    QString title() const { return m_title; }

private:
    QString m_version;
    QString m_title;
};

}
