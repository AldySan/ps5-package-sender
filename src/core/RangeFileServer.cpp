#include "RangeFileServer.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QLocale>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

namespace ps5 {

namespace {

constexpr qint64 WriteChunk = 512 * 1024;

// A kept-alive connection that goes quiet (the console's downloader left,
// the app was closed) must not sit in the connection set forever.
constexpr int IdleTimeoutMs = 75000;

// Fixed on purpose: a validator that changes between two range fetches of
// the same install reads as the file being replaced, and a downloader that
// caches only against a stable validator can stall mid-package.
const char PkgLastModified[] = "Wed, 01 Jan 2025 00:00:00 GMT";

const char *statusText(int status)
{
    switch (status) {
    case 200:
        return "OK";
    case 206:
        return "Partial Content";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 416:
        return "Range Not Satisfiable";
    case 431:
        return "Request Header Fields Too Large";
    default:
        return "Bad Request";
    }
}

QByteArray httpDateNow()
{
    return QLocale::c()
        .toString(QDateTime::currentDateTimeUtc(),
                  QStringLiteral("ddd, dd MMM yyyy HH:mm:ss"))
        .toLatin1() + " GMT";
}

enum RangeParse { RangeWholeBody, RangePartial, RangeUnsatisfiable };

// Sony's downloader asks for whole 64 KiB blocks, so the last block of a
// package whose size is not a multiple of 64 KiB arrives with an end past
// the file. That range must be clamped and answered 206; a full-body 200
// (or a bare 416) stalls the console's fetch loop mid-download.
RangeParse parseRange(const QString &header, qint64 size, qint64 *from,
                      qint64 *to)
{
    *from = 0;
    *to = size - 1;
    if (!header.startsWith(QStringLiteral("bytes="), Qt::CaseInsensitive))
        return RangeWholeBody;
    const QString spec = header.mid(6).trimmed();
    if (spec.isEmpty() || spec.contains(QLatin1Char(',')))
        return RangeWholeBody;
    const int dash = spec.indexOf(QLatin1Char('-'));
    if (dash < 0)
        return RangeWholeBody;
    const QString first = spec.left(dash);
    const QString last = spec.mid(dash + 1);
    bool ok = false;
    if (first.isEmpty()) {
        const qint64 n = last.toLongLong(&ok);
        if (!ok || n < 0)
            return RangeWholeBody;
        if (n == 0 || size <= 0)
            return RangeUnsatisfiable;
        *from = size - qMin<qint64>(n, size);
        *to = size - 1;
        return RangePartial;
    }
    const qint64 a = first.toLongLong(&ok);
    if (!ok || a < 0)
        return RangeWholeBody;
    qint64 b = size - 1;
    if (!last.isEmpty()) {
        b = last.toLongLong(&ok);
        if (!ok || b < 0)
            return RangeWholeBody;
    }
    if (a >= size || b < a)
        return RangeUnsatisfiable;
    *from = a;
    *to = qMin<qint64>(b, size - 1);
    return RangePartial;
}

} // namespace

class ServerConnection : public QObject {
    Q_OBJECT

public:
    ServerConnection(RangeFileServer *server, QTcpSocket *socket)
        : QObject(server), m_server(server), m_socket(socket)
    {
        m_socket->setParent(this);
        connect(m_socket, &QTcpSocket::readyRead, this, &ServerConnection::readHeader);
        connect(m_socket, &QTcpSocket::disconnected, this, &QObject::deleteLater);
        connect(m_socket, &QTcpSocket::bytesWritten, this,
                 &ServerConnection::continueWriting);
        m_idle.setSingleShot(true);
        m_idle.setInterval(IdleTimeoutMs);
        connect(&m_idle, &QTimer::timeout, this, [this] { closeNow(); });
        m_idle.start();
    }

private:
    struct Request {
        QString method;
        QString path;
        QString rangeHeader;
        bool keepAlive = false;
    };

    // Sony's downloader (ShellCore) appends "?product=...&serverIpAddr=...&r=..."
    // to every path it requests. Routing must use the bare path: treating the
    // query as part of the package id turns every such request into a 404,
    // which the console reads as "the PC stopped serving this package".
    static QString barePath(const QByteArray &target)
    {
        QByteArray path = target;
        for (const char cut : {'?', '#'}) {
            const int at = path.indexOf(cut);
            if (at >= 0)
                path.truncate(at);
        }
        return QString::fromLatin1(QByteArray::fromPercentEncoding(path));
    }

    // HTTP/1.1 keeps the connection alive unless the client says otherwise;
    // HTTP/1.0 closes it unless the client asks. Sony's downloader asks for
    // keep-alive on every request.
    static bool wantsKeepAlive(const QByteArray &version, const QList<QByteArray> &lines)
    {
        for (const QByteArray &line : lines) {
            const int colon = line.indexOf(':');
            if (colon > 0
                && line.left(colon).compare(QByteArrayLiteral("connection"),
                                            Qt::CaseInsensitive) == 0) {
                const QString value = QString::fromLatin1(line.mid(colon + 1))
                                          .trimmed().toLower();
                if (value.contains(QLatin1String("close")))
                    return false;
                if (value.contains(QLatin1String("keep-alive")))
                    return true;
            }
        }
        return version == QByteArrayLiteral("HTTP/1.1");
    }

    void readHeader()
    {
        // A response body is still streaming on this connection: a request
        // the client pipelined behind it stays buffered until finish()
        // re-runs us — serving it now would reuse m_file mid-body.
        if (m_remaining > 0)
            return;
        m_buffer += m_socket->readAll();
        const int headerEnd = m_buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            if (m_buffer.size() > 16 * 1024) {
                m_keepAlive = false; // a reply cannot help a client this broken
                simpleReply(431, QByteArrayLiteral("request header too large"));
            }
            return;
        }

        Request req;
        const QByteArray headerData = m_buffer.left(headerEnd);
        m_buffer.remove(0, headerEnd + 4); // a pipelined next request stays
        QList<QByteArray> lines;
        int pos = 0;
        while (pos <= headerData.size()) {
            const int nl = headerData.indexOf("\r\n", pos);
            if (nl < 0) {
                if (pos < headerData.size())
                    lines.append(headerData.mid(pos));
                break;
            }
            lines.append(headerData.mid(pos, nl - pos));
            pos = nl + 2;
        }
        const QList<QByteArray> parts = lines.isEmpty()
            ? QList<QByteArray>() : lines.first().split(' ');
        if (parts.size() < 3) {
            m_keepAlive = false;
            simpleReply(400, QByteArrayLiteral("bad request"));
            return;
        }
        req.method = QString::fromLatin1(parts.at(0));
        req.path = barePath(parts.at(1));
        req.keepAlive = wantsKeepAlive(parts.at(2), lines);
        for (const QByteArray &line : lines) {
            const int colon = line.indexOf(':');
            if (colon > 0 && line.left(colon).compare(QByteArrayLiteral("range"),
                                                     Qt::CaseInsensitive) == 0)
                req.rangeHeader = QString::fromLatin1(line.mid(colon + 1)).trimmed();
        }
        m_keepAlive = req.keepAlive;
        m_idle.start(); // this connection is active again
        route(req);
    }

    void route(const Request &req)
    {
        if (req.path == QStringLiteral("/catalog")) {
            if (req.method != QLatin1String("GET")
                && req.method != QLatin1String("HEAD")) {
                simpleReply(405, QByteArrayLiteral("method not allowed"));
                return;
            }
            const QJsonArray catalog =
                m_server->m_catalogProvider ? m_server->m_catalogProvider()
                                            : QJsonArray();
            const QByteArray body =
                QJsonDocument(catalog).toJson(QJsonDocument::Compact);
            writeHead(200, body.size(), "application/json",
                      QByteArrayLiteral("Access-Control-Allow-Origin: *"));
            if (req.method == QLatin1String("GET"))
                m_socket->write(body);
            finish();
            return;
        }

        QString id;
        bool isIcon = false;
        if (req.path.startsWith(QStringLiteral("/icon/"))) {
            id = req.path.mid(6);
            isIcon = true;
        } else if (req.path.startsWith(QStringLiteral("/pkg/"))) {
            id = req.path.mid(5);
        } else {
            simpleReply(404, QByteArrayLiteral("not found"));
            return;
        }

        if (req.method != QLatin1String("GET")
            && req.method != QLatin1String("HEAD")) {
            simpleReply(405, QByteArrayLiteral("method not allowed"));
            return;
        }

        if (isIcon) {
            const QByteArray png = m_server->m_iconProvider
                ? m_server->m_iconProvider(id) : QByteArray();
            if (png.isEmpty()) {
                simpleReply(404, QByteArrayLiteral("no icon"));
                return;
            }
            writeHead(200, png.size(), "image/png",
                      QByteArrayLiteral("Access-Control-Allow-Origin: *"));
            if (req.method == QLatin1String("GET"))
                m_socket->write(png);
            finish();
            return;
        }

        servePkg(req, id);
    }

    void servePkg(const Request &req, const QString &id)
    {
        if (m_server->m_revoked.value(id, false)) {
            simpleReply(404, QByteArrayLiteral("no such package"));
            return;
        }
        const QString path = m_server->m_files.value(id);
        if (path.isEmpty()) {
            simpleReply(404, QByteArrayLiteral("no such package"));
            return;
        }
        m_id = id;
        m_file.setFileName(path);
        if (!m_file.open(QIODevice::ReadOnly)) {
            simpleReply(404, QByteArrayLiteral("cannot open package"));
            return;
        }
        const qint64 size = m_file.size();

        qint64 from = 0;
        qint64 to = 0;
        const RangeParse rp = parseRange(req.rangeHeader, size, &from, &to);
        if (rp == RangeUnsatisfiable) {
            // Content-Range: bytes */<size> is required so the client can
            // re-derive the real size and retry; a bare 416 dead-ends it.
            writeHead(416, 0, "application/octet-stream",
                      QByteArrayLiteral("Content-Range: bytes */") +
                          QByteArray::number(size) +
                          QByteArrayLiteral("\r\nAccept-Ranges: bytes"));
            finish();
            return;
        }
        const bool partial = rp == RangePartial;

        const qint64 length = qMax<qint64>(0, to - from + 1);
        QByteArray extra = "Accept-Ranges: bytes";
        extra += "\r\nDate: " + httpDateNow();
        extra += QByteArrayLiteral("\r\nLast-Modified: ") + PkgLastModified;
        if (partial) {
            extra += "\r\nContent-Range: bytes " + QByteArray::number(from) +
                "-" + QByteArray::number(to) + "/" + QByteArray::number(size);
        }
        writeHead(partial ? 206 : 200, length, "application/octet-stream", extra);
        if (req.method == QLatin1String("HEAD")) {
            finish();
            return;
        }
        m_offset = from;
        m_remaining = length;
        continueWriting();
    }

    void continueWriting()
    {
        if (!m_file.isOpen())
            return;
        m_idle.start(); // a body still moving is not idle
        while (m_remaining > 0 && m_socket->bytesToWrite() < 2 * WriteChunk) {
            if (!m_file.seek(m_offset)) {
                closeNow(); // mid-body: the promised bytes will never come
                return;
            }
            const qint64 want = qMin<qint64>(WriteChunk, m_remaining);
            const QByteArray chunk = m_file.read(want);
            if (chunk.isEmpty()) {
                closeNow();
                return;
            }
            const qint64 written = m_socket->write(chunk);
            if (written <= 0) {
                closeNow();
                return;
            }
            m_offset += written;
            m_remaining -= written;
            if (!m_id.isEmpty())
                m_server->addServed(m_id, written);
        }
        if (m_remaining <= 0 && m_socket->bytesToWrite() == 0)
            finish();
    }

    void finish()
    {
        if (m_file.isOpen())
            m_file.close();
        m_id.clear();
        m_offset = 0;
        m_remaining = 0;
        if (!m_keepAlive) {
            m_idle.stop();
            m_socket->disconnectFromHost();
            if (m_socket->state() == QAbstractSocket::UnconnectedState)
                deleteLater();
            return;
        }
        // The client asked to reuse this connection: the next request is
        // either already buffered (pipelining) or arrives with readyRead.
        // Re-entering readHeader synchronously from here would recurse
        // once per pipelined request, and a client that stacks enough of
        // them can grow that chain past the stack. One queued hop back
        // into the event loop drains the backlog iteratively instead.
        m_idle.start();
        QMetaObject::invokeMethod(this, &ServerConnection::readHeader,
                                  Qt::QueuedConnection);
    }

    // Transport-level failure: the response was cut short or the request was
    // unparseable, so the byte stream can no longer be trusted — close even
    // on a keep-alive connection.
    void closeNow()
    {
        m_keepAlive = false;
        finish();
    }

    void writeHead(int status, qint64 length, const QByteArray &contentType,
                   const QByteArray &extra = {})
    {
        QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + " "
            + statusText(status) + "\r\nContent-Type: " + contentType
            + "\r\nContent-Length: " + QByteArray::number(length);
        if (!extra.isEmpty())
            head += "\r\n" + extra;
        head += "\r\nConnection: ";
        head += m_keepAlive ? "keep-alive" : "close";
        head += "\r\n\r\n";
        m_socket->write(head);
    }

    void simpleReply(int status, const QByteArray &body)
    {
        writeHead(status, body.size(), "text/plain");
        m_socket->write(body);
        finish();
    }

    RangeFileServer *m_server;
    QTcpSocket *m_socket;
    QByteArray m_buffer;
    QFile m_file;
    QString m_id;
    QTimer m_idle;
    bool m_keepAlive = false;
    qint64 m_offset = 0;
    qint64 m_remaining = 0;
};

RangeFileServer::RangeFileServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_listener, &QTcpServer::newConnection, this, [this] {
        while (m_listener.hasPendingConnections())
            new ServerConnection(this, m_listener.nextPendingConnection());
    });
    connect(&m_announceTimer, &QTimer::timeout, this, &RangeFileServer::announceNow);
}

bool RangeFileServer::start()
{
    for (int port = DefaultPort; port < DefaultPort + 10; port++) {
        if (m_listener.listen(QHostAddress::AnyIPv4, quint16(port))) {
            m_port = port;
            return true;
        }
    }
    return false;
}

void RangeFileServer::setCatalogProvider(const CatalogProvider &provider)
{
    m_catalogProvider = provider;
}

void RangeFileServer::setIconProvider(const IconProvider &provider)
{
    m_iconProvider = provider;
}

void RangeFileServer::registerFile(const QString &id, const QString &localPath)
{
    m_files.insert(id, localPath);
    m_revoked.remove(id);
}

void RangeFileServer::revoke(const QString &id)
{
    m_revoked.insert(id, true);
}

void RangeFileServer::forget(const QString &id)
{
    m_files.remove(id);
    m_revoked.remove(id);
    m_served.remove(id);
}

void RangeFileServer::resetServed(const QString &id)
{
    m_served.insert(id, 0);
}

qint64 RangeFileServer::servedFor(const QString &id) const
{
    return m_served.value(id, 0);
}

void RangeFileServer::addServed(const QString &id, qint64 delta)
{
    const qint64 total = m_served.value(id, 0) + delta;
    m_served.insert(id, total);
    emit bytesServed(id, total);
}

QString RangeFileServer::pkgUrl(const QString &host, const QString &id) const
{
    return QStringLiteral("http://%1:%2/pkg/%3")
        .arg(host, QString::number(m_port),
             QString::fromUtf8(QUrl::toPercentEncoding(id)));
}

QString RangeFileServer::iconUrl(const QString &host, const QString &id) const
{
    return QStringLiteral("http://%1:%2/icon/%3")
        .arg(host, QString::number(m_port),
             QString::fromUtf8(QUrl::toPercentEncoding(id)));
}

QString RangeFileServer::catalogUrl(const QString &host) const
{
    return QStringLiteral("http://%1:%2/catalog").arg(host, QString::number(m_port));
}

void RangeFileServer::setAnnouncing(bool on, const QString &pcIp)
{
    m_announceIp = pcIp;
    if (on && !m_announceTimer.isActive()) {
        m_announceTimer.start(3000);
        announceNow();
    } else if (!on && m_announceTimer.isActive()) {
        m_announceTimer.stop();
    }
}

void RangeFileServer::announceNow()
{
    if (m_announceIp.isEmpty() || m_port == 0)
        return;
    const QByteArray msg = QStringLiteral("PKGSENDER-PC %1:%2")
                               .arg(m_announceIp, QString::number(m_port))
                               .toLatin1();
    m_announceSocket.writeDatagram(msg, QHostAddress::Broadcast, PcAnnouncePort);
}

#include "RangeFileServer.moc"

}
