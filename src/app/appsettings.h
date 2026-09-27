#pragma once

#include <QFont>
#include <QString>
#include <QStringList>

class AppSettings
{
public:
    static AppSettings &instance();

    QString uiTheme() const;
    void setUiTheme(const QString &id);

    QString uiFontFamily() const;
    int uiFontSize() const;
    void setUiFont(const QString &family, int pointSize);

    QString termFontFamily() const;
    int termFontSize() const;
    void setTermFont(const QString &family, int pointSize);

    bool showQuickConnect() const;
    void setShowQuickConnect(bool on);
    bool showComposeBar() const;
    void setShowComposeBar(bool on);
    bool showToolbar() const;
    void setShowToolbar(bool on);
    int scrollbackLines() const;
    void setScrollbackLines(int n);
    bool logHighlight() const;
    void setLogHighlight(bool on);
    bool confirmDisconnect() const;
    void setConfirmDisconnect(bool on);
    bool promptEditUpload() const;
    void setPromptEditUpload(bool on);
    bool autoUploadOnEdit() const;
    void setAutoUploadOnEdit(bool on);
    bool askImageOpen() const;
    void setAskImageOpen(bool on);
    bool useImageViewer() const;
    void setUseImageViewer(bool on);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &g);
    QByteArray windowState() const;
    void setWindowState(const QByteArray &s);

    static QStringList themeIds();
    static QString themeDisplayName(const QString &id);
    static QString themeStylePath(const QString &id);

    QFont uiFont() const;
    QFont termFont() const;

    static void applyAppearance();

private:
    AppSettings() = default;
};
