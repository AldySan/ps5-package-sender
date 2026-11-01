#pragma once

#include <QObject>
#include <QString>
#include <functional>

namespace ps5 {

class UpdateChecker : public QObject {
    Q_OBJECT

public:
    struct Result {
        bool available = false;
        QString latestVersion;
        QString releaseUrl;
        QString error;
    };

    using ResultCallback = std::function<void(const Result &)>;

    explicit UpdateChecker(const QString &currentVersion, QObject *parent = nullptr);

    static bool isVersionNewer(const QString &latest, const QString &current);

    void check(const ResultCallback &callback) const;

private:
    QString m_currentVersion;
};

}
