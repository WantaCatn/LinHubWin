#include "app/application.h"
#include "app/appsettings.h"
#include "version.h"

#include <QFile>
#include <QGuiApplication>
#include <QFont>
#include <QIcon>
#include <QPixmapCache>
#include <QTimer>

#ifdef Q_OS_LINUX
#include <malloc.h>
#endif

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    setApplicationName(QStringLiteral(LINHUB_APP_NAME));
    setApplicationDisplayName(QStringLiteral(LINHUB_APP_DISPLAY_NAME_ZH));
    setOrganizationName(QStringLiteral(LINHUB_ORG_NAME));
    setApplicationVersion(QStringLiteral(LINHUB_VERSION));
    QIcon appIcon(QStringLiteral(":/icons/linhub.png"));
    if (appIcon.isNull())
        appIcon = QIcon(QStringLiteral(":/icons/linhub.svg"));
    setWindowIcon(appIcon);
    setQuitOnLastWindowClosed(true);
    setAttribute(Qt::AA_DontShowIconsInMenus, false);
    QPixmapCache::setCacheLimit(2048);
    applyTheme();

    connect(this, &QGuiApplication::applicationStateChanged, this, [](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive)
            Application::releaseSpareResources();
    });
    auto *trim = new QTimer(this);
    trim->setInterval(180000);
    connect(trim, &QTimer::timeout, this, [] { Application::releaseSpareResources(); });
    trim->start();
}

void Application::releaseSpareResources()
{
    QPixmapCache::clear();
#ifdef Q_OS_LINUX
    malloc_trim(0);
#endif
}

void Application::applyTheme()
{
    const QString path = AppSettings::themeStylePath(AppSettings::instance().uiTheme());
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        f.setFileName(QStringLiteral(":/styles/dark.qss"));
        f.open(QIODevice::ReadOnly);
    }
    if (f.isOpen())
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));

    qApp->setFont(AppSettings::instance().uiFont());
}
