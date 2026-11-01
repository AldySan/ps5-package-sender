#pragma once

#include <QObject>
#include <QString>
#include <functional>

namespace ps5 {

struct ReceiverStatus {
    bool online = false;
    bool busy = false;
    int active = 0;
    bool pull = false;
    QString pullName;
    qint64 pullGot = 0;
    qint64 pullWant = -1;
    bool pullPaused = false;
};

struct ReceiverSpace {
    bool online = false;
    qint64 free = -1;
    qint64 total = -1;
};

class ReceiverClient : public QObject {
    Q_OBJECT

public:
    static constexpr int Port = 12800;

    using BoolCallback = std::function<void(bool ok, const QString &reply)>;
    using StatusCallback = std::function<void(const ReceiverStatus &status)>;
    using SpaceCallback = std::function<void(const ReceiverSpace &space)>;
    using VersionCallback = std::function<void(bool ok, const QString &build)>;
    using StatCallback = std::function<void(bool ok, bool exists, qint64 size)>;

    explicit ReceiverClient(QObject *parent = nullptr);

    QString consoleIp() const { return m_ip; }
    void setConsoleIp(const QString &ip);

    void probe(const BoolCallback &cb) const;
    void status(const StatusCallback &cb) const;
    void space(const SpaceCallback &cb) const;
    void version(const VersionCallback &cb) const;

    void install(const QString &pkgUrl, const QString &name,
                 const QString &iconUrl, const BoolCallback &cb) const;
    void pull(const QString &url, const QString &remotePath, bool resume,
              const BoolCallback &cb) const;
    void pullPause(bool paused, const BoolCallback &cb) const;
    void pullCancel(const BoolCallback &cb) const;
    void stat(const QString &remotePath, const StatCallback &cb) const;

private:
    struct Reply {
        bool ok = false;
        QString body;
        int httpStatus = 0;
    };

    void get(const QString &path, int timeoutMs,
             const std::function<void(const Reply &)> &cb) const;
    void post(const QString &path, const QByteArray &json, int timeoutMs,
              const std::function<void(const Reply &)> &cb) const;
    QUrl baseUrl() const;
    static QString jsonEscape(const QString &s);

    QString m_ip;
};

}
