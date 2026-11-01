#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
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

    {
        QNetworkReply *r = syncGet(nam, QUrl(base + "/pkg/17"), "bytes=-5");
        CHECK(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200);
        CHECK(r->readAll() == payload);
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

    server.revoke(QStringLiteral("17"));
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
