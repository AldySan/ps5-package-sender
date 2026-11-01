#pragma once

#include <QObject>
#include <QUrl>
#include <QString>
#include <QStringList>

#include "core/LibraryModel.h"
#include "core/LibraryScanner.h"

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

    Q_INVOKABLE void addFolder(const QUrl &folder);
    Q_INVOKABLE void addDroppedUrls(const QList<QUrl> &urls);
    Q_INVOKABLE void rescanAll();
    Q_INVOKABLE void removeEntry(qlonglong id);

signals:
    void scanningChanged();
    void scanStatusChanged();
    void lastPickFolderChanged();
    void notify(const QString &message);

private:
    void startScan(const QStringList &roots, bool rememberRoots);
    void pruneMissing();

    QString m_version;
    QString m_title;
    LibraryModel *m_library = nullptr;
    LibraryScanner *m_scanner = nullptr;
    bool m_scanning = false;
    QString m_scanStatus;
};

}
