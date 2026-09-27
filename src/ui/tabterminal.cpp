#include "ui/tabterminal.h"

#include "util/crypto.h"

#include <QDateTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {

QString guessConnectReason(const QString &text)
{
    const QStringList keys = {
        QStringLiteral("Connection timed out"),
        QStringLiteral("Connection refused"),
        QStringLiteral("No route to host"),
        QStringLiteral("Network is unreachable"),
        QStringLiteral("Name or service not known"),
        QStringLiteral("Could not resolve hostname"),
        QStringLiteral("Temporary failure in name resolution"),
        QStringLiteral("Permission denied"),
        QStringLiteral("Authentication failed"),
        QStringLiteral("Host key verification failed"),
        QStringLiteral("Connection reset"),
        QStringLiteral("kex_exchange_identification"),
        QStringLiteral("No matching key exchange"),
        QStringLiteral("Connection closed by remote host"),
        QStringLiteral("ssh: connect to host"),
    };
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QString line = lines.at(i).trimmed();
        if (line.isEmpty())
            continue;
        for (const QString &key : keys) {
            if (line.contains(key, Qt::CaseInsensitive))
                return line;
        }
    }
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QString line = lines.at(i).trimmed();
        if (!line.isEmpty() && !line.startsWith(QLatin1String("Connecting to")))
            return line;
    }
    return QStringLiteral("无法连接到远程主机");
}

} // namespace

TabTerminal::TabTerminal(const Session &session, const QString &password, QWidget *parent)
    : QWidget(parent)
    , m_session(session)
    , m_password(password)
    , m_entryPassword(password)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_term = new TerminalWidget(this);
    lay->addWidget(m_term);
    setFocusProxy(m_term);
    setFocusPolicy(Qt::StrongFocus);
    connect(m_term, &TerminalWidget::sessionFinished, this, [this](int code) {
        if (m_ignoreFinished)
            return;
        m_master.stop();
        if (m_fallbackLocal) {
            m_remoteConnected = false;
            m_led = LedRed;
            emit finished(code);
            emit connectionChanged();
            return;
        }
        if (m_session.protocol == Protocol::Local) {
            m_remoteConnected = false;
            m_led = LedRed;
            emit finished(code);
            emit connectionChanged();
            return;
        }
        if (m_userDisconnect)
            enterLocalFallback(QString(), true);
        else
            enterLocalFallback(connectionFailReason(), false);
        emit finished(code);
        emit connectionChanged();
    });
    connect(m_term, &TerminalWidget::titleChanged, this, &TabTerminal::titleChanged);
    connect(m_term, &TerminalWidget::activity, this, [this] {
        if (!m_remoteConnected)
            return;
        m_connectAttempt = false;
        m_led = LedGreen;
        emit activity();
    });
    connect(&m_master, &SshMaster::ready, this, [this] { emit muxReady(); });

    const Session entry = sessionEntryTarget(session);
    if (!session.jumps.isEmpty()) {
        m_entryPassword = Crypto::decrypt(session.jumps.first().passwordEnc);
        if (m_entryPassword.isEmpty())
            m_entryPassword = password;
    }

}

void TabTerminal::startIfNeeded()
{
    if (m_started)
        return;
    m_started = true;
    startMuxAndSession(true);
    if (m_term)
        m_term->setFocus(Qt::OtherFocusReason);
}

TabTerminal::~TabTerminal()
{
    m_master.stop();
}

void TabTerminal::startMuxAndSession(bool resetScreen)
{
    m_userDisconnect = false;
    m_connectAttempt = m_session.protocol == Protocol::Ssh
        || m_session.protocol == Protocol::Sftp
        || m_session.protocol == Protocol::Telnet;

    if (!m_term->startSession(m_session, m_password, QString(), resetScreen)) {
        m_remoteConnected = false;
        m_fallbackLocal = false;
        m_led = LedRed;
        enterLocalFallback(QStringLiteral("无法启动连接程序"), false);
        return;
    }
    m_remoteConnected = m_term->isRunning();
    m_fallbackLocal = false;
    m_led = m_remoteConnected ? LedBlue : LedRed;

    if (m_connectAttempt) {
        const int port = m_session.port > 0
                             ? m_session.port
                             : (m_session.protocol == Protocol::Telnet ? 23 : 22);
        m_term->writeLocal(QStringLiteral("Connecting to %1:%2 ...\n")
                               .arg(m_session.host, QString::number(port)));
    }

#ifndef Q_OS_WIN
    if (m_session.protocol == Protocol::Ssh || m_session.protocol == Protocol::Sftp) {
        QTimer::singleShot(1800, this, [this] {
            if (m_userDisconnect || m_fallbackLocal || !m_term || !m_term->isRunning())
                return;
            m_master.start(sessionEntryTarget(m_session), m_entryPassword);
        });
    }
#endif
}

bool TabTerminal::reconnect()
{
    if (m_remoteConnected && !m_fallbackLocal)
        return false;
    m_ignoreFinished = true;
    m_master.stop();
    m_term->disconnectSession();
    m_started = true;
    startMuxAndSession(true);
    QTimer::singleShot(400, this, [this] { m_ignoreFinished = false; });
    emit connectionChanged();
    return m_remoteConnected;
}

void TabTerminal::disconnectRemote()
{
    if (m_fallbackLocal)
        return;
    if (!m_remoteConnected && !m_term->isRunning())
        return;
    m_userDisconnect = true;
    enterLocalFallback(QString(), true);
}

void TabTerminal::markSeen()
{
    if (m_remoteConnected && m_led == LedGreen)
        m_led = LedBlue;
}

QString TabTerminal::connectionFailReason() const
{
    const QString fromTerm = guessConnectReason(m_term->recentPlainText(16));
    const QString fromMux = m_master.lastError().trimmed();
    if (!fromTerm.isEmpty() && fromTerm != QLatin1String("无法连接到远程主机"))
        return fromTerm;
    if (!fromMux.isEmpty())
        return fromMux;
    return fromTerm;
}

void TabTerminal::enterLocalFallback(const QString &reason, bool userDisconnect)
{
    if (m_fallbackLocal)
        return;
    m_remoteConnected = false;
    m_fallbackLocal = true;
    m_connectAttempt = false;
    m_led = LedRed;
    m_ignoreFinished = true;
    m_master.stop();
    m_term->disconnectSession();

    const QString host = m_session.host.isEmpty() ? m_session.displayTitle() : m_session.host;
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    QString banner;
    if (userDisconnect) {
        banner = QStringLiteral(
                     "\r\nConnection closed.\r\n"
                     "Disconnected from remote host (%1) at %2.\r\n"
                     "已回到本地终端。输入命令，或点击工具栏「重连」重新连接。\r\n\r\n")
                     .arg(host, now);
    } else {
        const QString why = reason.trimmed().isEmpty() ? QStringLiteral("无法连接到远程主机")
                                                       : reason.trimmed();
        banner = QStringLiteral(
                     "\r\nConnection failed.\r\n"
                     "连不上的原因: %1\r\n"
                     "已回到本地终端。输入命令，或点击工具栏「重连」重新连接。\r\n\r\n")
                     .arg(why);
    }

    Session local;
    local.protocol = Protocol::Local;
    local.name = m_session.displayTitle();
    local.colorScheme = m_session.colorScheme;
    m_term->startSession(local, QString(), QString(), false);
    m_term->writeLocal(QStringLiteral("\x1b[?1049l\x1b[?1047l"));
    m_term->writeLocal(banner);
    QTimer::singleShot(80, this, [this] {
        if (m_term)
            m_term->sendText(QStringLiteral(""), true);
        if (m_term)
            m_term->setFocus(Qt::OtherFocusReason);
    });
    QTimer::singleShot(400, this, [this] { m_ignoreFinished = false; });
    emit connectionChanged();
}
