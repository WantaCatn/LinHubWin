#include "app/appsettings.h"
#include "app/application.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QSettings>

AppSettings &AppSettings::instance()
{
    static AppSettings s;
    return s;
}

QString AppSettings::uiTheme() const
{
    return QSettings().value(QStringLiteral("ui/theme"), QStringLiteral("dark")).toString();
}

void AppSettings::setUiTheme(const QString &id)
{
    QSettings().setValue(QStringLiteral("ui/theme"), id);
}

QString AppSettings::uiFontFamily() const
{
    return QSettings().value(QStringLiteral("ui/fontFamily"), QString()).toString();
}

int AppSettings::uiFontSize() const
{
    return QSettings().value(QStringLiteral("ui/fontSize"), 12).toInt();
}

void AppSettings::setUiFont(const QString &family, int pointSize)
{
    QSettings s;
    s.setValue(QStringLiteral("ui/fontFamily"), family);
    s.setValue(QStringLiteral("ui/fontSize"), pointSize);
}

QString AppSettings::termFontFamily() const
{
    return QSettings().value(QStringLiteral("term/fontFamily"), QString()).toString();
}

int AppSettings::termFontSize() const
{
    return qBound(8, QSettings().value(QStringLiteral("term/fontSize"), 11).toInt(), 32);
}

void AppSettings::setTermFont(const QString &family, int pointSize)
{
    QSettings s;
    s.setValue(QStringLiteral("term/fontFamily"), family);
    s.setValue(QStringLiteral("term/fontSize"), pointSize);
}

bool AppSettings::showQuickConnect() const
{
    return QSettings().value(QStringLiteral("ui/showQuickConnect"), true).toBool();
}

void AppSettings::setShowQuickConnect(bool on)
{
    QSettings().setValue(QStringLiteral("ui/showQuickConnect"), on);
}

bool AppSettings::showComposeBar() const
{
    return QSettings().value(QStringLiteral("ui/showComposeBar"), true).toBool();
}

void AppSettings::setShowComposeBar(bool on)
{
    QSettings().setValue(QStringLiteral("ui/showComposeBar"), on);
}

bool AppSettings::showToolbar() const
{
    return QSettings().value(QStringLiteral("ui/showToolbar"), true).toBool();
}

void AppSettings::setShowToolbar(bool on)
{
    QSettings().setValue(QStringLiteral("ui/showToolbar"), on);
}

int AppSettings::scrollbackLines() const
{
    return qBound(200, QSettings().value(QStringLiteral("term/scrollback"), 20000).toInt(), 50000);
}

void AppSettings::setScrollbackLines(int n)
{
    QSettings().setValue(QStringLiteral("term/scrollback"), qBound(200, n, 50000));
}

bool AppSettings::logHighlight() const
{
    return QSettings().value(QStringLiteral("term/logHighlight"), true).toBool();
}

void AppSettings::setLogHighlight(bool on)
{
    QSettings().setValue(QStringLiteral("term/logHighlight"), on);
}

bool AppSettings::confirmDisconnect() const
{
    return QSettings().value(QStringLiteral("ui/confirmDisconnect"), true).toBool();
}

void AppSettings::setConfirmDisconnect(bool on)
{
    QSettings().setValue(QStringLiteral("ui/confirmDisconnect"), on);
}

bool AppSettings::promptEditUpload() const
{
    return QSettings().value(QStringLiteral("sftp/promptEditUpload"), true).toBool();
}

void AppSettings::setPromptEditUpload(bool on)
{
    QSettings().setValue(QStringLiteral("sftp/promptEditUpload"), on);
}

bool AppSettings::autoUploadOnEdit() const
{
    return QSettings().value(QStringLiteral("sftp/autoUploadOnEdit"), true).toBool();
}

void AppSettings::setAutoUploadOnEdit(bool on)
{
    QSettings().setValue(QStringLiteral("sftp/autoUploadOnEdit"), on);
}

bool AppSettings::askImageOpen() const
{
    return QSettings().value(QStringLiteral("sftp/askImageOpen"), true).toBool();
}

void AppSettings::setAskImageOpen(bool on)
{
    QSettings().setValue(QStringLiteral("sftp/askImageOpen"), on);
}

bool AppSettings::useImageViewer() const
{
    return QSettings().value(QStringLiteral("sftp/useImageViewer"), true).toBool();
}

void AppSettings::setUseImageViewer(bool on)
{
    QSettings().setValue(QStringLiteral("sftp/useImageViewer"), on);
}

QByteArray AppSettings::windowGeometry() const
{
    return QSettings().value(QStringLiteral("win/geometry")).toByteArray();
}

void AppSettings::setWindowGeometry(const QByteArray &g)
{
    QSettings().setValue(QStringLiteral("win/geometry"), g);
}

QByteArray AppSettings::windowState() const
{
    return QSettings().value(QStringLiteral("win/state")).toByteArray();
}

void AppSettings::setWindowState(const QByteArray &s)
{
    QSettings().setValue(QStringLiteral("win/state"), s);
}

QStringList AppSettings::themeIds()
{
    return {QStringLiteral("dark"), QStringLiteral("midnight"), QStringLiteral("light")};
}

QString AppSettings::themeDisplayName(const QString &id)
{
    if (id == QLatin1String("midnight"))
        return QStringLiteral("午夜蓝");
    if (id == QLatin1String("light"))
        return QStringLiteral("浅色");
    return QStringLiteral("深色（默认）");
}

QString AppSettings::themeStylePath(const QString &id)
{
    if (id == QLatin1String("midnight"))
        return QStringLiteral(":/styles/midnight.qss");
    if (id == QLatin1String("light"))
        return QStringLiteral(":/styles/light.qss");
    return QStringLiteral(":/styles/dark.qss");
}

QFont AppSettings::uiFont() const
{
    QFont f = QApplication::font();
    const QString family = uiFontFamily();
    if (!family.isEmpty())
        f.setFamily(family);
    f.setPointSize(qBound(8, uiFontSize(), 20));
    return f;
}

QFont AppSettings::termFont() const
{
    QFont f(QStringLiteral("monospace"), termFontSize());
    const QString family = termFontFamily();
    if (!family.isEmpty())
        f.setFamily(family);
    f.setStyleHint(QFont::Monospace);
    f.setFixedPitch(true);
    return f;
}

void AppSettings::applyAppearance()
{
    Application::applyTheme();
}
