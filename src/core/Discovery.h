#pragma once

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QStringList>

namespace ps5 {

struct LanNetwork {
    QString name;
    QHostAddress address;
    QHostAddress mask;
    int prefixLength = 0;
    bool isVirtual = false;

    QHostAddress network() const;
    QHostAddress broadcast() const;
    bool contains(const QHostAddress &ip) const;
    QStringList hosts(int maxHosts) const;
};

class Discovery : public QObject {
    Q_OBJECT

public:
    static constexpr int ReceiverPort = 12800;
    static constexpr int BeaconPort = 12801;
    static constexpr int MaxHostsPerNic = 4096;

    using QObject::QObject;

    static QList<LanNetwork> lanNetworks();
    static QString bestPcIpFor(const QList<LanNetwork> &nets, const QString &psIp);

    void findConsoles();

signals:
    void progressMessage(const QString &message);
    void consolesFound(const QStringList &ips);

private:
    bool m_running = false;
};

}
