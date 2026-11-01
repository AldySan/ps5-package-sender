#include "Application.h"

#include <QCoreApplication>

namespace ps5 {

Application::Application(QObject *parent)
    : QObject(parent),
      m_version(QCoreApplication::applicationVersion()),
      m_title(QStringLiteral("PS5 Package Sender"))
{
}

}
