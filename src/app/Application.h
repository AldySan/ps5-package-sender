#pragma once

#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include "core/Discovery.h"
#include "core/LibraryModel.h"
#include "core/LibraryScanner.h"
#include "core/ReceiverClient.h"

namespace ps5 {

class Application : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(ps5::LibraryModel *libraryModel READ libraryModel CONSTANT)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanStatusChanged)
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(QString lastPickFolder READ lastPickFolder WRITE setLastPickFolder
                   NOTIFY lastPickFolderChanged)
    Q_PROPERTY(bool canSend READ canSend CONSTANT)

    Q_PROPERTY(QString psIp READ psIp WRITE setPsIp NOTIFY psIpChanged)
    Q_PROPERTY(QString pcIp READ pcIp WRITE setPcIp NOTIFY pcIpChanged)
    Q_PROPERTY(QStringList pcAddresses READ pcAddresses NOTIFY pcAddressesChanged)
    Q_PROPERTY(bool hasNetwork READ hasNetwork NOTIFY hasNetworkChanged)
    Q_PROPERTY(bool receiverOnline READ receiverOnline NOTIFY receiverOnlineChanged)
    Q_PROPERTY(bool receiverBusy READ receiverBusy NOTIFY receiverOnlineChanged)
    Q_PROPERTY(QString receiverBuild READ receiverBuild NOTIFY receiverOnlineChanged)
    Q_PROPERTY(QString freeSpaceText READ freeSpaceText NOTIFY receiverOnlineChanged)
    Q_PROPERTY(bool discovering READ discovering NOTIFY discoveringChanged)
    Q_PROPERTY(QString discoveryStatus READ discoveryStatus NOTIFY discoveringChanged)
    Q_PROPERTY(QVariantList consoles READ consoles NOTIFY consolesChanged)

public:
    explicit Application(QObject *parent = nullptr);

    QString version() const { return m_version; }
    QString title() const { return m_title; }
    LibraryModel *libraryModel() const { return m_library; }
    bool scanning() const { return m_scanning; }
    QString scanStatus() const { return m_scanStatus; }
    QString databasePath() const { return Database::instance().databasePath(); }
    QString lastPickFolder() const;
    void setLastPickFolder(const QString &folder);
    bool canSend() const { return false; }

    QString psIp() const { return m_psIp; }
    void setPsIp(const QString &ip);
    QString pcIp() const { return m_pcIp; }
    void setPcIp(const QString &ip);
    QStringList pcAddresses() const { return m_pcAddresses; }
    bool hasNetwork() const { return !m_pcAddresses.isEmpty(); }
    bool receiverOnline() const { return m_receiverOnline; }
    bool receiverBusy() const { return m_receiverBusy; }
    QString receiverBuild() const { return m_receiverBuild; }
    QString freeSpaceText() const { return m_freeSpaceText; }
    bool discovering() const { return m_discovering; }
    QString discoveryStatus() const { return m_discoveryStatus; }
    QVariantList consoles() const { return m_consoles; }

    Q_INVOKABLE void addFolder(const QUrl &folder);
    Q_INVOKABLE void addDroppedUrls(const QList<QUrl> &urls);
    Q_INVOKABLE void rescanAll();
    Q_INVOKABLE void removeEntry(qlonglong id);

    Q_INVOKABLE void detectConsoles();
    Q_INVOKABLE void testConnection();
    Q_INVOKABLE void useConsole(const QString &ip);
    Q_INVOKABLE void refreshPcAddresses();

signals:
    void scanningChanged();
    void scanStatusChanged();
    void lastPickFolderChanged();
    void notify(const QString &message);
    void psIpChanged();
    void pcIpChanged();
    void pcAddressesChanged();
    void hasNetworkChanged();
    void receiverOnlineChanged();
    void discoveringChanged();
    void consolesChanged();

private:
    void startScan(const QStringList &roots, bool rememberRoots);
    void pruneMissing();
    void pollReceiver();
    void probeConsoles(const QStringList &ips);
    void saveIpSettings();

    QString m_version;
    QString m_title;
    LibraryModel *m_library = nullptr;
    LibraryScanner *m_scanner = nullptr;
    bool m_scanning = false;
    QString m_scanStatus;

    QString m_psIp;
    QString m_pcIp;
    QStringList m_pcAddresses;
    bool m_receiverOnline = false;
    bool m_receiverBusy = false;
    QString m_receiverBuild;
    QString m_freeSpaceText;

    Discovery *m_discovery = nullptr;
    bool m_discovering = false;
    QString m_discoveryStatus;
    QVariantList m_consoles;

    ReceiverClient *m_receiver = nullptr;
    QTimer m_pollTimer;
    QTimer m_offlineDebounce;
    int m_missedPolls = 0;
};

}
