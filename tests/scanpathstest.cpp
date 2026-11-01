#include <QCoreApplication>

#include "../src/core/ScanPaths.h"
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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qputenv("PS5PKGSENDER_DATA_DIR", "/tmp/ps5pkg-scanpaths-test-data");

    CHECK(ps5::Database::instance().open());

    const QStringList builtins = ps5::ScanPaths::builtinPaths();
    CHECK(builtins.size() == 34);
    CHECK(builtins.first() == "/data/homebrew");
    CHECK(builtins.contains("/data/etaHEN/games"));
    CHECK(builtins.contains("/mnt/ext0/homebrew"));
    CHECK(builtins.contains("/mnt/ext1/etaHEN/games"));
    CHECK(builtins.contains("/mnt/usb0/homebrew"));
    CHECK(builtins.contains("/mnt/usb7/etaHEN/games"));
    CHECK(builtins.contains("/mnt/usb3"));
    CHECK(builtins.contains("/mnt/shadowmnt/pfsc"));
    CHECK(builtins.contains("/mnt/shadowmnt"));

    CHECK(ps5::ScanPaths::normalize("/data/homebrew/") == "/data/homebrew");
    CHECK(ps5::ScanPaths::normalize("/data/homebrew///") == "/data/homebrew");
    CHECK(ps5::ScanPaths::normalize("  /mnt/usb0  ") == "/mnt/usb0");
    CHECK(ps5::ScanPaths::normalize("/") == "/");

    CHECK(ps5::ScanPaths::isAbsoluteRemotePath("/mnt/usb0/homebrew"));
    CHECK(!ps5::ScanPaths::isAbsoluteRemotePath("mnt/usb0"));
    CHECK(!ps5::ScanPaths::isAbsoluteRemotePath("/data/../root"));
    CHECK(!ps5::ScanPaths::isAbsoluteRemotePath(""));
    CHECK(!ps5::ScanPaths::isAbsoluteRemotePath("/"));

    ps5::Database &db = ps5::Database::instance();
    db.addCustomScanPath("/mnt/usb0/dumps");
    db.addCustomScanPath("/mnt/usb0/dumps");
    CHECK(db.customScanPaths() == QStringList{"/mnt/usb0/dumps"});
    db.removeCustomScanPath("/mnt/usb0/dumps");
    CHECK(db.customScanPaths().isEmpty());

    if (failures == 0) {
        std::fprintf(stderr, "scan paths tests passed\n");
        return 0;
    }
    return 1;
}
