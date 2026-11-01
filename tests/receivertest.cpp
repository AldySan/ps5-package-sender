#include <QCoreApplication>
#include <QEventLoop>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "../src/core/ReceiverClient.h"

#include <cstdio>

static int failures = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

class MockReceiver : public QTcpServer {
    Q_OBJECT

public:
    QString lastBody;
    QString lastPath;

    bool start()
    {
        return listen(QHostAddress::LocalHost, ps5::ReceiverClient::Port);
    }

private:
    void incomingConnection(qintptr handle) override
    {
        QTcpSocket *sock = new QTcpSocket(this);
        sock->setSocketDescriptor(handle);
        connect(sock, &QTcpSocket::readyRead, this, [this, sock] {
            if (!sock->canReadLine())
                return;
            while (sock->bytesAvailable() > 0 && !sock->atEnd())
                if (!sock->waitForReadyRead(50))
                    break;
            const QByteArray req = sock->readAll();
            const QList<QByteArray> lines = req.split('\n');
            const QList<QByteArray> parts = lines.first().simplified().split(' ');
            lastPath = parts.value(1);
            lastBody = req.mid(req.indexOf("\r\n\r\n") + 4);

            QByteArray reply;
            if (lastPath == "/api") {
                reply = "{\"status\":\"fail\",\"error\":\"Unsupported method: "
                        "use POST /api/install\"}";
            } else if (lastPath == "/api/status") {
                reply = "{\"busy\":true,\"active\":1,\"pull\":true,"
                        "\"pullName\":\"game.exfat\",\"pullGot\":100,"
                        "\"pullWant\":400,\"pullPaused\":false}";
            } else if (lastPath == "/api/space") {
                reply = "{\"free\":1000,\"total\":2000}";
            } else if (lastPath == "/api/version") {
                reply = "{\"build\":\"v1.2.3\"}";
            } else if (lastPath == "/api/install") {
                reply = "{\"status\":\"success\"}";
            } else if (lastPath == "/api/files/pull") {
                reply = "{\"ok\":true,\"started\":true}";
            } else if (lastPath.startsWith("/api/pull/pause")) {
                reply = "{\"ok\":true,\"paused\":true}";
            } else if (lastPath.startsWith("/api/pull/cancel")) {
                reply = "{\"ok\":true,\"cancelled\":true}";
            } else if (lastPath.startsWith("/api/files/stat")) {
                reply = "{\"exists\":true,\"size\":400}";
            } else {
                reply = "{\"status\":\"fail\"}";
            }
            sock->write("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
                        "Content-Length: " + QByteArray::number(reply.size())
                        + "\r\nConnection: close\r\n\r\n" + reply);
            sock->flush();
            sock->disconnectFromHost();
        });
        connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);
    }
};

template <typename F>
void waitCallback(F &&startCallback, int timeoutMs = 5000)
{
    QEventLoop loop;
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    startCallback([&loop]() { loop.quit(); });
    loop.exec();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    MockReceiver mock;
    if (!mock.start()) {
        std::fprintf(stderr, "port %d busy, skipping endpoint tests\n",
                     int(ps5::ReceiverClient::Port));
        return 0;
    }

    ps5::ReceiverClient client;
    client.setConsoleIp(QStringLiteral("127.0.0.1"));

    {
        bool ok = false;
        QString body;
        waitCallback([&](auto done) {
            client.probe([&](bool o, const QString &b) {
                ok = o;
                body = b;
                done();
            });
        });
        CHECK(ok);
        CHECK(body.contains("Unsupported method"));
    }

    {
        ps5::ReceiverStatus st;
        waitCallback([&](auto done) {
            client.status([&](const ps5::ReceiverStatus &s) {
                st = s;
                done();
            });
        });
        CHECK(st.online);
        CHECK(st.busy);
        CHECK(st.active == 1);
        CHECK(st.pull);
        CHECK(st.pullName == "game.exfat");
        CHECK(st.pullGot == 100);
        CHECK(st.pullWant == 400);
        CHECK(!st.pullPaused);
    }

    {
        ps5::ReceiverSpace sp;
        waitCallback([&](auto done) {
            client.space([&](const ps5::ReceiverSpace &s) {
                sp = s;
                done();
            });
        });
        CHECK(sp.online);
        CHECK(sp.free == 1000);
        CHECK(sp.total == 2000);
    }

    {
        bool ok = false;
        QString build;
        waitCallback([&](auto done) {
            client.version([&](bool o, const QString &b) {
                ok = o;
                build = b;
                done();
            });
        });
        CHECK(ok);
        CHECK(build == "v1.2.3");
    }

    {
        bool ok = false;
        QString reply;
        waitCallback([&](auto done) {
            client.install("http://192.168.1.2:9898/pkg/1", "My Game",
                           "http://192.168.1.2:9898/icon/1",
                           [&](bool o, const QString &r) {
                ok = o;
                reply = r;
                done();
            });
        });
        CHECK(ok);
        CHECK(reply.contains("success"));
        CHECK(mock.lastBody.contains("\"packages\":[\"http://192.168.1.2:9898/pkg/1\"]"));
        CHECK(mock.lastBody.contains("\"name\":\"My Game\""));
    }

    {
        bool ok = false;
        waitCallback([&](auto done) {
            client.pull("http://192.168.1.2:9898/pkg/2",
                        "/data/homebrew/dump.exfat", false, [&](bool o, const QString &) {
                ok = o;
                done();
            });
        });
        CHECK(ok);
        CHECK(mock.lastBody.contains("\"mode\":\"overwrite\""));
        CHECK(mock.lastBody.contains("/data/homebrew/dump.exfat"));
    }

    {
        bool ok = false;
        waitCallback([&](auto done) {
            client.pullPause(true, [&](bool o, const QString &) {
                ok = o;
                done();
            });
        });
        CHECK(ok);
        CHECK(mock.lastBody.contains("\"paused\":1"));
    }

    {
        bool ok = false;
        waitCallback([&](auto done) {
            client.pullCancel([&](bool o, const QString &) {
                ok = o;
                done();
            });
        });
        CHECK(ok);
    }

    {
        bool ok = false;
        bool exists = false;
        qint64 size = -1;
        waitCallback([&](auto done) {
            client.stat("/data/homebrew/dump.exfat", [&](bool o, bool e, qint64 s) {
                ok = o;
                exists = e;
                size = s;
                done();
            });
        });
        CHECK(ok);
        CHECK(exists);
        CHECK(size == 400);
    }

    {
        ps5::ReceiverStatus st;
        waitCallback([&](auto done) {
            client.status([&](const ps5::ReceiverStatus &s) {
                st = s;
                done();
            });
        });
        CHECK(st.online);
    }

    if (failures == 0) {
        std::fprintf(stderr, "receiver client tests passed\n");
        return 0;
    }
    return 1;
}

#include "receivertest.moc"
