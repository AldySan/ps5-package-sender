#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>

#include "../src/core/PkgInfo.h"

#include <cstdio>

static int failures = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

static void put32le(QByteArray &b, qsizetype off, quint32 v)
{
    for (int i = 0; i < 4; i++)
        b[off + i] = char(v >> (8 * i));
}

static void append32le(QByteArray &b, quint32 v)
{
    for (int i = 0; i < 4; i++)
        b.append(char(v >> (8 * i)));
}

static void append16le(QByteArray &b, quint16 v)
{
    b.append(char(v & 0xFF));
    b.append(char(v >> 8));
}

static void put32be(QByteArray &b, qsizetype off, quint32 v)
{
    b[off] = char(v >> 24);
    b[off + 1] = char(v >> 16);
    b[off + 2] = char(v >> 8);
    b[off + 3] = char(v);
}

static void put64le(QByteArray &b, qsizetype off, quint64 v)
{
    for (int i = 0; i < 8; i++)
        b[off + i] = char(v >> (8 * i));
}

struct PkgLayout {
    QByteArray body;
    quint32 sfoEntryFlags = 0;
    bool ps4 = false;
};

static QByteArray buildCnt(const PkgLayout &layout)
{
    const quint32 cntBase = 0;
    const quint32 entryCount = 4;
    const quint32 tableOff = 0x5A0;
    const quint32 nameTableOff = 0x5A0 + entryCount * 0x20;
    const quint32 nameTableSize = 40;
    const quint32 dataOff = nameTableOff + nameTableSize;

    const QString param = layout.ps4
        ? QStringLiteral("\x00PSF") // replaced below
        : QStringLiteral("{\"contentVersion\":\"1.02\",\"localizedParameters\":"
                         "{\"defaultLanguage\":\"en-US\","
                         "\"en-US\":{\"titleName\":\"Test PS5 Game\"}}}");
    const QByteArray paramData = layout.ps4 ? layout.body : param.toUtf8();
    const QByteArray iconData = QByteArray::fromHex("89504e470d0a1a0a0000000d49484452") + QByteArray(16, 'x');
    const QByteArray digestBytes(32, 0xAB);

    QByteArray pkg;
    const qint64 minSize = 0x1004;
    pkg.resize(int(qMax<qint64>(dataOff, minSize) + paramData.size() + iconData.size()
        + digestBytes.size()));

    pkg[0] = char(0x7F);
    pkg[1] = 'C';
    pkg[2] = 'N';
    pkg[3] = 'T';
    put32be(pkg, 0x10, entryCount);
    put32be(pkg, 0x18, tableOff);
    const QByteArray cid = QByteArrayLiteral("UP0001-PPSA90001_00-TESTGAME00000");
    for (int i = 0; i < cid.size(); i++)
        pkg[0x40 + i] = cid[i];

    const auto entry = [&](qsizetype row, quint32 id, quint32 nameOff, quint32 flags,
                           quint32 off, quint32 size) {
        const qsizetype o = 0x5A0 + row * 0x20;
        put32be(pkg, o, id);
        put32be(pkg, o + 4, nameOff);
        put32be(pkg, o + 8, flags);
        put32be(pkg, o + 0x10, off);
        put32be(pkg, o + 0x14, size);
    };

    const QByteArray names = QByteArray("param.sfo\0param.json\0icon0.png\0names", 36)
        + QByteArray(4, '\0');
    entry(0, 0x0200, 0, 0, nameTableOff, nameTableSize);
    entry(1, 0x1000, 0, layout.sfoEntryFlags, dataOff, quint32(layout.body.size()));
    entry(2, 0x2000, 10, 0, dataOff, quint32(paramData.size()));
    entry(3, 0x1200, 21, 0, dataOff + paramData.size(), quint32(iconData.size()));

    memcpy(pkg.data() + nameTableOff, names.constData(), nameTableSize);
    memcpy(pkg.data() + dataOff, paramData.constData(), paramData.size());
    memcpy(pkg.data() + dataOff + paramData.size(), iconData.constData(), iconData.size());
    memcpy(pkg.data() + 0xFE0, digestBytes.constData(), digestBytes.size());
    return pkg;
}

static QByteArray buildSfo()
{
    struct Row {
        const char *name;
        const char *value;
    };
    const Row rows[] = {
        {"TITLE", "Test PS4 Game"},
        {"CONTENT_ID", "UP0001-CUSA90001_00-TESTGAME00000"},
        {"TITLE_ID", "CUSA90001"},
        {"CATEGORY", "gd"},
        {"APP_VER", "1.03"},
    };
    QByteArray keys, data, idx;
    for (const Row &r : rows) {
        const QByteArray name = QByteArray(r.name) + '\0';
        const QByteArray value = QByteArray(r.value) + '\0';
        append16le(idx, quint16(keys.size()));
        append16le(idx, 0x0204);
        append32le(idx, quint32(value.size()));
        append32le(idx, 0);
        append32le(idx, quint32(data.size()));
        keys += name;
        data += value;
    }
    QByteArray sfo(0x14, 0);
    sfo[1] = 'P';
    sfo[2] = 'S';
    sfo[3] = 'F';
    put32le(sfo, 8, quint32(0x14 + idx.size()));
    put32le(sfo, 12, quint32(0x14 + idx.size() + keys.size()));
    put32le(sfo, 16, quint32(sizeof(rows) / sizeof(rows[0])));
    sfo += idx + keys + data;
    return sfo;
}

int main()
{
    QTemporaryDir dir;

    {
        PkgLayout layout;
        layout.ps4 = false;
        const QByteArray pkg = buildCnt(layout);
        const QString path = dir.filePath("game-ps5.pkg");
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(pkg);
        f.close();
        const ps5::PkgData d = ps5::PkgReader::read(path);
        CHECK(d.title == "Test PS5 Game");
        CHECK(d.contentId == "UP0001-PPSA90001_00-TESTGAME00000");
        CHECK(d.titleId == "PPSA90001");
        CHECK(d.version == "1.02");
        CHECK(d.platform == "PS5");
        CHECK(d.format == "pkg");
        CHECK(!d.isDlc);
        CHECK(d.size == pkg.size());
        CHECK(d.digest.size() == 64);
        CHECK(d.digest == QByteArray("AB").repeated(32));
        CHECK(d.icon.left(4) == QByteArray::fromHex("89504e47"));
    }

    {
        PkgLayout layout;
        layout.ps4 = true;
        layout.body = buildSfo();
        const QByteArray pkg = buildCnt(layout);
        const QString path = dir.filePath("game-ps4.pkg");
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(pkg);
        f.close();
        const ps5::PkgData d = ps5::PkgReader::read(path);
        CHECK(d.title == "Test PS4 Game");
        CHECK(d.titleId == "CUSA90001");
        CHECK(d.version == "1.03");
        CHECK(d.platform == "PS4");
        CHECK(d.category == "gd");
        CHECK(!d.isDlc);
    }

    {
        PkgLayout layout;
        const QByteArray cnt = buildCnt(layout);
        QByteArray fih = QByteArray(0x60, 0);
        fih[0] = char(0x7F);
        fih[1] = 'F';
        fih[2] = 'I';
        fih[3] = 'H';
        fih[5] = char(0x01);
        put64le(fih, 0x58, quint64(0x60));
        const QByteArray wrapped = fih + cnt;
        const QString path = dir.filePath("game-fih.pkg");
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(wrapped);
        f.close();
        const ps5::PkgData d = ps5::PkgReader::read(path);
        CHECK(d.title == "Test PS5 Game");
        CHECK(d.titleId == "PPSA90001");
    }

    {
        PkgLayout layout;
        const QByteArray pkg = buildCnt(layout);
        const QString path = dir.filePath("game-fake.fpkg");
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(pkg);
        f.close();
        const ps5::PkgData d = ps5::PkgReader::read(path);
        CHECK(d.title == "Test PS5 Game");
        CHECK(d.titleId == "PPSA90001");
        CHECK(d.platform == "PS5");
        CHECK(d.format == "fpkg");
        CHECK(!d.isDlc);
        CHECK(!ps5::PkgReader::isImageFile(d.format));
    }

    {
        const QString path = dir.filePath("PPSA12345 My Dump.exfat");
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(QByteArray(64, 'z'));
        f.close();
        const ps5::PkgData d = ps5::PkgReader::read(path);
        CHECK(d.titleId == "PPSA12345");
        CHECK(d.format == "exfat");
        CHECK(d.title == "PPSA12345 My Dump");
        CHECK(d.size == 64);
    }

    {
        CHECK(ps5::PkgReader::isLibraryFile("fpkg"));
        CHECK(ps5::PkgReader::isLibraryFile("FPKG"));
        CHECK(ps5::PkgReader::isLibraryFile("pkg"));
        CHECK(ps5::PkgReader::isLibraryFile("PKG"));
        CHECK(ps5::PkgReader::isLibraryFile("ffpkg"));
        CHECK(!ps5::PkgReader::isLibraryFile("zip"));
        CHECK(!ps5::PkgReader::isLibraryFile(""));
        CHECK(ps5::PkgReader::isImageFile("FFPKG"));
        CHECK(!ps5::PkgReader::isImageFile("pkg"));
        CHECK(!ps5::PkgReader::isImageFile("fpkg"));
        CHECK(ps5::PkgReader::titleIdFromName("CUSA00123 something") == "CUSA00123");
    }

    if (failures == 0) {
        std::printf("all parser tests passed\n");
        return 0;
    }
    return 1;
}
