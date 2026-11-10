#include "Discovery.h"

#include <QElapsedTimer>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QtConcurrent/QtConcurrentMap>
#include <QtConcurrent/QtConcurrentRun>

namespace ps5 {

namespace {

constexpr int ProbeChunkSize = 64;

bool tcpOpen(const QString &ip, int port, int timeoutMs)
{
    QTcpSocket sock;
    sock.connectToHost(ip, quint16(port));
    return sock.waitForConnected(timeoutMs);
}

bool isVirtualName(const QString &name)
{
    const QString s = name.toLower();
    static const QStringList marks = {
        QStringLiteral("virtual"), QStringLiteral("hyper-v"), QStringLiteral("hyperv"),
        QStringLiteral("wsl"), QStringLiteral("vmware"), QStringLiteral("virtualbox"),
        QStringLiteral("vpn"), QStringLiteral("pseudo"), QStringLiteral("wireguard"),
        QStringLiteral("tailscale"), QStringLiteral("tap-"),
    };
    for (const QString &m : marks)
        if (s.contains(m))
            return true;
    return false;
}

quint32 toUint(const QHostAddress &a)
{
    return a.toIPv4Address();
}

QHostAddress fromUint(quint32 v)
{
    return QHostAddress(v);
}

} // namespace

QHostAddress LanNetwork::network() const
{
    return fromUint(toUint(address) & toUint(mask));
}

QHostAddress LanNetwork::broadcast() const
{
    return fromUint(toUint(address) | ~toUint(mask));
}

bool LanNetwork::contains(const QHostAddress &ip) const
{
    if (ip.protocol() != QAbstractSocket::IPv4Protocol)
        return false;
    return (toUint(ip) & toUint(mask)) == toUint(network());
}

QStringList LanNetwork::hosts(int maxHosts) const
{
    QStringList out;
    const quint32 net = toUint(network());
    const quint32 bc = toUint(broadcast());
    if (prefixLength >= 31) {
        out << fromUint(net).toString();
        if (bc != net)
            out << fromUint(bc).toString();
        return out;
    }
    quint32 yielded = 0;
    for (quint32 h = net + 1; h < bc; h++) {
        if (yielded >= quint32(maxHosts))
            break;
        out.append(fromUint(h).toString());
        yielded++;
    }
    return out;
}

QList<LanNetwork> Discovery::lanNetworks()
{
    QList<LanNetwork> out;
    const auto nics = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &nic : nics) {
        if (nic.flags().testFlag(QNetworkInterface::IsUp)
            && !nic.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            for (const QNetworkAddressEntry &entry : nic.addressEntries()) {
                if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                    continue;
                if (entry.ip().isLoopback())
                    continue;
                if (entry.netmask().isNull())
                    continue;
                if (entry.ip().toString().startsWith(QStringLiteral("169.254.")))
                    continue;
                LanNetwork net;
                net.name = nic.humanReadableName();
                net.address = entry.ip();
                net.mask = entry.netmask();
                net.prefixLength = entry.prefixLength();
                net.isVirtual = isVirtualName(
                    nic.humanReadableName() + QLatin1Char(' ') + nic.name());
                out.append(net);
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const LanNetwork &a, const LanNetwork &b) {
        return a.isVirtual < b.isVirtual;
    });
    return out;
}

QString Discovery::bestPcIpFor(const QList<LanNetwork> &nets, const QString &psIp)
{
    if (nets.isEmpty())
        return {};
    if (!psIp.isEmpty()) {
        const QHostAddress ps(psIp);
        for (const LanNetwork &net : nets) {
            if (!net.isVirtual && net.contains(ps))
                return net.address.toString();
        }
    }
    for (const LanNetwork &net : nets) {
        if (!net.isVirtual)
            return net.address.toString();
    }
    return nets.first().address.toString();
}

void Discovery::cancel()
{
    m_cancelled = true;
}

void Discovery::findConsoles()
{
    if (m_running)
        return;
    m_running = true;
    m_cancelled = false;

    (void)QtConcurrent::run([this] {
        const auto finish = [this] {
            QMetaObject::invokeMethod(this, [this] { m_running = false; },
                                      Qt::QueuedConnection);
        };

        emit progressMessage(tr("Listening for receiver beacons…"));

        QStringList beaconIps;
        {
            QUdpSocket udp;
            udp.bind(QHostAddress::AnyIPv4, BeaconPort,
                     QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
            QElapsedTimer clock;
            clock.start();
            while (clock.elapsed() < 3500) {
                if (m_cancelled) {
                    emit progressMessage(tr("Cancelled"));
                    emit cancelled();
                    finish();
                    return;
                }
                if (!udp.waitForReadyRead(250))
                    continue;
                while (udp.hasPendingDatagrams()) {
                    QByteArray d;
                    d.resize(int(udp.pendingDatagramSize()));
                    QHostAddress from;
                    quint16 port = 0;
                    udp.readDatagram(d.data(), d.size(), &from, &port);
                    if (d.startsWith("PKGSENDER ") && !from.isLoopback()
                        && !beaconIps.contains(from.toString()))
                        beaconIps.append(from.toString());
                }
            }
        }

        emit progressMessage(beaconIps.isEmpty()
            ? tr("No beacon heard, sweeping the LAN…")
            : tr("Beacon heard from %n host(s), sweeping to be sure…", "",
                 beaconIps.size()));

        QStringList candidates = beaconIps;
        const QList<LanNetwork> nets = lanNetworks();
        for (const LanNetwork &net : nets) {
            if (net.isVirtual)
                continue;
            for (const QString &ip : net.hosts(MaxHostsPerNic)) {
                if (!candidates.contains(ip))
                    candidates.append(ip);
            }
        }

        QStringList found;
        for (int i = 0; i < candidates.size(); i += ProbeChunkSize) {
            if (m_cancelled) {
                emit progressMessage(tr("Cancelled"));
                emit cancelled();
                finish();
                return;
            }
            const QStringList chunk = candidates.mid(i, ProbeChunkSize);
            const QStringList probed = QtConcurrent::blockingMapped(
                chunk, [](const QString &ip) -> QString {
                    return tcpOpen(ip, ReceiverPort, 300) ? ip : QString();
                });
            for (const QString &ip : probed)
                if (!ip.isEmpty())
                    found.append(ip);
            emit progressMessage(tr("Sweeping the LAN… %1/%2")
                                     .arg(qMin(i + ProbeChunkSize,
                                               candidates.size()))
                                     .arg(candidates.size()));
        }
        std::sort(found.begin(), found.end());

        emit progressMessage(tr("Found %n console(s)", "", found.size()));
        emit consolesFound(found);
        finish();
    });
}

}
