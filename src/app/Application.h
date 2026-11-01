#pragma once

#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include "core/Discovery.h"
#include "core/LibraryModel.h"
#include "core/LibraryScanner.h"
#include "core/RangeFileServer.h"
#include "core/ReceiverClient.h"
#include "core/TransferQueue.h"
#include "core/UpdateChecker.h"

namespace ps5 {

class Application : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString accentColor READ accentColor WRITE setAccentColor
                   NOTIFY accentColorChanged)
    Q_PROPERTY(ps5::LibraryModel *libraryModel READ libraryModel CONSTANT)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanStatusChanged)
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(QString lastPickFolder READ lastPickFolder WRITE setLastPickFolder
                   NOTIFY lastPickFolderChanged)
    Q_PROPERTY(ps5::TransferQueue *queue READ queue CONSTANT)
    Q_PROPERTY(QString destination READ destination WRITE setDestination
                   NOTIFY destinationChanged)
    Q_PROPERTY(QVariantList historyRows READ historyRows NOTIFY historyChanged)
    Q_PROPERTY(QStringList scanPaths READ scanPaths NOTIFY scanPathsChanged)
    Q_PROPERTY(bool destinationIsCustom READ destinationIsCustom
                   NOTIFY destinationChanged)

    Q_PROPERTY(QString psIp READ psIp WRITE setPsIp NOTIFY psIpChanged)
    Q_PROPERTY(QString pcIp READ pcIp WRITE setPcIp NOTIFY pcIpChanged)
    Q_PROPERTY(QStringList pcAddresses READ pcAddresses NOTIFY pcAddressesChanged)
    Q_PROPERTY(bool hasNetwork READ hasNetwork NOTIFY hasNetworkChanged)
    Q_PROPERTY(bool receiverOnline READ receiverOnline NOTIFY receiverOnlineChanged)
    Q_PROPERTY(bool receiverBusy READ receiverBusy NOTIFY receiverOnlineChanged)
    Q_PROPERTY(QString receiverBuild READ receiverBuild NOTIFY receiverOnlineChanged)
    Q_PROPERTY(QString freeSpaceText READ freeSpaceText NOTIFY receiverOnlineChanged)
    Q_PROPERTY(bool discovering READ discovering NOTIFY discoveringChanged)
    Q_PROPERTY(bool shareEnabled READ shareEnabled WRITE setShareEnabled
                   NOTIFY shareEnabledChanged)
    Q_PROPERTY(QString catalogUrl READ catalogUrl NOTIFY shareEnabledChanged)
    Q_PROPERTY(QString discoveryStatus READ discoveryStatus NOTIFY discoveringChanged)
    Q_PROPERTY(QVariantList consoles READ consoles NOTIFY consolesChanged)
    Q_PROPERTY(bool autoCheckUpdates READ autoCheckUpdates WRITE setAutoCheckUpdates
                   NOTIFY autoCheckUpdatesChanged)
    Q_PROPERTY(bool checkingUpdate READ checkingUpdate NOTIFY updateStatusChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateStatusChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateStatusChanged)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY updateStatusChanged)

public:
    explicit Application(QObject *parent = nullptr);

    QString version() const { return m_version; }
    QString title() const { return m_title; }
    QString theme() const { return m_theme; }
    void setTheme(const QString &theme);
    QString accentColor() const { return m_accentColor; }
    void setAccentColor(const QString &color);
    LibraryModel *libraryModel() const { return m_library; }
    bool scanning() const { return m_scanning; }
    QString scanStatus() const { return m_scanStatus; }
    QString databasePath() const { return Database::instance().databasePath(); }
    QString lastPickFolder() const;
    void setLastPickFolder(const QString &folder);
    TransferQueue *queue() const { return m_queue; }
    QString destination() const;
    void setDestination(const QString &destination);
    QVariantList historyRows() const { return m_historyRows; }
    QStringList scanPaths() const { return m_scanPaths; }
    bool destinationIsCustom() const;

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

    bool shareEnabled() const;
    void setShareEnabled(bool on);
    QString catalogUrl() const;

    bool autoCheckUpdates() const { return m_autoCheckUpdates; }
    void setAutoCheckUpdates(bool on);
    bool checkingUpdate() const { return m_checkingUpdate; }
    bool updateAvailable() const { return m_updateAvailable; }
    QString latestVersion() const { return m_latestVersion; }
    QString releaseUrl() const { return m_releaseUrl; }

    Q_INVOKABLE void addFolder(const QUrl &folder);
    Q_INVOKABLE void addDroppedUrls(const QList<QUrl> &urls);
    Q_INVOKABLE void rescanAll();
    Q_INVOKABLE void removeEntry(qlonglong id);

    Q_INVOKABLE void detectConsoles();
    Q_INVOKABLE void cancelDiscovery();
    Q_INVOKABLE void testConnection();
    Q_INVOKABLE void useConsole(const QString &ip);
    Q_INVOKABLE void refreshPcAddresses();
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void sendSelected();
    Q_INVOKABLE void copySelectedImages();
    Q_INVOKABLE void reloadHistory();
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE bool addCustomScanPath(const QString &path);
    Q_INVOKABLE void removeCustomScanPath(const QString &path);

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void openReleasePage();
    Q_INVOKABLE void openLink(const QString &url);

signals:
    void scanningChanged();
    void scanStatusChanged();
    void lastPickFolderChanged();
    void notify(const QString &message);
    void themeChanged();
    void accentColorChanged();
    void psIpChanged();
    void pcIpChanged();
    void pcAddressesChanged();
    void hasNetworkChanged();
    void receiverOnlineChanged();
    void discoveringChanged();
    void consolesChanged();
    void shareEnabledChanged();
    void destinationChanged();
    void historyChanged();
    void scanPathsChanged();
    void autoCheckUpdatesChanged();
    void updateStatusChanged();

private:
    void startScan(const QStringList &roots);
    void pruneMissing();
    void enqueueSelection(bool copy);
    QVariantList buildHistoryRows() const;
    QStringList refreshScanPaths();
    void pollReceiver();
    void probeConsoles(const QStringList &ips);
    void saveIpSettings();
    void runUpdateCheck(bool manual);

    QString m_version;
    QString m_title;
    QString m_theme;
    QString m_accentColor;
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

    bool m_shareEnabled = false;
    RangeFileServer *m_server = nullptr;
    TransferQueue *m_queue = nullptr;
    QVariantList m_historyRows;
    QStringList m_scanPaths;

    ReceiverClient *m_receiver = nullptr;
    UpdateChecker *m_updater = nullptr;
    QTimer m_pollTimer;
    QTimer m_offlineDebounce;
    int m_missedPolls = 0;
    int m_pollCount = 0;

    bool m_autoCheckUpdates = true;
    bool m_checkingUpdate = false;
    bool m_updateAvailable = false;
    QString m_latestVersion;
    QString m_releaseUrl;
};

}
