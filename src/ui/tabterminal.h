#pragma once

#include "core/session.h"
#include "net/sessionlauncher.h"
#include "terminal/terminalwidget.h"

#include <QWidget>

class TabTerminal : public QWidget
{
    Q_OBJECT
public:
    enum Led { LedBlue, LedGreen, LedRed };

    explicit TabTerminal(const Session &session, const QString &password, QWidget *parent = nullptr);
    ~TabTerminal() override;
    TerminalWidget *terminal() const { return m_term; }
    Session session() const { return m_session; }
    QString controlPath() const { return m_master.controlPath(); }
    QString password() const { return m_password; }
    QString entryPassword() const { return m_entryPassword; }
    QString tabTitle() const { return sessionTabTitle(m_session); }
    bool remoteConnected() const { return m_remoteConnected; }
    Led ledState() const { return m_led; }
    void startIfNeeded();
    bool reconnect();
    void disconnectRemote();
    void markSeen();

signals:
    void finished(int code);
    void titleChanged(const QString &title);
    void activity();
    void connectionChanged();
    void muxReady();

private:
    void startMuxAndSession(bool resetScreen);
    void enterLocalFallback(const QString &reason, bool userDisconnect);
    QString connectionFailReason() const;

    Session m_session;
    QString m_password;
    QString m_entryPassword;
    SshMaster m_master;
    TerminalWidget *m_term = nullptr;
    bool m_remoteConnected = false;
    bool m_fallbackLocal = false;
    bool m_ignoreFinished = false;
    bool m_userDisconnect = false;
    bool m_connectAttempt = false;
    bool m_started = false;
    Led m_led = LedBlue;
};
