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
    // Live progress of the install job the receiver is running right now
    // (absent on older builds): instPhase 0 = none, 1 = the console fetches
    // the package itself, 2 = the receiver is staging it to /data, 3 = the
    // console is copying the staged package from the receiver. instUrl
    // identifies the job so the queue can match the URL it pushed.
    int instPhase = 0;
    QString instName;
    QString instUrl;
    qint64 instGot = 0;
    qint64 instWant = -1;
    // Outcome of the last install the receiver ran, present once it has
    // run one (empty lastUrl = nothing recorded yet). The queue matches
    // lastUrl against the URL it pushed and fails the row with the
    // console's own words when lastOk is false.
    //
    // lastFinal separates a verdict from a progress note: a direct
    // install the console merely ACCEPTED is recorded with lastOk=true
    // but lastFinal=false — its downloader owns the job from the moment
    // the call returns and never reports back, so that record can never
    // mean "installed". Only a final record may finish a row as done;
    // receivers that predate the flag never send it, and a missing
    // field reads as false (not final) here — the safe default.
    bool hasLast = false;
    bool lastOk = false;
    bool lastFinal = false;
    QString lastUrl;
    QString lastErr;
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
