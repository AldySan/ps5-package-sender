#include <QCoreApplication>

#include "core/UpdateChecker.h"

#include <iostream>

using ps5::UpdateChecker;

namespace {

int failures = 0;

void expect(bool condition, const char *what)
{
    if (!condition) {
        failures++;
        std::cout << "FAIL: " << what << "\n";
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    expect(UpdateChecker::isVersionNewer("v0.0.2", "0.0.1"), "patch bump is newer");
    expect(UpdateChecker::isVersionNewer("0.1", "0.0.9"), "minor bump beats patches");
    expect(UpdateChecker::isVersionNewer("1.0.1", "1.0"), "extra component counts");
    expect(UpdateChecker::isVersionNewer("v10.0.0", "v9.99.99"), "major bump is newer");
    expect(!UpdateChecker::isVersionNewer("v0.0.1", "0.0.1"), "same version is not newer");
    expect(!UpdateChecker::isVersionNewer("0.0.1", "v0.0.2"), "older is not newer");
    expect(!UpdateChecker::isVersionNewer("1.0.0", "1.0"), "padded equal is not newer");
    expect(UpdateChecker::isVersionNewer("0.0.1", "0.0.1-beta"), "release beats pre-release tag");

    if (failures == 0)
        std::cout << "updatecheckertest: all checks passed\n";
    return failures == 0 ? 0 : 1;
}
