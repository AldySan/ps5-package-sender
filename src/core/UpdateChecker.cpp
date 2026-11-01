#include "UpdateChecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace ps5 {

namespace {

QNetworkAccessManager &manager()
{
    static QNetworkAccessManager nam;
    return nam;
}

QString stripTagPrefix(const QString &version)
{
    return version.startsWith(QLatin1Char('v')) ? version.mid(1) : version;
}

} // namespace

UpdateChecker::UpdateChecker(const QString &currentVersion, QObject *parent)
    : QObject(parent), m_currentVersion(stripTagPrefix(currentVersion))
{
}

bool UpdateChecker::isVersionNewer(const QString &latest, const QString &current)
{
    const QList<int> a = [v = stripTagPrefix(latest)] {
        QList<int> parts;
        for (const QString &s : v.split(QLatin1Char('.'), Qt::SkipEmptyParts))
            parts.append(s.toInt());
        return parts;
    }();
    const QList<int> b = [v = stripTagPrefix(current)] {
        QList<int> parts;
        for (const QString &s : v.split(QLatin1Char('.'), Qt::SkipEmptyParts))
            parts.append(s.toInt());
        return parts;
    }();
    const int n = qMax(a.size(), b.size());
    for (int i = 0; i < n; i++) {
        const int x = i < a.size() ? a.at(i) : 0;
        const int y = i < b.size() ? b.at(i) : 0;
        if (x != y)
            return x > y;
    }
    return false;
}

void UpdateChecker::check(const ResultCallback &callback) const
{
    QNetworkRequest req(QUrl(QStringLiteral(
        "https://api.github.com/repos/AldySan/ps5-package-sender/releases/latest")));
    req.setTransferTimeout(15000);
    req.setRawHeader(QByteArrayLiteral("Accept"),
                     QByteArrayLiteral("application/vnd.github+json"));
    req.setRawHeader(QByteArrayLiteral("User-Agent"),
                     QStringLiteral("ps5-package-sender/%1").arg(m_currentVersion).toUtf8());

    QNetworkReply *reply = manager().get(req);
    connect(reply, &QNetworkReply::finished, reply,
            [reply, callback, current = m_currentVersion] {
        Result out;
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        if (status != 200) {
            out.error = QStringLiteral("HTTP %1").arg(status);
            return callback(out);
        }
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        const QJsonObject root = doc.object();
        out.latestVersion = root.value(QStringLiteral("tag_name")).toString();
        out.releaseUrl = root.value(QStringLiteral("html_url")).toString();
        if (out.latestVersion.isEmpty()) {
            out.error = QStringLiteral("no release info");
            return callback(out);
        }
        out.available = isVersionNewer(out.latestVersion, current);
        callback(out);
    });
}

}
