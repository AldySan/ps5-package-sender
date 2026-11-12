#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "../src/core/QueueModel.h"
#include "../src/core/RangeFileServer.h"
#include "../src/core/ReceiverClient.h"
#include "../src/core/TransferQueue.h"
#include "../src/db/Database.h"

#include <cstdio>

static int failures = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

static bool waitFor(const std::function<bool()> &predicate, int timeoutMs)
{
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < timeoutMs) {
        if (predicate())
            return true;
        QEventLoop loop;
        QTimer::singleShot(25, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return predicate();
}

class MockConsole : public QTcpServer {
    Q_OBJECT

public:
    MockConsole()
    {
        m_remoteDir = new QTemporaryDir();
        listen(QHostAddress::LocalHost, ps5::ReceiverClient::Port);
    }

    ~MockConsole() override { delete m_remoteDir; }

    QString remotePath(const QString &file) const
    {
        return m_remoteDir->filePath(file);
    }

    void preseedRemote(const QString &file, const QByteArray &data)
    {
        QFile f(remotePath(file));
        f.open(QIODevice::WriteOnly);
        f.write(data);
        f.close();
    }

    ps5::ReceiverStatus status() const
    {
        ps5::ReceiverStatus st;
        st.online = true;
        st.busy = m_busy;
        st.active = m_active;
        st.pull = m_pullActive;
        st.pullName = m_pullName;
        st.pullGot = m_pullGot;
        st.pullWant = m_pullWant;
        st.pullPaused = false;
        st.instPhase = m_instPhase;
        st.instName = m_instName;
        st.instUrl = m_instUrl;
        st.instGot = m_instGot;
        st.instWant = m_instWant;
        st.hasLast = m_hasLast;
        st.lastOk = m_lastOk;
        st.lastFinal = m_lastFinal;
        st.lastUrl = m_lastUrl;
        st.lastErr = m_lastErr;
        return st;
    }

    /* the accepted-only record a newer receiver writes for a direct
     * install: ok, never final, console idle — exactly the report that
     * used to be misread as "installed" */
    bool acceptedOnlyReported() const
    {
        return m_hasLast && m_lastOk && !m_lastFinal && !m_busy;
    }

    QString lastPushUrl() const { return m_lastPushUrl; }

    // The package size the staged timeline reports, set by the test.
    qint64 stagedPkgSize = 0;

    int installCalls = 0;
    int installReplies = 0;
    bool delayNextInstall = false;
    int installReplyDelayMs = 600;
    bool delayNextPause = false;
    int pauseReplyDelayMs = 600;
    int pauseCalls = 0;
    int pauseReplies = 0;
    // What /api/space reports; the default is generous so ordinary flows
    // never trip the pre-flight warning.
    qint64 spaceFree = qint64(1) << 30;

private:
    void incomingConnection(qintptr handle) override
    {
        auto *sock = new QTcpSocket(this);
        sock->setSocketDescriptor(handle);
        connect(sock, &QTcpSocket::readyRead, this, [this, sock] {
            if (!sock->canReadLine())
                return;
            m_buffer = sock->readAll();
            const QList<QByteArray> lines = m_buffer.split('\n');
            const QList<QByteArray> parts = lines.first().simplified().split(' ');
            m_lastPath = QString::fromLatin1(parts.value(1));
            m_lastBody = m_buffer.mid(m_buffer.indexOf("\r\n\r\n") + 4);
            route(sock);
        });
        connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);
    }

    void route(QTcpSocket *sock)
    {
        if (m_lastPath == "/api/status") {
            replyJson(sock, statusJson());
        } else if (m_lastPath == "/api/install") {
            handleInstall(sock);
        } else if (m_lastPath == "/api/files/pull") {
            handlePull(sock);
        } else if (m_lastPath.startsWith("/api/files/stat")) {
            handleStat(sock);
        } else if (m_lastPath == "/api/space") {
            replyJson(sock, QStringLiteral("{\"free\":%1,\"total\":%2}")
                          .arg(QString::number(spaceFree),
                               QString::number(qint64(2) << 30))
                          .toLatin1());
        } else if (m_lastPath == "/api/pull/cancel") {
            m_cancelPull = true;
            replyJson(sock, "{\"ok\":true,\"cancelled\":true}");
        } else if (m_lastPath == "/api/pull/pause") {
            pauseCalls++;
            const auto respond = [this, sock]() {
                pauseReplies++;
                replyJson(sock, "{\"ok\":true,\"paused\":true}");
            };
            if (delayNextPause) {
                delayNextPause = false;
                QTimer::singleShot(pauseReplyDelayMs, this, respond);
            } else {
                respond();
            }
        } else {
            replyJson(sock, "{\"status\":\"fail\",\"error\":\"Unsupported method: "
                            "use POST /api/install\"}");
        }
    }

    QByteArray statusJson() const
    {
        return QStringLiteral(
                   "{\"busy\":%1,\"active\":%2,\"pull\":%3,\"pullName\":\"%4\","
                   "\"pullGot\":%5,\"pullWant\":%6,\"pullPaused\":false}")
            .arg(m_busy ? "true" : "false")
            .arg(m_active)
            .arg(m_pullActive ? "true" : "false")
            .arg(m_pullName, QString::number(m_pullGot), QString::number(m_pullWant))
            .toLatin1();
    }

    void handleInstall(QTcpSocket *sock)
    {
        installCalls++;
        const QJsonObject body =
            QJsonDocument::fromJson(m_lastBody).object();
        const QString url = body.value(QStringLiteral("packages"))
                                .toArray().at(0).toString();
        const QString name = body.value(QStringLiteral("name")).toString();
        const bool fail = name.contains(QLatin1String("FAILME")) && m_installFailures < 1;
        // "INSTALLFAIL": the push is accepted (the console downloads the
        // package) but the install itself fails afterwards — the case the
        // receiver reports through the lastOk/lastErr status fields.
        const bool failInstall = name.contains(QLatin1String("INSTALLFAIL"));
        // "DOWNLOADFAIL": the console's downloader gives up part-way — it
        // accepts the push, pulls only a slice of the package and the
        // install dies with the console's own error while the PC's served
        // counter never reaches the full size. "PARTIALOK": the console
        // finishes the install having fetched fewer bytes than the PC saw
        // served (a resumed download). Both leave the row counting
        // download bytes with the outcome already reported.
        const bool abortDownload = name.contains(QLatin1String("DOWNLOADFAIL"));
        // "SILENTOK": the console accepts the push and takes the whole
        // package but never reports an install outcome — a receiver that
        // restarted mid-install comes back idle with no record for it.
        // "STALLME": the console accepts the push, fetches a slice of the
        // package and then goes idle with no report at all — a download
        // that died silently, which is how a restart between two polls
        // looks from the PC.
        const bool silentOk = name.contains(QLatin1String("SILENTOK"));
        const bool stallMe = name.contains(QLatin1String("STALLME"));
        // "ACCEPTSTALL": the console accepts the push and the receiver
        // writes the accepted-only record right away, but the console's
        // downloader only starts fetching 1.5 s later — the window in
        // which the old code declared victory and unregistered the file
        // while the download had not even begun.
        const bool acceptStall = name.contains(QLatin1String("ACCEPTSTALL"));
        const bool partialFetch = abortDownload || stallMe
            || name.contains(QLatin1String("PARTIALOK"));
        // "PARTIALOK": the downloader takes a slice, then resumes and
        // takes the whole file — the PC sees slice + full served.
        const bool resumeCompletes = name.contains(QLatin1String("PARTIALOK"));
        // "STAGED*": the console refuses the LAN route and the receiver
        // runs the two-copy choreography (stage to /data, then the console
        // pulls it back from the receiver's loopback server) — reported
        // through the instPhase/instGot progress fields.
        const bool staged = name.contains(QLatin1String("STAGED"));
        const auto respond = [this, sock, fail, url, failInstall, abortDownload,
                              partialFetch, resumeCompletes, staged, name, silentOk,
                              stallMe, acceptStall]() {
            installReplies++;
            if (fail) {
                m_installFailures++;
                replyJson(sock, "{\"status\":\"fail\"}");
                return;
            }
            replyJson(sock, "{\"status\":\"success\"}");

            if (staged) {
                runStagedTimeline(url, name,
                                  !name.contains(QLatin1String("STAGEDFAIL")));
                return;
            }
            if (acceptStall) {
                // accepted-only record right away (the receiver's worker
                // is done), the console's download starts only later
                m_hasLast = true;
                m_lastUrl = url;
                m_lastOk = true;
                m_lastFinal = false;
                m_lastErr.clear();
                m_lastPushUrl = url;
                QTimer::singleShot(1500, this, [this, url]() {
                    download(url, [](const QByteArray &) {}, false);
                });
                return;
            }
            m_active++;
            m_busy = true;
            const auto finish = [this, url, failInstall, abortDownload,
                                 silentOk, stallMe]() {
                m_active--;
                m_busy = m_active > 0;
                if (silentOk || stallMe)
                    return; // the outcome is never reported
                m_hasLast = true;
                m_lastUrl = url;
                m_lastPushUrl = url;
                if (failInstall || abortDownload) {
                    m_lastOk = false;
                    // a failure is definitive
                    m_lastFinal = true;
                    m_lastErr = QStringLiteral(
                        "not enough free space on the console");
                } else {
                    m_lastOk = true;
                    // accepted: the console's downloader owns the job
                    // and never reports back — never a verdict
                    m_lastFinal = false;
                    m_lastErr.clear();
                }
            };
            if (failInstall) {
                // The console accepts the push and pulls the whole
                // package, and only then reports the failed install —
                // the row sits in "installing on console…" meanwhile.
                download(url, [this, finish](const QByteArray &) {
                    QTimer::singleShot(600, this, finish);
                }, partialFetch);
            } else if (resumeCompletes) {
                // A slice first, then the whole file: the PC's served
                // counter ends up past the size (a resumed download).
                download(url, [this, url, finish](const QByteArray &) {
                    download(url, [finish](const QByteArray &) { finish(); },
                             false);
                }, true);
            } else {
                download(url, [finish](const QByteArray &) { finish(); },
                          partialFetch);
            }
        };
        if (delayNextInstall) {
            delayNextInstall = false;
            QTimer::singleShot(installReplyDelayMs, this, respond);
        } else {
            respond();
        }
    }

    void handlePull(QTcpSocket *sock)
    {
        const QJsonObject body = QJsonDocument::fromJson(m_lastBody).object();
        const QString url = body.value(QStringLiteral("url")).toString();
        const QString path = mapRemotePath(body.value(QStringLiteral("path")).toString());
        replyJson(sock, "{\"ok\":true,\"started\":true}");

        m_pullName = QFileInfo(path).fileName();
        m_pullWant = 0;
        m_pullGot = 0;
        m_pullActive = true;
        m_cancelPull = false;
        download(url, [this, path](const QByteArray &bytes) {
            QFile f(path);
            f.open(QIODevice::WriteOnly);
            f.write(bytes);
            f.close();
            m_pullActive = false;
        });
    }

    void handleStat(QTcpSocket *sock)
    {
        const QString path = mapRemotePath(QUrl::fromPercentEncoding(
            m_lastPath.mid(QString("/api/files/stat?path=").size()).toLatin1()));
        QFileInfo fi(path);
        if (fi.exists()) {
            replyJson(sock, QStringLiteral("{\"exists\":true,\"size\":%1}")
                                 .arg(fi.size()).toLatin1());
        } else {
            replyJson(sock, "{\"exists\":false,\"size\":0}");
        }
    }

    QString mapRemotePath(const QString &remote) const
    {
        const QString prefix = QStringLiteral("/data/homebrew/");
        if (remote.startsWith(prefix))
            return m_remoteDir->filePath(remote.mid(prefix.size()));
        return remote;
    }

    void download(const QString &url, const std::function<void(const QByteArray &)> &done,
                  bool partial = false)
    {
        QNetworkRequest req((QUrl(url)));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        if (partial)
            req.setRawHeader("Range", "bytes=0-99");
        QNetworkReply *reply = m_nam.get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, done] {
            const QByteArray bytes = reply->readAll();
            const qint64 want = reply->header(
                QNetworkRequest::ContentLengthHeader).toLongLong();
            if (m_pullActive && !m_cancelPull) {
                m_pullWant = want;
                m_pullGot = qint64(bytes.size());
            }
            reply->deleteLater();
            done(bytes);
        });
    }

    void replyJson(QTcpSocket *sock, const QByteArray &body)
    {
        sock->write("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
                    "Content-Length: " + QByteArray::number(body.size())
                    + "\r\nConnection: close\r\n\r\n" + body);
        sock->flush();
        sock->disconnectFromHost();
    }

    /* Deterministic staged-install choreography: staging reaches half at
     * +400ms, the console's copy leg starts at +800ms, half of it at
     * +1200ms, and the receiver's last-install report lands at +1600ms.
     * The 400ms windows are four times wider than the 100ms poll, so
     * every phase is observable without racing. */
    void runStagedTimeline(const QString &url, const QString &name, bool ok)
    {
        const qint64 size = stagedPkgSize;

        m_active++;
        m_busy = true;
        m_instPhase = 2;
        m_instUrl = url;
        m_instName = name;
        m_instGot = 0;
        m_instWant = size;
        QTimer::singleShot(400, this, [this, size]() { m_instGot = size / 2; });
        QTimer::singleShot(800, this, [this]() {
            m_instPhase = 3;
            m_instGot = 0;
        });
        QTimer::singleShot(1200, this, [this, size]() {
            m_instGot = size / 2;
        });
        QTimer::singleShot(1600, this, [this, url, ok]() {
            m_instPhase = 0;
            m_active--;
            m_busy = m_active > 0;
            m_hasLast = true;
            m_lastUrl = url;
            m_lastPushUrl = url;
            m_lastOk = ok;
            // a staged outcome is definitive: the receiver served the
            // whole copy before recording it
            m_lastFinal = true;
            m_lastErr = ok
                ? QString()
                : QStringLiteral("not enough free space on the console");
        });
    }

    QNetworkAccessManager m_nam;
    QTemporaryDir *m_remoteDir = nullptr;
    QByteArray m_buffer;
    QString m_lastPath;
    QByteArray m_lastBody;

    bool m_busy = false;
    int m_active = 0;
    bool m_pullActive = false;
    QString m_pullName;
    qint64 m_pullGot = 0;
    qint64 m_pullWant = 0;
    bool m_cancelPull = false;
    int m_instPhase = 0;
    QString m_instName;
    QString m_instUrl;
    qint64 m_instGot = 0;
    qint64 m_instWant = -1;
    int m_installFailures = 0;
    bool m_hasLast = false;
    bool m_lastOk = false;
    bool m_lastFinal = false;
    QString m_lastUrl;
    QString m_lastErr;
    QString m_lastPushUrl;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dataDir;
    qputenv("PS5PKGSENDER_DATA_DIR", dataDir.path().toUtf8());

    CHECK(ps5::Database::instance().open());

    MockConsole console;
    ps5::ReceiverClient receiver;
    receiver.setConsoleIp(QStringLiteral("127.0.0.1"));
    ps5::RangeFileServer server;
    CHECK(server.start());

    ps5::TransferQueue queue(&receiver, &server);
    queue.setPcIp(QStringLiteral("127.0.0.1"));
    queue.setDestination(QStringLiteral("/data/homebrew"));

    QTimer pollTimer;
    pollTimer.setInterval(100);
    QObject::connect(&pollTimer, &QTimer::timeout, &pollTimer, [&] {
        receiver.status([&queue, &console](const ps5::ReceiverStatus &st) {
            if (st.online)
                queue.onReceiverStatus(console.status());
        });
    });
    pollTimer.start();

    {
        QTemporaryDir sourceDir;
        const QString pkgPath = sourceDir.filePath("ps5pkg-queue-test-file");
        const QString dumpPath = sourceDir.filePath("ps5pkg-queue-test-dump.exfat");

        QByteArray payload(1200000, 0);
        for (int i = 0; i < payload.size(); i++)
            payload[i] = char((i * 7) % 251);

        ps5::LibraryRow row;
        row.id = 1;
        row.title = "Queue Test Game";
        row.format = "pkg";
        row.size = payload.size();

        ps5::LibraryRow imageRow;
        imageRow.id = 2;
        imageRow.title = "Queue Test Dump";
        imageRow.format = "exfat";
        imageRow.size = payload.size();

        // --- install flow ---
        server.registerFile("q1", pkgPath);
        {
            QFile f(pkgPath);
            f.open(QIODevice::WriteOnly);
            CHECK(f.isOpen());
            f.write(payload);
            f.close();
        }
        row.path = pkgPath;
        queue.enqueue({row}, false);
        CHECK(waitFor([&] {
            return queue.model()->count() == 1
                && queue.model()->itemAt(0).state == ps5::TransferState::Done;
        }, 15000));
        // A direct install is only ever "accepted" by the console: its
        // downloader owns the job and never reports back, so the row
        // closes honestly with the outcome unreported rather than a
        // verdict nobody has.
        if (queue.model()->count() == 1)
            CHECK(queue.model()->itemAt(0).message
                  == "installed (outcome unreported)");

        // --- copy flow ---
        {
            QFile f(dumpPath);
            f.open(QIODevice::WriteOnly);
            CHECK(f.isOpen());
            f.write(payload);
            f.close();
        }
        imageRow.path = dumpPath;
        queue.enqueue({imageRow}, true);
        CHECK(waitFor([&] {
            return queue.model()->count() == 2
                && queue.model()->itemAt(1).state == ps5::TransferState::Done;
        }, 15000));
        {
            QFile f(console.remotePath("ps5pkg-queue-test-dump.exfat"));
            CHECK(f.exists());
            if (f.exists()) {
                f.open(QIODevice::ReadOnly);
                CHECK(f.readAll() == payload);
            }
        }

        // --- skip flow (same size already present) ---
        console.preseedRemote("ps5pkg-queue-test-dump.exfat", payload);
        queue.enqueue({imageRow}, true);
        CHECK(waitFor([&] {
            return queue.model()->count() == 3
                && queue.model()->itemAt(2).state == ps5::TransferState::Skipped;
        }, 15000));

        // --- failed install then retryFailed ---
        ps5::LibraryRow failRow = row;
        failRow.title = "FAILME Once";
        queue.enqueue({failRow}, false);
        CHECK(waitFor([&] {
            return queue.model()->count() == 4
                && queue.model()->itemAt(3).state == ps5::TransferState::Failed;
        }, 15000));
        queue.retryFailed();
        CHECK(waitFor([&] {
            return queue.model()->count() == 4
                && queue.model()->itemAt(3).state == ps5::TransferState::Done;
        }, 15000));
        CHECK(console.installCalls >= 3);

        // --- history recorded ---
        const QList<ps5::HistoryRow> history = ps5::Database::instance().historyRows(50);
        CHECK(history.size() >= 4);
        for (const ps5::HistoryRow &entry : history) {
            CHECK(entry.started > 0);
            CHECK(entry.finished >= entry.started);
        }

        // --- cancel during push stays cancelled when the reply lands late ---
        ps5::LibraryRow cancelRow = row;
        cancelRow.title = "Cancel Me";
        console.delayNextInstall = true;
        queue.enqueue({cancelRow}, false);
        CHECK(waitFor([&] {
            return queue.model()->count() == 5
                && queue.model()->itemAt(4).state == ps5::TransferState::Pushing;
        }, 5000));
        queue.cancelRow(queue.model()->itemAt(4).id);
        CHECK(queue.model()->itemAt(4).state == ps5::TransferState::Cancelled);
        CHECK(waitFor([&] { return console.installCalls >= 4; }, 10000));
        waitFor([&] { return false; }, 800);
        CHECK(queue.model()->itemAt(4).state == ps5::TransferState::Cancelled);
        CHECK(queue.model()->itemAt(4).message == "cancelled");

        // --- a late reply must not release another item's push slot ---
        ps5::LibraryRow firstRow = row;
        firstRow.title = "First Push";
        ps5::LibraryRow secondRow = row;
        secondRow.title = "Second Push";
        ps5::LibraryRow thirdRow = row;
        thirdRow.title = "Third Push";

        const int callsBefore = console.installCalls;
        const int repliesBefore = console.installReplies;
        console.delayNextInstall = true;
        queue.enqueue({firstRow}, false);
        CHECK(waitFor([&] { return console.installCalls == callsBefore + 1; }, 5000));
        CHECK(waitFor([&] {
            return queue.model()->count() == 6
                && queue.model()->itemAt(5).state == ps5::TransferState::Pushing;
        }, 5000));
        queue.cancelRow(queue.model()->itemAt(5).id);
        CHECK(queue.model()->itemAt(5).state == ps5::TransferState::Cancelled);

        console.delayNextInstall = true;
        console.installReplyDelayMs = 1500;
        queue.enqueue({secondRow}, false);
        CHECK(waitFor([&] { return console.installCalls == callsBefore + 2; }, 5000));
        CHECK(queue.model()->itemAt(6).state == ps5::TransferState::Pushing);

        // Let the cancelled item's delayed reply land while the second push
        // is still waiting for its own (much later) reply.
        CHECK(waitFor([&] { return console.installReplies == repliesBefore + 1; },
                      5000));

        // The third item must stay queued: the second push still owns the slot.
        queue.enqueue({thirdRow}, false);
        CHECK(queue.model()->itemAt(7).state == ps5::TransferState::Queued);
        console.installReplyDelayMs = 600;

        CHECK(waitFor([&] {
            return queue.model()->itemAt(6).state == ps5::TransferState::Done;
        }, 15000));
        CHECK(waitFor([&] {
            return queue.model()->itemAt(7).state == ps5::TransferState::Done;
        }, 15000));
        CHECK(queue.model()->itemAt(5).state == ps5::TransferState::Cancelled);

        // The status poll flips Paused back to Copying (the mock never really
        // pauses), which would make the races below nondeterministic. The
        // pause/resume/cancel HTTP replies still land via the event loop.
        pollTimer.stop();

        // --- a late pause reply must not resurrect a cancelled copy ---
        const QString pausePath = sourceDir.filePath("pause-race.exfat");
        {
            QFile f(pausePath);
            f.open(QIODevice::WriteOnly);
            f.write(payload);
            f.close();
        }
        ps5::LibraryRow pauseRow = imageRow;
        pauseRow.title = "Pause Race";
        pauseRow.path = pausePath;
        queue.enqueue({pauseRow}, true);
        CHECK(waitFor([&] {
            return queue.model()->count() == 9
                && queue.model()->itemAt(8).state == ps5::TransferState::Copying;
        }, 15000));
        console.delayNextPause = true;
        console.pauseReplyDelayMs = 900;
        queue.pauseRow(queue.model()->itemAt(8).id);   // reply will land late
        queue.cancelRow(queue.model()->itemAt(8).id);  // cancel wins the race
        CHECK(waitFor([&] {
            return queue.model()->itemAt(8).state == ps5::TransferState::Cancelled;
        }, 5000));
        CHECK(waitFor([&] { return console.pauseReplies >= 1; }, 5000));
        waitFor([&] { return false; }, 400);
        CHECK(queue.model()->itemAt(8).state == ps5::TransferState::Cancelled);
        CHECK(queue.model()->itemAt(8).message == "cancelled (partial kept)");

        // --- a late resume reply must not resurrect a cancelled copy ---
        const QString resumePath = sourceDir.filePath("resume-race.exfat");
        {
            QFile f(resumePath);
            f.open(QIODevice::WriteOnly);
            f.write(payload);
            f.close();
        }
        ps5::LibraryRow resumeRow = imageRow;
        resumeRow.title = "Resume Race";
        resumeRow.path = resumePath;
        queue.enqueue({resumeRow}, true);
        CHECK(waitFor([&] {
            return queue.model()->count() == 10
                && queue.model()->itemAt(9).state == ps5::TransferState::Copying;
        }, 15000));
        queue.pauseRow(queue.model()->itemAt(9).id);
        CHECK(waitFor([&] {
            return queue.model()->itemAt(9).state == ps5::TransferState::Paused;
        }, 5000));
        console.delayNextPause = true;
        console.pauseReplyDelayMs = 900;
        queue.resumeRow(queue.model()->itemAt(9).id);  // reply will land late
        // Cancelling a paused copy goes through the console (async pullCancel),
        // so the cancelled state settles once that reply arrives.
        queue.cancelRow(queue.model()->itemAt(9).id);
        CHECK(waitFor([&] {
            return queue.model()->itemAt(9).state == ps5::TransferState::Cancelled;
        }, 5000));
        // pauseReplies: 1 = pause-race delayed, 2 = the pause above,
        // 3 = the delayed resume reply that must not resurrect the item.
        CHECK(waitFor([&] { return console.pauseReplies >= 3; }, 5000));
        waitFor([&] { return false; }, 400);
        CHECK(queue.model()->itemAt(9).state == ps5::TransferState::Cancelled);

        // --- enqueueing a file that no longer exists fails fast ---
        // The scan metadata can outlive the file on disk; the row must fail
        // in the queue with a clear message instead of pushing a dead URL
        // and failing minutes later inside the console install.
        QStringList notifyLog;
        QObject::connect(&queue, &ps5::TransferQueue::notify, &queue,
                         [&](const QString &msg) { notifyLog << msg; });
        ps5::LibraryRow ghostRow = row;
        ghostRow.title = "Ghost File";
        ghostRow.path = sourceDir.filePath("never-existed.pkg");
        const int callsBeforeGhost = console.installCalls;
        queue.enqueue({ghostRow}, false);
        CHECK(queue.model()->count() == 11);
        CHECK(queue.model()->itemAt(10).state == ps5::TransferState::Failed);
        CHECK(queue.model()->itemAt(10).message
              == "file no longer exists on the PC");
        // no request was wasted on the dead file
        CHECK(console.installCalls == callsBeforeGhost);
        bool ghostNotified = false;
        for (const QString &msg : notifyLog)
            ghostNotified |= msg.contains(QLatin1String("no longer exist"));
        CHECK(ghostNotified);

        // --- retrying after the file was deleted fails fast too ---
        // The Done transition needs the status poll, which was paused for the
        // pause/resume races above.
        pollTimer.start();
        const QString doomedPath = sourceDir.filePath("doomed.pkg");
        {
            QFile f(doomedPath);
            f.open(QIODevice::WriteOnly);
            CHECK(f.isOpen());
            f.write(payload);
            f.close();
        }
        ps5::LibraryRow doomedRow = row;
        doomedRow.title = "Doomed File";
        doomedRow.path = doomedPath;
        queue.enqueue({doomedRow}, false);
        CHECK(waitFor([&] {
            return queue.model()->itemAt(11).state == ps5::TransferState::Done;
        }, 15000));
        CHECK(QFile::remove(doomedPath));
        queue.retryRow(queue.model()->itemAt(11).id);
        CHECK(queue.model()->itemAt(11).state == ps5::TransferState::Failed);
        CHECK(queue.model()->itemAt(11).message
              == "file no longer exists on the PC");

        // --- an install the console accepts but then fails is a failed row ---
        // The push succeeds and the console downloads the whole package,
        // but the install itself dies afterwards. The receiver reports
        // that outcome (lastOk=false + the URL it ran + its own error
        // text); the queue must match the URL it pushed and fail the row
        // with the console's words instead of a false "installed".
        ps5::LibraryRow consoleFailRow = row;
        consoleFailRow.title = "INSTALLFAIL Once";
        queue.enqueue({consoleFailRow}, false);
        CHECK(waitFor([&] {
            return queue.model()->itemAt(12).state == ps5::TransferState::Installing;
        }, 15000));
        // The install tail has no progress numbers: the row reports an
        // indeterminate phase (spinner + moving bar), not a fake "100%".
        CHECK(queue.model()->data(queue.model()->index(12, 0),
                                  ps5::QueueModel::IndeterminateRole).toBool());
        CHECK(queue.model()->data(queue.model()->index(12, 0),
                                  ps5::QueueModel::PercentRole).toDouble()
              == 100.0);
        CHECK(waitFor([&] {
            return queue.model()->count() == 13
                && queue.model()->itemAt(12).state == ps5::TransferState::Failed;
        }, 15000));
        CHECK(queue.model()->itemAt(12).message
              == "console: not enough free space on the console");

        // A following successful install of a different row still closes
        // Done — the failed last-result must not leak onto it.
        ps5::LibraryRow okRow = row;
        okRow.title = "Good After Bad";
        queue.enqueue({okRow}, false);
        CHECK(waitFor([&] {
            return queue.model()->count() == 14
                && queue.model()->itemAt(13).state == ps5::TransferState::Done;
        }, 15000));
        CHECK(queue.model()->itemAt(13).message
              == "installed (outcome unreported)");

        // --- a download the console abandons fails the row, not hangs ---
        // The console accepts the push but its downloader gives up after
        // a slice: the served counter never reaches the size, the
        // receiver goes idle with lastOk=false for the exact URL this row
        // pushed, and the console shows its own error. The row must end
        // Failed with those words instead of sitting in
        // "console is downloading…" forever.
        ps5::LibraryRow dlFailRow = row;
        dlFailRow.title = "DOWNLOADFAIL Once";
        queue.enqueue({dlFailRow}, false);
        CHECK(waitFor([&] {
            return queue.model()->count() == 15
                && queue.model()->itemAt(14).state == ps5::TransferState::Failed;
        }, 15000));
        CHECK(queue.model()->itemAt(14).message
              == "console: not enough free space on the console");

        // --- an install finished from a partial fetch is done ---
        // The downloader takes a slice, then resumes and takes the whole
        // file: the PC sees slice + full served (past the size). Only a
        // final verdict may close a row as "installed", and a resumed
        // direct download carries none — so the row settles Done with
        // the outcome unreported instead of hanging while it counts
        // bytes that never come.
        ps5::LibraryRow partialOkRow = row;
        partialOkRow.title = "PARTIALOK Once";
        queue.enqueue({partialOkRow}, false);
        CHECK(waitFor([&] {
            return queue.model()->count() == 16
                && queue.model()->itemAt(15).state == ps5::TransferState::Done;
        }, 15000));
        CHECK(queue.model()->itemAt(15).message
              == "installed (outcome unreported)");

        // --- a tight console warns before the transfer, not after ---
        // The receiver reports less than twice the package free: the push
        // still goes out (the console decides), but the user hears about
        // the space now instead of after a full download and a refusal.
        console.spaceFree = payload.size(); // < 2 * size → must warn
        ps5::LibraryRow tightRow = row;
        tightRow.title = "Tight Space";
        queue.enqueue({tightRow}, false);
        CHECK(waitFor([&] {
            return queue.model()->count() == 17
                && queue.model()->itemAt(16).state == ps5::TransferState::Done;
        }, 15000));
        bool spaceWarned = false;
        for (int i = 0; i < 40 && !spaceWarned; i++) {
            for (const QString &msg : notifyLog)
                spaceWarned |= msg.contains(QLatin1String("free on the console"))
                    && msg.contains(QLatin1String("Tight Space"));
            if (!spaceWarned)
                waitFor([&] { return false; }, 50);
        }
        CHECK(spaceWarned);

        // --- an install the console never reports closes honestly ---
        // The push is accepted and the whole package is served, but the
        // receiver restarts before it records the outcome: it comes back
        // idle with no report for this row. The row must not claim a
        // confirmed install.
        ps5::LibraryRow silentRow = row;
        silentRow.title = "SILENTOK Once";
        queue.enqueue({silentRow}, false);
        CHECK(waitFor([&] {
            return queue.model()->count() == 18
                && queue.model()->itemAt(17).state
                       == ps5::TransferState::Installing;
        }, 15000));
        CHECK(waitFor([&] {
            return queue.model()->itemAt(17).state == ps5::TransferState::Done;
        }, 15000));
        CHECK(queue.model()->itemAt(17).message
              == "installed (outcome unreported)");

        // --- a download that dies with the receiver fails, not hangs ---
        // The console accepts the push, fetches a slice and then the
        // receiver restarts between two polls: it comes back online
        // idle, with no job, no report and no bytes moving. The queue
        // must fail the row once the stall window passes instead of
        // sitting in "console is downloading…" forever.
        queue.setStallFailMs(1200);
        ps5::LibraryRow stallRow = row;
        stallRow.title = "STALLME Once";
        queue.enqueue({stallRow}, false);
        CHECK(waitFor([&] {
            return queue.model()->count() == 19
                && queue.model()->itemAt(18).state
                       == ps5::TransferState::Downloading;
        }, 15000));
        CHECK(waitFor([&] {
            return queue.model()->itemAt(18).state == ps5::TransferState::Failed;
        }, 15000));
        CHECK(queue.model()->itemAt(18).message.contains(
            QLatin1String("stopped downloading")));
        queue.setStallFailMs(ps5::TransferQueue::DefaultStallFailMs);

        // --- a staged install keeps one bar growing through both copies ---
        // The console refuses the LAN route; the receiver stages the
        // package and the console then copies it back from the receiver's
        // loopback server. The queue counts both legs against twice the
        // size: 25% while staging has half the package, 75% while the
        // console has half of its copy — never a dead full bar in
        // between — and the row settles Done from the receiver's report.
        console.stagedPkgSize = payload.size();
        ps5::LibraryRow stagedOkRow = row;
        stagedOkRow.title = "STAGEDOK Once";
        queue.enqueue({stagedOkRow}, false);
        CHECK(waitFor([&] {
            return queue.model()->count() == 20
                && queue.model()->itemAt(19).state == ps5::TransferState::Downloading
                && queue.model()->itemAt(19).staged
                && queue.model()->itemAt(19).message
                       == "console is preparing the download…";
        }, 15000));
        CHECK(waitFor([&] {
            return queue.model()->itemAt(19).sent == payload.size() / 2;
        }, 15000));
        CHECK(queue.model()->data(queue.model()->index(19, 0),
                                  ps5::QueueModel::PercentRole).toDouble()
              == 25.0);
        CHECK(waitFor([&] {
            return queue.model()->itemAt(19).message
                == "console is downloading…";
        }, 15000));
        CHECK(waitFor([&] {
            return queue.model()->itemAt(19).sent
                == payload.size() + payload.size() / 2;
        }, 15000));
        CHECK(queue.model()->data(queue.model()->index(19, 0),
                                  ps5::QueueModel::PercentRole).toDouble()
              == 75.0);
        CHECK(waitFor([&] {
            return queue.model()->itemAt(19).state == ps5::TransferState::Done;
        }, 15000));
        CHECK(queue.model()->itemAt(19).message == "installed");

        // --- a staged install whose console copy dies fails with its words ---
        ps5::LibraryRow stagedFailRow = row;
        stagedFailRow.title = "STAGEDFAIL Once";
        queue.enqueue({stagedFailRow}, false);
        CHECK(waitFor([&] {
            return queue.model()->count() == 21
                && queue.model()->itemAt(20).state == ps5::TransferState::Downloading
                && queue.model()->itemAt(20).staged;
        }, 15000));
        CHECK(waitFor([&] {
            return queue.model()->itemAt(20).message
                == "console is downloading…";
        }, 15000));
        CHECK(waitFor([&] {
            return queue.model()->itemAt(20).state == ps5::TransferState::Failed;
        }, 15000));
        CHECK(queue.model()->itemAt(20).message
              == "console: not enough free space on the console");

        // --- an accepted install keeps the row open and the file served ---
        // The console accepts the push and the receiver immediately
        // writes its accepted-only record (ok, never final, console
        // idle) — but the console's own download starts only later. The
        // row must NOT claim "installed" on that record, and the
        // package must still be fetchable from the file server when the
        // download finally runs: unregistering it here is exactly what
        // used to kill the console's download with a 404 mid-package
        // ("Can't install.").
        ps5::LibraryRow acceptRow = row;
        acceptRow.title = "ACCEPTSTALL Once";
        queue.enqueue({acceptRow}, false);
        CHECK(waitFor([&] {
            return queue.model()->count() == 22 && console.acceptedOnlyReported();
        }, 15000));
        // accepted, console idle per the receiver — the row must still
        // be open, not finished by the accepted-only record
        CHECK(queue.model()->itemAt(21).state == ps5::TransferState::Downloading);
        // the package is still served to the console's downloader
        {
            QNetworkAccessManager nam;
            QNetworkReply *reply = nam.get(QNetworkRequest(QUrl(console.lastPushUrl())));
            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            QTimer::singleShot(5000, &loop, &QEventLoop::quit);
            loop.exec();
            const int code = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const qint64 length = reply->header(
                QNetworkRequest::ContentLengthHeader).toLongLong();
            const bool stillServed = reply->error() == QNetworkReply::NoError
                && code == 200 && length == payload.size();
            reply->deleteLater();
            CHECK(stillServed);
        }
        CHECK(waitFor([&] {
            return queue.model()->itemAt(21).state == ps5::TransferState::Done;
        }, 15000));
        CHECK(queue.model()->itemAt(21).message
              == "installed (outcome unreported)");
    }

    if (failures == 0) {
        std::fprintf(stderr, "transfer queue tests passed\n");
        return 0;
    }
    return 1;
}

#include "queuetest.moc"
