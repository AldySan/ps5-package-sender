#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QThreadPool>
#include <QUrl>

#include "app/Application.h"
#include "app/IconProvider.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("AldySan");
    app.setApplicationName("ps5-package-sender");
    app.setApplicationVersion(APP_VERSION);

    QQuickStyle::setStyle("Basic");

    ps5::Application controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.addImageProvider(QStringLiteral("pkgicon"), new ps5::IconProvider);
    engine.loadFromModule("PS5PackageSender", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    const int rc = app.exec();
    QThreadPool::globalInstance()->waitForDone();
    return rc;
}
