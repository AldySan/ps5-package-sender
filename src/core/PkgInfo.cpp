#include "PkgInfo.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <optional>

namespace ps5 {
namespace {

constexpr quint32 CntHeaderSize = 0x5A0;
constexpr quint32 EntrySize = 0x20;
constexpr quint32 NameTableId = 0x0200;
constexpr quint32 SfoId = 0x1000;
constexpr quint32 ParamJsonId = 0x2000;
constexpr quint32 IconId = 0x1200;

quint32 be32(const QByteArray &b, qsizetype off)
{
    return (quint32(quint8(b[off])) << 24) | (quint32(quint8(b[off + 1])) << 16)
        | (quint32(quint8(b[off + 2])) << 8) | quint8(b[off + 3]);
}

quint32 le32(const QByteArray &b, qsizetype off)
{
    return quint8(b[off]) | (quint8(b[off + 1]) << 8) | (quint8(b[off + 2]) << 16)
        | (quint32(quint8(b[off + 3])) << 24);
}

quint64 le64(const QByteArray &b, qsizetype off)
{
    quint64 v = 0;
    for (int i = 7; i >= 0; i--)
        v = (v << 8) | quint8(b[off + i]);
    return v;
}

bool hasMagic(const QByteArray &b, qsizetype off, const char magic[4])
{
    return quint8(b[off]) == 0x7F && b[off + 1] == magic[0] && b[off + 2] == magic[1]
        && b[off + 3] == magic[2];
}

struct Entry {
    quint32 id = 0;
    quint32 nameOff = 0;
    quint32 flags = 0;
    quint32 dataOff = 0;
    quint32 dataSize = 0;
    QString name;
};

struct Container {
    qint64 cntBase = 0;
    bool isBackport = false;
    bool isDebug = false;
    bool isMeta = false;
    QString contentId;
    QString digest;
    QList<Entry> entries;

    const Entry *find(quint32 id) const
    {
        for (int pass = 0; pass < 2; pass++)
            for (const Entry &e : entries)
                if (e.id == id && (pass == 1 || !(e.flags & 0x80000000)))
                    return &e;
        return nullptr;
    }

    const Entry *findEntry(quint32 id, const char *name) const
    {
        for (int pass = 0; pass < 2; pass++)
            for (const Entry &e : entries)
                if (e.id == id && (pass == 1 || !(e.flags & 0x80000000)))
                    return &e;
        for (int pass = 0; pass < 2; pass++)
            for (const Entry &e : entries)
                if ((pass == 1 || !(e.flags & 0x80000000))
                    && e.name.compare(QLatin1String(name), Qt::CaseInsensitive) == 0)
                    return &e;
        return nullptr;
    }
};

struct SfoEntry {
    QString name;
    quint16 fmt = 0;
    QByteArray data;
};

QList<SfoEntry> parseSfo(const QByteArray &b)
{
    QList<SfoEntry> out;
    if (b.size() < 0x14 || quint8(b[0]) != 0 || b[1] != 'P' || b[2] != 'S' || b[3] != 'F')
        return out;
    const quint32 keyTab = le32(b, 8);
    const quint32 dataTab = le32(b, 12);
    const quint32 count = le32(b, 16);
    if (count > 4096)
        return out;
    for (quint32 k = 0; k < count; k++) {
        const qsizetype o = 0x14 + qsizetype(k) * 0x10;
        if (o + 16 > b.size())
            return out;
        const quint32 keyOff = quint8(b[o]) | (quint8(b[o + 1]) << 8);
        const quint16 fmt = quint8(b[o + 2]) | (quint8(b[o + 3]) << 8);
        const quint32 len = le32(b, o + 4);
        const quint32 dataOff = le32(b, o + 12);
        SfoEntry e;
        e.fmt = fmt;
        qsizetype ks = keyTab + keyOff;
        if (ks >= 0 && ks < b.size()) {
            qsizetype ke = ks;
            while (ke < b.size() && b[ke] != 0)
                ke++;
            e.name = QString::fromLatin1(b.constData() + ks, int(ke - ks));
        }
        qsizetype ds = qsizetype(dataTab) + dataOff;
        if (ds >= 0 && ds <= b.size()) {
            const qsizetype avail = b.size() - ds;
            e.data = b.mid(ds, qMin<qsizetype>(avail, fmt == 0x404 ? 4 : qsizetype(len)));
        }
        out.append(e);
    }
    return out;
}

QString sfoString(const QList<SfoEntry> &sfo, const char *name)
{
    for (const SfoEntry &e : sfo)
        if (e.name == QLatin1String(name))
            return QString::fromUtf8(e.data.constData(), int(qstrlen(e.data.constData())));
    return {};
}

QString titleIdFromContentId(const QString &contentId)
{
    const QStringList parts = contentId.split(u'-');
    QString mid = parts.size() >= 2 ? parts.at(1) : contentId;
    const int us = mid.indexOf(u'_');
    if (us > 0)
        mid = mid.left(us);
    if (mid.size() >= 4 && mid.size() <= 16)
        return mid;
    return {};
}

QByteArray readEntry(QFile &file, const Container &cnt, const Entry &e, qsizetype cap)
{
    if (e.dataSize == 0 || qsizetype(e.dataSize) > cap)
        return {};
    if (!file.seek(cnt.cntBase + e.dataOff))
        return {};
    return file.read(e.dataSize);
}

bool looksLikeDlc(const QString &title, const QString &contentId)
{
    const QString t = title.toLower();
    const QString c = contentId.toLower();
    return t.contains(QStringLiteral("dlc")) || t.contains(QStringLiteral("add-on"))
        || t.contains(QStringLiteral("addon")) || t.contains(QStringLiteral("expansion"))
        || t.contains(QStringLiteral("season pass")) || c.contains(QLatin1String("-dlc"))
        || c.contains(QLatin1String("_dlc")) || c.contains(QStringLiteral("addon"));
}

std::optional<Container> openContainer(QFile &file)
{
    const qint64 size = file.size();
    if (size < CntHeaderSize)
        return {};
    QByteArray head = file.read(0x60);
    if (head.size() < 0x60)
        return {};

    qint64 cntBase = -1;
    bool isBackport = false, isDebug = false, isMeta = false;
    if (hasMagic(head, 0, "FIH")) {
        isDebug = quint8(head[0x05]) == 0;
        const quint64 emb = le64(head, 0x58);
        if (emb == 0 || emb > quint64(size) - CntHeaderSize)
            return {};
        cntBase = qint64(emb);
    } else if (hasMagic(head, 0, "LIH")) {
        isDebug = quint8(head[0x05]) == 0;
        isBackport = true;
        const quint64 emb = le64(head, 0x30);
        if (emb == 0 || emb > quint64(size) - CntHeaderSize)
            return {};
        cntBase = qint64(emb);
    } else if (hasMagic(head, 0, "CNT")) {
        cntBase = 0;
        isDebug = true;
        isMeta = true;
    } else {
        return {};
    }

    if (!file.seek(cntBase))
        return {};
    const QByteArray hdr = file.read(CntHeaderSize);
    if (hdr.size() < CntHeaderSize || !hasMagic(hdr, 0, "CNT"))
        return {};

    Container cnt;
    cnt.cntBase = cntBase;
    cnt.isBackport = isBackport;
    cnt.isDebug = isDebug;
    cnt.isMeta = isMeta;

    {
        qsizetype end = 0x40;
        for (int i = 0; i < 0x30; i++)
            if (hdr.at(0x40 + i) == 0) {
                end = 0x40 + i;
                break;
            }
        cnt.contentId = QString::fromLatin1(hdr.constData() + 0x40, int(end - 0x40)).trimmed();
    }
    if (cntBase + 0xFE0 + 32 <= size && file.seek(cntBase + 0xFE0)) {
        const QByteArray d = file.read(32);
        if (d.size() == 32)
            cnt.digest = QString::fromLatin1(d.toHex()).toUpper();
    }

    const quint32 count = be32(hdr, 0x10);
    const quint32 tableOff = be32(hdr, 0x18);
    if (count == 0 || count > 0x10000)
        return {};
    if (!file.seek(cntBase + tableOff))
        return {};
    const QByteArray table = file.read(qint64(count) * EntrySize);
    if (table.size() < qint64(count) * EntrySize)
        return {};

    QList<Entry> raw;
    raw.reserve(int(count));
    for (quint32 i = 0; i < count; i++) {
        const qsizetype o = qsizetype(i) * EntrySize;
        Entry e;
        e.id = be32(table, o);
        e.nameOff = be32(table, o + 4);
        e.flags = be32(table, o + 8);
        e.dataOff = be32(table, o + 0x10);
        e.dataSize = be32(table, o + 0x14);
        raw.append(e);
    }

    QHash<quint32, QString> names;
    for (const Entry &e : raw) {
        if (e.id != NameTableId || e.dataSize == 0 || qsizetype(e.dataSize) > 4 * 1024 * 1024)
            continue;
        if (!file.seek(cnt.cntBase + e.dataOff))
            continue;
        const QByteArray nb = file.read(e.dataSize);
        if (nb.size() != int(e.dataSize))
            continue;
        qsizetype s = 0;
        for (qsizetype i = 0; i <= nb.size(); i++) {
            if (i == nb.size() || nb.at(i) == 0) {
                if (i > s)
                    names.insert(quint32(s), QString::fromLatin1(nb.constData() + s, int(i - s)));
                s = i + 1;
            }
        }
    }
    for (Entry &e : raw) {
        e.name = names.value(e.nameOff);
        cnt.entries.append(e);
    }
    return cnt;
}

QByteArray findIcon(QFile &file, const Container &cnt)
{
    const auto tryEntry = [&](const Entry &e) -> QByteArray {
        const QByteArray b = readEntry(file, cnt, e, 8 * 1024 * 1024);
        if (b.size() >= 8 && quint8(b[0]) == 0x89 && b[1] == 'P')
            return b;
        return {};
    };
    for (int pass = 0; pass < 2; pass++)
        for (const Entry &e : cnt.entries)
            if (e.id == IconId && (pass == 1 || !(e.flags & 0x80000000))) {
                const QByteArray hit = tryEntry(e);
                if (!hit.isEmpty())
                    return hit;
            }
    for (int pass = 0; pass < 2; pass++)
        for (const Entry &e : cnt.entries)
            if (e.id >= 0x1201 && e.id <= 0x1220 && (pass == 1 || !(e.flags & 0x80000000))) {
                const QByteArray hit = tryEntry(e);
                if (!hit.isEmpty())
                    return hit;
            }
    return {};
}

} // namespace

namespace PkgReader {

bool isLibraryFile(const QString &suffix)
{
    const QString s = suffix.toLower();
    return s == QLatin1String("pkg") || s == QLatin1String("fpkg")
        || s == QLatin1String("exfat") || s == QLatin1String("ffpkg")
        || s == QLatin1String("ffpfsc") || s == QLatin1String("ffpfs");
}

bool isImageFile(const QString &suffix)
{
    const QString s = suffix.toLower();
    return s == QLatin1String("exfat") || s == QLatin1String("ffpkg")
        || s == QLatin1String("ffpfsc") || s == QLatin1String("ffpfs");
}

QString titleIdFromName(const QString &fileName)
{
    static const QRegularExpression re(
        QStringLiteral("\\b(PPSA\\d{4}|PPSA\\d{5}|CUSA\\d{5}|NPXS\\d{5}|[A-Z]{4}\\d{5})\\b"));
    const QRegularExpressionMatch m = re.match(fileName.toUpper());
    return m.hasMatch() ? m.captured(1) : QString();
}

PkgData read(const QString &path)
{
    const QFileInfo fi(path);
    PkgData out;
    out.size = fi.size();
    out.format = fi.suffix().toLower();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return out;

    if (isImageFile(out.format)) {
        out.titleId = titleIdFromName(fi.completeBaseName());
        out.title = fi.completeBaseName();
        out.platform = QStringLiteral("PS5");
        return out;
    }

    const std::optional<Container> cnt = openContainer(file);
    if (!cnt || cnt->contentId.isEmpty())
        return out;

    if (const Entry *sfo = cnt->findEntry(SfoId, "param.sfo")) {
        const QByteArray sfoBytes = readEntry(file, *cnt, *sfo, 1024 * 1024);
        const QList<SfoEntry> sfoEntries = parseSfo(sfoBytes);
        const QString title = sfoString(sfoEntries, "TITLE");
        const QString contentId = sfoString(sfoEntries, "CONTENT_ID");
        if (!title.isEmpty() || !contentId.isEmpty()) {
            out.title = title;
            out.contentId = contentId;
            out.titleId = sfoString(sfoEntries, "TITLE_ID");
            out.category = sfoString(sfoEntries, "CATEGORY");
            out.version = sfoString(sfoEntries, "APP_VER");
            if (out.version.isEmpty())
                out.version = sfoString(sfoEntries, "VERSION");
            out.platform = QStringLiteral("PS4");
            out.isDlc = out.category.compare(QLatin1String("ac"), Qt::CaseInsensitive) == 0;
            out.digest = cnt->digest;
            out.icon = findIcon(file, *cnt);
            if (out.titleId.isEmpty())
                out.titleId = titleIdFromContentId(out.contentId);
            return out;
        }
    }

    if (const Entry *pj = cnt->findEntry(ParamJsonId, "param.json")) {
        const QByteArray pjBytes = readEntry(file, *cnt, *pj, 2 * 1024 * 1024);
        const QJsonDocument doc = QJsonDocument::fromJson(pjBytes);
        if (!doc.isNull()) {
            out.contentId = cnt->contentId;
            out.titleId = titleIdFromContentId(out.contentId);
            out.platform = QStringLiteral("PS5");
            out.digest = cnt->digest;
            out.icon = findIcon(file, *cnt);
            out.isBackport = cnt->isBackport;

            const QJsonObject root = doc.object();
            out.version = root.value(QStringLiteral("contentVersion")).toString();
            const QJsonObject lp = root.value(QStringLiteral("localizedParameters")).toObject();
            QString lang = lp.value(QStringLiteral("defaultLanguage")).toString(
                QStringLiteral("en-US"));
            out.title = lp.value(lang).toObject().value(QStringLiteral("titleName")).toString();
            if (out.title.isEmpty()) {
                for (const QString &key : lp.keys()) {
                    const QString t = lp.value(key).toObject().value(
                        QStringLiteral("titleName")).toString();
                    if (!t.isEmpty()) {
                        out.title = t;
                        break;
                    }
                }
            }
            if (out.title.isEmpty())
                out.title = out.titleId.isEmpty() ? out.contentId : out.titleId;
            out.isDlc = looksLikeDlc(out.title, out.contentId);
            return out;
        }
    }

    if (!cnt->contentId.isEmpty()) {
        out.contentId = cnt->contentId;
        out.titleId = titleIdFromContentId(out.contentId);
        out.platform = QStringLiteral("PS5");
        out.title = out.titleId.isEmpty() ? out.contentId : out.titleId;
        out.digest = cnt->digest;
    }
    return out;
}

}

}
