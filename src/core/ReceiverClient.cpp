#include "ReceiverClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QVariant>

namespace ps5 {

namespace {

QNetworkAccessManager &manager()
{
    static QNetworkAccessManager nam;
    return nam;
}

} // namespace

ReceiverClient::ReceiverClient(QObject *parent)
    : QObject(parent)
{
}

void ReceiverClient::setConsoleIp(const QString &ip)
{
    if (m_ip == ip)
        return;
    m_ip = ip;
}

QUrl ReceiverClient::baseUrl() const
{
    return QUrl(QStringLiteral("http://%1:%2/")
                    .arg(m_ip.isEmpty() ? QStringLiteral("127.0.0.1") : m_ip)
                    .arg(Port));
}

void ReceiverClient::get(const QString &path, int timeoutMs,
                         const std::function<void(const Reply &)> &cb) const
{
    QNetworkRequest req(baseUrl().resolved(QUrl(path)));
    req.setTransferTimeout(timeoutMs);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = manager().get(req);
    connect(reply, &QNetworkReply::finished, reply, [reply, cb] {
        Reply out;
        out.httpStatus = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        out.ok = out.httpStatus >= 200 && out.httpStatus < 300;
        out.body = QString::fromUtf8(reply->readAll());
        reply->deleteLater();
        cb(out);
    });
}

void ReceiverClient::post(const QString &path, const QByteArray &json, int timeoutMs,
                          const std::function<void(const Reply &)> &cb) const
{
    QNetworkRequest req(baseUrl().resolved(QUrl(path)));
    req.setTransferTimeout(timeoutMs);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = manager().post(req, json);
    connect(reply, &QNetworkReply::finished, reply, [reply, cb] {
        Reply out;
        out.httpStatus = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        out.ok = out.httpStatus >= 200 && out.httpStatus < 300;
        out.body = QString::fromUtf8(reply->readAll());
        reply->deleteLater();
        cb(out);
    });
}

QString ReceiverClient::jsonEscape(const QString &s)
{
    QString out;
    for (const QChar c : s) {
        if (c == QLatin1Char('\\'))
            out += QStringLiteral("\\\\");
        else if (c == QLatin1Char('"'))
            out += QStringLiteral("\\\"");
        else if (c == QLatin1Char('\r') || c == QLatin1Char('\n'))
            out += QLatin1Char(' ');
        else if (c.unicode() < 0x20)
            out += QStringLiteral("\\u%1").arg(c.unicode(), 4, 16, QLatin1Char('0'));
        else
            out += c;
    }
    return out;
}

void ReceiverClient::probe(const BoolCallback &cb) const
{
    get(QStringLiteral("/api"), 4000, [cb](const Reply &r) {
        cb(r.ok, r.body);
    });
}

void ReceiverClient::status(const StatusCallback &cb) const
{
    get(QStringLiteral("/api/status"), 10000, [cb](const Reply &r) {
        ReceiverStatus out;
        if (!r.ok)
            return cb(out);
        const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
        if (!doc.isObject())
            return cb(out);
        const QJsonObject o = doc.object();
        out.online = true;
        out.busy = o.value(QStringLiteral("busy")).toBool();
        out.active = o.value(QStringLiteral("active")).toInt();
        out.pull = o.value(QStringLiteral("pull")).toBool();
        out.pullName = o.value(QStringLiteral("pullName")).toString();
        out.pullGot = qint64(o.value(QStringLiteral("pullGot")).toDouble());
        out.pullWant = qint64(o.value(QStringLiteral("pullWant")).toDouble());
        out.pullPaused = o.value(QStringLiteral("pullPaused")).toBool();
        const QString lastUrl = o.value(QStringLiteral("lastUrl")).toString();
        if (!lastUrl.isEmpty()) {
            out.hasLast = true;
            out.lastUrl = lastUrl;
            out.lastOk = o.value(QStringLiteral("lastOk")).toBool();
            out.lastErr = o.value(QStringLiteral("lastErr")).toString();
        }
        cb(out);
    });
}

void ReceiverClient::space(const SpaceCallback &cb) const
{
    get(QStringLiteral("/api/space"), 10000, [cb](const Reply &r) {
        ReceiverSpace out;
        if (!r.ok)
            return cb(out);
        const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
        if (!doc.isObject())
            return cb(out);
        out.online = true;
        out.free = qint64(doc.object().value(QStringLiteral("free")).toDouble());
        out.total = qint64(doc.object().value(QStringLiteral("total")).toDouble());
        cb(out);
    });
}

void ReceiverClient::version(const VersionCallback &cb) const
{
    get(QStringLiteral("/api/version"), 5000, [cb](const Reply &r) {
        if (!r.ok)
            return cb(false, QString());
        const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
        const QString build = doc.object().value(QStringLiteral("build")).toString();
        cb(!build.isEmpty(), build);
    });
}

void ReceiverClient::install(const QString &pkgUrl, const QString &name,
                             const QString &iconUrl, const BoolCallback &cb) const
{
    QString json = QStringLiteral("{\"type\":\"direct\",\"packages\":[\"%1\"]")
                       .arg(jsonEscape(pkgUrl));
    if (!name.isEmpty())
        json += QStringLiteral(",\"name\":\"%1\"").arg(jsonEscape(name));
    if (!iconUrl.isEmpty())
        json += QStringLiteral(",\"icon_url\":\"%1\"").arg(jsonEscape(iconUrl));
    json += QLatin1Char('}');
    post(QStringLiteral("/api/install"), json.toUtf8(), 15000,
         [cb](const Reply &r) {
             if (!r.ok)
                 return cb(false, r.body.isEmpty()
                     ? QStringLiteral("no reply from console") : r.body);
             const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
             const QString st = doc.object().value(QStringLiteral("status")).toString();
             cb(st == QLatin1String("success"), r.body);
         });
}

void ReceiverClient::pull(const QString &url, const QString &remotePath, bool resume,
                          const BoolCallback &cb) const
{
    const QString json =
        QStringLiteral("{\"url\":\"%1\",\"path\":\"%2\",\"mode\":\"%3\"}")
            .arg(jsonEscape(url), jsonEscape(remotePath),
                 resume ? QStringLiteral("resume") : QStringLiteral("overwrite"));
    post(QStringLiteral("/api/files/pull"), json.toUtf8(), 15000, [cb](const Reply &r) {
        if (!r.ok)
            return cb(false, r.body.isEmpty()
                ? QStringLiteral("no reply from console") : r.body);
        const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
        cb(doc.object().value(QStringLiteral("ok")).toBool()
            || doc.object().value(QStringLiteral("started")).toBool(), r.body);
    });
}

void ReceiverClient::pullPause(bool paused, const BoolCallback &cb) const
{
    const QByteArray json =
        QStringLiteral("{\"paused\":%1}").arg(paused ? 1 : 0).toUtf8();
    post(QStringLiteral("/api/pull/pause"), json, 10000, [cb](const Reply &r) {
        const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
        cb(r.ok && doc.object().value(QStringLiteral("ok")).toBool(), r.body);
    });
}

void ReceiverClient::pullCancel(const BoolCallback &cb) const
{
    post(QStringLiteral("/api/pull/cancel"), QByteArrayLiteral("{}"), 10000,
         [cb](const Reply &r) {
             const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
             cb(r.ok && doc.object().value(QStringLiteral("ok")).toBool(), r.body);
         });
}

void ReceiverClient::stat(const QString &remotePath, const StatCallback &cb) const
{
    get(QStringLiteral("/api/files/stat?path=%1")
            .arg(QString::fromUtf8(
                QUrl::toPercentEncoding(remotePath, {}, "/"))),
        10000, [cb](const Reply &r) {
            if (!r.ok)
                return cb(false, false, -1);
            const QJsonDocument doc = QJsonDocument::fromJson(r.body.toUtf8());
            const QJsonObject o = doc.object();
            cb(true, o.value(QStringLiteral("exists")).toBool(),
               qint64(o.value(QStringLiteral("size")).toDouble()));
        });
}

}
