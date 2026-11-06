#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QTcpServer>
#include <QTimer>
#include <QUdpSocket>
#include <functional>

namespace ps5 {

class RangeFileServer : public QObject {
    Q_OBJECT

public:
    static constexpr int DefaultPort = 9898;
    static constexpr int PcAnnouncePort = 12802;

    using CatalogProvider = std::function<QJsonArray()>;
    using IconProvider = std::function<QByteArray(const QString &id)>;

    explicit RangeFileServer(QObject *parent = nullptr);

    bool start();
    int port() const { return m_port; }
    bool isListening() const { return m_port != 0; }

    void setCatalogProvider(const CatalogProvider &provider);
    void setIconProvider(const IconProvider &provider);

    void registerFile(const QString &id, const QString &localPath);
    void revoke(const QString &id);
    void forget(const QString &id);
    void resetServed(const QString &id);
    qint64 servedFor(const QString &id) const;

    QString pkgUrl(const QString &host, const QString &id) const;
    QString iconUrl(const QString &host, const QString &id) const;
    QString catalogUrl(const QString &host) const;

    void setAnnouncing(bool on, const QString &pcIp);

signals:
    void bytesServed(const QString &id, qint64 total);

private:
    friend class ServerConnection;

    void addServed(const QString &id, qint64 delta);
    void announceNow();

    QTcpServer m_listener;
    QUdpSocket m_announceSocket;
    QTimer m_announceTimer;
    QString m_announceIp;

    QHash<QString, QString> m_files;
    QHash<QString, bool> m_revoked;
    QHash<QString, qint64> m_served;
    CatalogProvider m_catalogProvider;
    IconProvider m_iconProvider;
    int m_port = 0;
};

}
