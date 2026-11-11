#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

#include "../src/core/RangeFileServer.h"

#include <cstdio>

static int failures = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

static QNetworkReply *syncGet(QNetworkAccessManager &nam, const QUrl &url,
                               const QByteArray &range = {})
{
    QEventLoop loop;
    QNetworkRequest req(url);
    if (!range.isEmpty())
        req.setRawHeader(QByteArrayLiteral("Range"), range);
    QNetworkReply *reply = nam.get(req);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    return reply;
}

struct RawResponse {
    int status = 0;
    QByteArray headers;
    QByteArray body;
};

// Runs the application event loop for a moment. The server lives in this
// same process and thread, so a raw waitForReadyRead() that blocks without
// pumping events starves it: the connection is accepted by the kernel (the
// connect succeeds) but no ServerConnection is ever created and no reply
// ever comes back.
static void pumpEvents(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

// Reads exactly one HTTP response over a raw socket. Every reply this server
// sends carries Content-Length, so the body end is known without a close.
static bool readOneResponse(QTcpSocket &sock, RawResponse *out)
{
    QByteArray buf;
    QElapsedTimer timer;
    timer.start();
    int headEnd = -1;
    while (timer.elapsed() < 5000) {
        buf += sock.readAll();
        headEnd = buf.indexOf("\r\n\r\n");
        if (headEnd >= 0)
            break;
        if (sock.state() != QAbstractSocket::ConnectedState)
            return false;
        pumpEvents(25); // feeds the in-process server, not just this socket
    }
    if (headEnd < 0)
        return false;
    const QByteArray head = buf.left(headEnd);
    qint64 length = -1;
    for (const QByteArray &line : head.split('\n')) {
        const int colon = line.indexOf(':');
        if (colon > 0 && line.left(colon).trimmed().compare(
                QByteArrayLiteral("content-length"), Qt::CaseInsensitive) == 0)
            length = line.mid(colon + 1).trimmed().toLongLong();
    }
    if (length < 0)
        return false;
    while (buf.size() < headEnd + 4 + int(length) && timer.elapsed() < 15000) {
        if (sock.state() != QAbstractSocket::ConnectedState)
            return false;
        pumpEvents(25);
        buf += sock.readAll();
    }
    out->status = head.mid(9, 3).toInt();
    out->headers = head;
    out->body = buf.mid(headEnd + 4, int(length));
    return out->body.size() == int(length);
}

// True once the server has closed its side of the connection.
static bool serverClosedConnection(QTcpSocket &sock)
{
    for (int i = 0; i < 20; i++) {
        if (sock.state() != QAbstractSocket::ConnectedState)
            return true;
        pumpEvents(25);
        if (!sock.readAll().isEmpty())
            return false; // unsolicited bytes: not a clean close
    }
    return sock.state() != QAbstractSocket::ConnectedState;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTemporaryDir dir;
    QByteArray payload(300000, 0);
    for (int i = 0; i < payload.size(); i++)
        payload[i] = char(i % 251);
    const QString filePath = dir.filePath("big.pkg");
    {
        QFile f(filePath);
        f.open(QIODevice::WriteOnly);
        f.write(payload);
        f.close();
    }

    ps5::RangeFileServer server;
    CHECK(server.start());
    CHECK(server.port() == ps5::RangeFileServer::DefaultPort);

    server.registerFile(QStringLiteral("17"), filePath);
    server.setIconProvider([](const QString &id) {
        return id == QLatin1String("17") ? QByteArrayLiteral("PNGDATA")
                                         : QByteArray();
    });
    server.setCatalogProvider([] {
        QJsonArray arr;
        QJsonObject o;
        o.insert(QStringLiteral("id"), QStringLiteral("17"));
        o.insert(QStringLiteral("title"), QStringLiteral("Test Game"));
        arr.append(o);
        return arr;
    });

    QNetworkAccessManager nam;
    const QString base = QStringLiteral("http://127.0.0.1:%1").arg(server.port());

    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"));
        CHECK(r->error() == QNetworkReply::NoError);
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200);
        CHECK(r->rawHeader("Accept-Ranges") == "bytes");
        CHECK(r->readAll() == payload);
        r->deleteLater();
    }
    CHECK(server.servedFor(QStringLiteral("17")) == payload.size());

    server.resetServed(QStringLiteral("17"));
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"), "bytes=10-19");
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 206);
        CHECK(r->rawHeader("Content-Range") == "bytes 10-19/300000");
        CHECK(r->readAll() == payload.mid(10, 10));
        r->deleteLater();
    }

    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"), "bytes=299990-");
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 206);
        CHECK(r->readAll() == payload.mid(299990));
        r->deleteLater();
    }

    // Sony's downloader asks for whole 64 KiB blocks; the last block of a
    // package whose size is not a multiple of 64 KiB arrives with an end
    // past the file and must be clamped, not answered with a full body.
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"), "bytes=294912-360447");
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 206);
        CHECK(r->rawHeader("Content-Range") == "bytes 294912-299999/300000");
        CHECK(r->readAll() == payload.mid(294912));
        r->deleteLater();
    }

    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"), "bytes=299990-400000");
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 206);
        CHECK(r->rawHeader("Content-Range") == "bytes 299990-299999/300000");
        CHECK(r->readAll() == payload.mid(299990));
        r->deleteLater();
    }

    // suffix range: the last N bytes
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"), "bytes=-5");
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 206);
        CHECK(r->rawHeader("Content-Range") == "bytes 299995-299999/300000");
        CHECK(r->readAll() == payload.right(5));
        r->deleteLater();
    }

    // a suffix longer than the file is the whole file, still 206
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"), "bytes=-999999");
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 206);
        CHECK(r->rawHeader("Content-Range") == "bytes 0-299999/300000");
        CHECK(r->readAll() == payload);
        r->deleteLater();
    }

    // start past the end of the file: 416 that names the real size
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"), "bytes=300000-");
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 416);
        CHECK(r->rawHeader("Content-Range") == "bytes */300000");
        CHECK(r->rawHeader("Accept-Ranges") == "bytes");
        r->deleteLater();
    }

    // reversed range is unsatisfiable too
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"), "bytes=100-50");
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 416);
        r->deleteLater();
    }

    // multi-range is not supported: the whole file, as a plain 200
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"), "bytes=0-9,20-29");
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200);
        CHECK(r->readAll() == payload);
        r->deleteLater();
    }

    // the console's downloader only advances against a cacheable response:
    // a stable validator pair on every package answer
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"), "bytes=10-19");
        CHECK(r->rawHeader("Last-Modified") == "Wed, 01 Jan 2025 00:00:00 GMT");
        CHECK(r->rawHeader("Date").endsWith("GMT"));
        r->deleteLater();
    }

    // Sony's downloader (ShellCore) appends "?product=...&serverIpAddr=...&r=..."
    // to every path it requests; those must route instead of 404ing (the 404
    // is what the console reports as the PC having stopped serving).
    {
        QNetworkReply *r = syncGet(nam, QUrl(base +
            "/pkg/17?product=PKG.SEND.PKG&serverIpAddr=192.168.1.5&r=0123456789"));
        CHECK(r->error() == QNetworkReply::NoError);
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200);
        CHECK(r->readAll() == payload);
        r->deleteLater();
    }
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/icon/17?product=PKG.SEND.PKG"));
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200);
        CHECK(r->readAll() == "PNGDATA");
        r->deleteLater();
    }
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/catalog?product=PKG.SEND.PKG"));
        CHECK(r->error() == QNetworkReply::NoError);
        const QJsonDocument doc = QJsonDocument::fromJson(r->readAll());
        CHECK(doc.isArray());
        CHECK(doc.array().size() == 1);
        r->deleteLater();
    }
    // Sony probes with single-byte ranges — with a query attached.
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17?product=PKG.SEND.PKG"),
                                   "bytes=0-0");
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 206);
        CHECK(r->rawHeader("Content-Range") == "bytes 0-0/300000");
        CHECK(r->readAll() == payload.left(1));
        r->deleteLater();
    }

    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/icon/17"));
        CHECK(r->error() == QNetworkReply::NoError);
        CHECK(r->header(QNetworkRequest::ContentTypeHeader).toString()
              == "image/png");
        CHECK(r->readAll() == "PNGDATA");
        r->deleteLater();
    }

    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/icon/99"));
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404);
        r->deleteLater();
    }

    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/catalog"));
        CHECK(r->error() == QNetworkReply::NoError);
        const QJsonDocument doc = QJsonDocument::fromJson(r->readAll());
        CHECK(doc.isArray());
        CHECK(doc.array().size() == 1);
        CHECK(doc.array().at(0).toObject().value("title").toString()
              == "Test Game");
        r->deleteLater();
    }

    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/unknown"));
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404);
        r->deleteLater();
    }

    // Sony's downloader asks for keep-alive on every request and expects the
    // connection to carry the next one; a "Connection: close" request must
    // still end it.
    {
        QTcpSocket sock;
        sock.connectToHost(QStringLiteral("127.0.0.1"), quint16(server.port()));
        CHECK(sock.waitForConnected(5000));

        sock.write("GET /pkg/17 HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                   "Connection: keep-alive\r\n\r\n");
        CHECK(sock.waitForBytesWritten(5000));
        RawResponse r1;
        CHECK(readOneResponse(sock, &r1));
        CHECK(r1.status == 200);
        CHECK(r1.body == payload);
        CHECK(r1.headers.contains("Connection: keep-alive"));
        CHECK(sock.state() == QAbstractSocket::ConnectedState);

        // HTTP/1.1 defaults to keep-alive, and Sony's query strings must ride
        // the reused connection too.
        sock.write("GET /pkg/17?product=PKG.SEND.PKG&serverIpAddr=192.168.1.5&r=1"
                   " HTTP/1.1\r\nHost: 127.0.0.1\r\nRange: bytes=0-0\r\n\r\n");
        CHECK(sock.waitForBytesWritten(5000));
        RawResponse r2;
        CHECK(readOneResponse(sock, &r2));
        CHECK(r2.status == 206);
        CHECK(r2.body == payload.left(1));
        CHECK(sock.state() == QAbstractSocket::ConnectedState);

        // a 416 ends the exchange, not the connection: the next request on
        // the same socket must still be served
        sock.write("GET /pkg/17 HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                   "Range: bytes=400000-400009\r\n\r\n");
        CHECK(sock.waitForBytesWritten(5000));
        RawResponse r416;
        CHECK(readOneResponse(sock, &r416));
        CHECK(r416.status == 416);
        CHECK(r416.headers.contains("Content-Range: bytes */300000"));
        CHECK(sock.state() == QAbstractSocket::ConnectedState);

        sock.write("GET /pkg/17 HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                   "Range: bytes=294912-360447\r\n\r\n");
        CHECK(sock.waitForBytesWritten(5000));
        RawResponse rclamp;
        CHECK(readOneResponse(sock, &rclamp));
        CHECK(rclamp.status == 206);
        CHECK(rclamp.body == payload.mid(294912));
        CHECK(rclamp.headers.contains("Last-Modified: Wed, 01 Jan 2025 00:00:00 GMT"));

        sock.write("GET /pkg/17 HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                   "Connection: close\r\n\r\n");
        CHECK(sock.waitForBytesWritten(5000));
        RawResponse r3;
        CHECK(readOneResponse(sock, &r3));
        CHECK(r3.status == 200);
        CHECK(r3.headers.contains("Connection: close"));
        CHECK(serverClosedConnection(sock));
    }

    // A client may pipeline: hundreds of requests stacked in one write must
    // all be answered on the same connection, iteratively rather than
    // through one recursive call per request.
    {
        QTcpSocket sock;
        sock.connectToHost(QStringLiteral("127.0.0.1"), quint16(server.port()));
        CHECK(sock.waitForConnected(5000));

        QByteArray batch;
        for (int i = 0; i < 500; i++)
            batch += "GET /pkg/17 HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                     "Range: bytes=0-0\r\n\r\n";
        sock.write(batch);
        CHECK(sock.waitForBytesWritten(5000));
        int served = 0;
        RawResponse r;
        while (served < 500 && readOneResponse(sock, &r))
            served++;
        CHECK(served == 500);
        CHECK(r.status == 206);
        CHECK(r.body == payload.left(1));
        CHECK(sock.state() == QAbstractSocket::ConnectedState);

        sock.write("GET /pkg/17 HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                   "Connection: close\r\n\r\n");
        CHECK(sock.waitForBytesWritten(5000));
        RawResponse rclose;
        CHECK(readOneResponse(sock, &rclose));
        CHECK(rclose.status == 200);
        CHECK(serverClosedConnection(sock));
    }

    server.revoke(QStringLiteral("17"));
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"));
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404);
        r->deleteLater();
    }

    // forget() drops the registration entirely, served counters included
    server.registerFile(QStringLiteral("17"), filePath);
    server.resetServed(QStringLiteral("17"));
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"));
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200);
        r->deleteLater();
    }
    CHECK(server.servedFor(QStringLiteral("17")) == payload.size());
    server.forget(QStringLiteral("17"));
    CHECK(server.servedFor(QStringLiteral("17")) == 0);
    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"));
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404);
        r->deleteLater();
    }

    CHECK(server.pkgUrl(QStringLiteral("192.168.1.2"), QStringLiteral("17"))
          == QStringLiteral("http://192.168.1.2:9898/pkg/17"));

    if (failures == 0) {
        std::fprintf(stderr, "file server tests passed\n");
        return 0;
    }
    return 1;
}
