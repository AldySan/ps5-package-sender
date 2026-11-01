#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>

#include "../src/core/LibraryModel.h"
#include "../src/core/LibraryScanner.h"
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

static void put32be(QByteArray &b, qsizetype off, quint32 v)
{
    b[off] = char(v >> 24);
    b[off + 1] = char(v >> 16);
    b[off + 2] = char(v >> 8);
    b[off + 3] = char(v);
}

static QString makePkg(const QString &dir, const char *title)
{
    const quint32 tableOff = 0x5A0;
    const quint32 nameTableOff = tableOff + 4 * 0x20;
    const QByteArray param = QStringLiteral(
        "{\"contentVersion\":\"1.00\",\"localizedParameters\":"
        "{\"defaultLanguage\":\"en-US\",\"en-US\":{\"titleName\":\"%1\"}}}")
        .arg(QString::fromUtf8(title)).toUtf8();
    const QByteArray names = QByteArray("param.json\0x", 12) + QByteArray(28, '\0');

    QByteArray pkg(0x1004 + param.size(), 0);
    pkg[0] = char(0x7F);
    pkg[1] = 'C';
    pkg[2] = 'N';
    pkg[3] = 'T';
    put32be(pkg, 0x10, 4);
    put32be(pkg, 0x18, tableOff);
    const QByteArray cid = QByteArrayLiteral("UP0001-PPSA90001_00-TESTGAME00000");
    for (int i = 0; i < cid.size(); i++)
        pkg[0x40 + i] = cid[i];

    const auto entry = [&](qsizetype row, quint32 id, quint32 nameOff, quint32 off,
                           quint32 size) {
        const qsizetype o = tableOff + row * 0x20;
        put32be(pkg, o, id);
        put32be(pkg, o + 4, nameOff);
        put32be(pkg, o + 8, 0);
        put32be(pkg, o + 0x10, off);
        put32be(pkg, o + 0x14, size);
    };
    entry(0, 0x0200, 0, nameTableOff, 40);
    entry(1, 0x2000, 0, 0x1004, quint32(param.size()));
    memcpy(pkg.data() + nameTableOff, names.constData(), 40);
    memcpy(pkg.data() + 0x1004, param.constData(), param.size());

    const QString path = dir + "/game-" + QString::fromUtf8(title) + ".pkg";
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(pkg);
    f.close();
    return path;
}

static void scanAndWait(ps5::LibraryScanner &scanner, const QStringList &roots)
{
    QEventLoop loop;
    QObject::connect(&scanner, &ps5::LibraryScanner::finished, &loop,
                     &QEventLoop::quit);
    QTimer::singleShot(15000, &loop, &QEventLoop::quit);
    scanner.scan(roots);
    loop.exec();
    QEventLoop flush;
    QTimer::singleShot(0, &flush, &QEventLoop::quit);
    flush.exec();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dataDir;
    qputenv("PS5PKGSENDER_DATA_DIR", dataDir.path().toUtf8());
    QTemporaryDir dir;

    CHECK(ps5::Database::instance().open());
    ps5::Database &db = ps5::Database::instance();

    ps5::LibraryScanner scanner;
    ps5::LibraryModel model;

    makePkg(dir.path(), "Alpha Game");
    makePkg(dir.path(), "Beta Game");
    db.addScannedRoot(dir.path());

    scanAndWait(scanner, db.scannedRoots());

    const QList<ps5::LibraryRow> rows = db.libraryRows();
    CHECK(rows.size() == 2);
    if (rows.size() == 2) {
        CHECK(rows.at(0).title == "Alpha Game");
        CHECK(rows.at(0).titleId == "PPSA90001");
        CHECK(rows.at(0).platform == "PS5");
        CHECK(rows.at(0).format == "pkg");
        CHECK(rows.at(0).size > 0x1000);
        CHECK(!rows.at(0).hasIcon);
        CHECK(db.libraryIcon(rows.at(0).id).isEmpty());
    }

    model.reload();
    CHECK(model.rowCount() == rows.size());
    if (rows.isEmpty()) {
        std::fprintf(stderr, "library empty, aborting further checks\n");
        return 1;
    }
    model.setSearchText("beta");
    CHECK(model.rowCount() == 1);
    model.setSearchText("");
    model.toggleSelected(rows.first().id);
    CHECK(model.selectedCount() == 1);
    model.selectVisible(true);
    CHECK(model.selectedCount() == rows.size());
    model.clearSelection();
    CHECK(model.selectedCount() == 0);

    db.setSetting("k", "v");
    CHECK(db.setting("k") == "v");
    CHECK(db.setting("missing", "def") == "def");

    const QString lone = makePkg(dir.path(), "Lone Game");
    scanAndWait(scanner, {lone});
    CHECK(db.libraryRows().size() == 3);
    CHECK(db.scannedRoots() == QStringList{dir.path()});

    if (failures == 0) {
        std::fprintf(stderr, "all library tests passed\n");
        return 0;
    }
    return 1;
}
