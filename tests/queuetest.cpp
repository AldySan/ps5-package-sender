#include <QCoreApplication>
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
        return st;
    }

    int installCalls = 0;

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
        } else if (m_lastPath == "/api/pull/cancel") {
            m_cancelPull = true;
            replyJson(sock, "{\"ok\":true,\"cancelled\":true}");
        } else if (m_lastPath == "/api/pull/pause") {
            replyJson(sock, "{\"ok\":true,\"paused\":true}");
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
        if (name.contains(QLatin1String("FAILME")) && m_installFailures < 1) {
            m_installFailures++;
            replyJson(sock, "{\"status\":\"fail\"}");
            return;
        }
        replyJson(sock, "{\"status\":\"success\"}");

        m_active++;
        m_busy = true;
        download(url, [this](const QByteArray &) {
            m_active--;
            m_busy = m_active > 0;
        });
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

    void download(const QString &url, const std::function<void(const QByteArray &)> &done)
    {
        QNetworkRequest req((QUrl(url)));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
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
    int m_installFailures = 0;
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
        if (queue.model()->count() == 1)
            CHECK(queue.model()->itemAt(0).message == "installed");

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
    }

    if (failures == 0) {
        std::fprintf(stderr, "transfer queue tests passed\n");
        return 0;
    }
    return 1;
}

#include "queuetest.moc"
