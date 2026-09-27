#include "net/sessionlauncher.h"
#include "net/askpass.h"
#include "util/qtcompat.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>
#include <QtGlobal>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace {
QString g_askpassFile;
int g_muxSeq = 0;
}

QString SessionLauncher::askpassPath()
{
#ifdef Q_OS_WIN
    const QString name = QStringLiteral("linhub-askpass.exe");
#else
    const QString name = QStringLiteral("linhub-askpass");
#endif
    const QString sibling = QCoreApplication::applicationDirPath()
        + QLatin1Char('/') + name;
    if (QFileInfo::exists(sibling))
        return sibling;

#ifndef Q_OS_WIN
    const QString libexec = QCoreApplication::applicationDirPath()
        + QStringLiteral("/../libexec/linhub/linhub-askpass");
    if (QFileInfo::exists(libexec))
        return QFileInfo(libexec).absoluteFilePath();
#endif

    const QString path = QStandardPaths::findExecutable(QStringLiteral("linhub-askpass"));
    if (!path.isEmpty())
        return path;

    return sibling;
}

void SessionLauncher::preparePassword(const QString &password)
{
    AskPass::store(password, &g_askpassFile);
    qputenv("SSH_ASKPASS", askpassPath().toLocal8Bit());
    qunsetenv("SSH_ASKPASS_REQUIRE");
    qputenv("LINHUB_ASKPASS_FILE", g_askpassFile.toLocal8Bit());
    if (qgetenv("DISPLAY").isEmpty())
#ifdef Q_OS_WIN
        qputenv("DISPLAY", "1");
#else
        qputenv("DISPLAY", ":0");
#endif
}

QString SessionLauncher::sshProgram()
{
#ifdef Q_OS_WIN
    const QString root = QString::fromLocal8Bit(qgetenv("SystemRoot"));
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList bundledCandidates = {
        appDir + QStringLiteral("/ssh.exe"),
        appDir + QStringLiteral("/openssh/ssh.exe"),
    };
    for (const QString &bundled : bundledCandidates) {
        if (QFileInfo::exists(bundled))
            return bundled;
    }
    const QString sys = root + QStringLiteral("/System32/OpenSSH/ssh.exe");
    if (QFileInfo::exists(sys))
        return sys;
    const QString found = QStandardPaths::findExecutable(QStringLiteral("ssh"));
    if (!found.isEmpty())
        return found;
    return QStringLiteral("ssh.exe");
#else
    return QStringLiteral("ssh");
#endif
}

void SessionLauncher::clearPassword()
{
    AskPass::clear(g_askpassFile);
    g_askpassFile.clear();
}

QStringList SessionLauncher::muxArgs(const QString &controlPath, bool asMaster)
{
    QStringList args;
    if (controlPath.isEmpty()) {
        args << QStringLiteral("-o") << QStringLiteral("ControlMaster=no");
        args << QStringLiteral("-o") << QStringLiteral("ControlPath=none");
        return args;
    }
    args << QStringLiteral("-o")
         << (asMaster ? QStringLiteral("ControlMaster=yes") : QStringLiteral("ControlMaster=no"));
    args << QStringLiteral("-o") << (QStringLiteral("ControlPath=") + controlPath);
    args << QStringLiteral("-o") << QStringLiteral("ControlPersist=no");
    return args;
}

QStringList SessionLauncher::sshOptions(const Session &session, bool includeForwards)
{
    QStringList args;
    args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new");
    args << QStringLiteral("-o") << QStringLiteral("UpdateHostKeys=yes");
    args << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=20");
    args << QStringLiteral("-o") << QStringLiteral("GSSAPIAuthentication=no");
    if (session.keepAlive > 0) {
        args << QStringLiteral("-o")
             << QStringLiteral("ServerAliveInterval=%1").arg(session.keepAlive);
    }
    if (session.x11Forward) {
        args << QStringLiteral("-Y");
        args << QStringLiteral("-o") << QStringLiteral("ForwardX11=yes");
        args << QStringLiteral("-o") << QStringLiteral("ForwardX11Trusted=yes");
    } else {
        args << QStringLiteral("-o") << QStringLiteral("ForwardX11=no");
        args << QStringLiteral("-o") << QStringLiteral("ForwardX11Trusted=no");
    }
    if (session.jumps.isEmpty() && !session.jumpHost.trimmed().isEmpty())
        args << QStringLiteral("-o") << QStringLiteral("ProxyJump=%1").arg(session.jumpHost.trimmed());
    if (session.authType == AuthType::Key && !session.keyPath.isEmpty())
        args << QStringLiteral("-i") << session.keyPath;
    if (session.authType == AuthType::Agent)
        args << QStringLiteral("-o") << QStringLiteral("PreferredAuthentications=publickey");
    if (!session.proxyCommand.trimmed().isEmpty())
        args << QStringLiteral("-o") << QStringLiteral("ProxyCommand=%1").arg(session.proxyCommand);
    if (includeForwards) {
        for (const PortForward &f : session.forwards) {
            if (f.bindPort <= 0)
                continue;
            const QString bind = f.bindHost.trimmed().isEmpty()
                                     ? QString()
                                     : (f.bindHost.trimmed() + QLatin1Char(':'));
            const QString type = f.type.trimmed().toUpper();
            if (type == QLatin1String("D")) {
                args << QStringLiteral("-D") << (bind + QString::number(f.bindPort));
            } else if (type == QLatin1String("R")) {
                if (f.destHost.trimmed().isEmpty() || f.destPort <= 0)
                    continue;
                args << QStringLiteral("-R")
                     << (bind + QString::number(f.bindPort) + QLatin1Char(':')
                         + f.destHost.trimmed() + QLatin1Char(':') + QString::number(f.destPort));
            } else {
                if (f.destHost.trimmed().isEmpty() || f.destPort <= 0)
                    continue;
                args << QStringLiteral("-L")
                     << (bind + QString::number(f.bindPort) + QLatin1Char(':')
                         + f.destHost.trimmed() + QLatin1Char(':') + QString::number(f.destPort));
            }
        }
    }
    if (session.sshCompat == QLatin1String("legacy")) {
        args << QStringLiteral("-o")
             << QStringLiteral("KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group14-sha256,diffie-hellman-group-exchange-sha256");
        args << QStringLiteral("-o")
             << QStringLiteral("HostKeyAlgorithms=+ssh-rsa,rsa-sha2-256,rsa-sha2-512");
        args << QStringLiteral("-o")
             << QStringLiteral("PubkeyAcceptedKeyTypes=+ssh-rsa");
    } else if (session.sshCompat == QLatin1String("modern")) {
        args << QStringLiteral("-o")
             << QStringLiteral("KexAlgorithms=curve25519-sha256,curve25519-sha256@libssh.org,ecdh-sha2-nistp256,ecdh-sha2-nistp384,ecdh-sha2-nistp521,diffie-hellman-group-exchange-sha256,diffie-hellman-group14-sha256");
        args << QStringLiteral("-o")
             << QStringLiteral("HostKeyAlgorithms=ssh-ed25519,ecdsa-sha2-nistp256,rsa-sha2-512,rsa-sha2-256");
    }
    if (!session.extraArgs.trimmed().isEmpty())
        args << splitSkipEmpty(session.extraArgs, QLatin1Char(' '));
    return args;
}

QString SessionLauncher::sshTarget(const Session &session)
{
    if (!session.username.trimmed().isEmpty())
        return session.username.trimmed() + QLatin1Char('@') + session.host;
    return session.host;
}

int SessionLauncher::sshPort(const Session &session)
{
    return session.port > 0 ? session.port : 22;
}

bool SessionLauncher::build(const Session &session,
                            const QString &password,
                            QString *program,
                            QStringList *arguments,
                            QString *workingDir,
                            const QString &controlPath)
{
    if (!program || !arguments || !workingDir)
        return false;
    arguments->clear();
    *workingDir = QDir::homePath();

    if (!password.isEmpty() && controlPath.isEmpty())
        preparePassword(password);
    else if (password.isEmpty())
        clearPassword();

    switch (session.protocol) {
    case Protocol::Local: {
#ifdef Q_OS_WIN
        const QString pwsh = QStandardPaths::findExecutable(QStringLiteral("pwsh"));
        if (!pwsh.isEmpty()) {
            *program = pwsh;
        } else {
            *program = QStringLiteral("powershell.exe");
        }
        *arguments = QStringList{QStringLiteral("-NoLogo"), QStringLiteral("-NoExit")};
#else
        *program = QString::fromLocal8Bit(qgetenv("SHELL"));
        if (program->isEmpty())
            *program = QStringLiteral("/bin/bash");
        *arguments = QStringList{QStringLiteral("-l")};
#endif
        return true;
    }
    case Protocol::Telnet: {
#ifdef Q_OS_WIN
        *program = QStringLiteral("telnet.exe");
#else
        *program = QStringLiteral("telnet");
#endif
        *arguments = QStringList{session.host, QString::number(session.port > 0 ? session.port : 23)};
        return true;
    }
    case Protocol::Serial: {
#ifdef Q_OS_WIN
        *program = QStringLiteral("powershell.exe");
        *arguments = QStringList{
            QStringLiteral("-NoLogo"),
            QStringLiteral("-Command"),
            QStringLiteral("Write-Host 'Windows 版暂不支持串口会话，请使用 SSH 或本地 PowerShell。'")
        };
#else
        *program = QStringLiteral("minicom");
        *arguments = QStringList{
            QStringLiteral("-D"), session.serialDevice,
            QStringLiteral("-b"), QString::number(session.serialBaud > 0 ? session.serialBaud : 115200)
        };
#endif
        return true;
    }
    case Protocol::Sftp:
    case Protocol::Ssh: {
        *program = sshProgram();
        QStringList args;
        args << QStringLiteral("-tt");
        args << muxArgs(controlPath, false);
        args << sshOptions(session, controlPath.isEmpty());
        if (controlPath.isEmpty() && !password.isEmpty() && session.authType == AuthType::Password) {
            args << QStringLiteral("-o") << QStringLiteral("PreferredAuthentications=keyboard-interactive,password");
            args << QStringLiteral("-o") << QStringLiteral("PubkeyAuthentication=no");
            args << QStringLiteral("-o") << QStringLiteral("NumberOfPasswordPrompts=3");
        }
        args << QStringLiteral("-p") << QString::number(sshPort(session));
        args << sshTarget(session);
        *arguments = args;
        return true;
    }
    }
    return false;
}

SshMaster::SshMaster(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(50);
    connect(m_timer, &QTimer::timeout, this, &SshMaster::tick);
}

void SshMaster::start(const Session &session, const QString &password)
{
    stop();
    m_error.clear();
    if (session.host.trimmed().isEmpty()) {
        m_error = QStringLiteral("主机地址为空");
        emit failed(m_error);
        return;
    }

#ifdef Q_OS_WIN
    Q_UNUSED(password)
    m_error = QStringLiteral("Windows 使用独立 SSH 会话");
    return;
#endif

#ifdef Q_OS_UNIX
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();
    QDir().mkpath(dir);
    m_path = dir + QStringLiteral("/lh%1-%2.sock").arg(::getpid()).arg(++g_muxSeq);
#else
    m_path = QDir::tempPath() + QStringLiteral("/lh-%1.sock").arg(++g_muxSeq);
#endif
    QFile::remove(m_path);

    if (!password.isEmpty())
        SessionLauncher::preparePassword(password);

    QStringList args;
    args << QStringLiteral("-N") << QStringLiteral("-T");
    args << QStringLiteral("-o") << QStringLiteral("RequestTTY=no");
    args << QStringLiteral("-o") << QStringLiteral("LogLevel=ERROR");
    args << SessionLauncher::muxArgs(m_path, true);
    QStringList opts = SessionLauncher::sshOptions(session, true);
    args << opts;
    if (!password.isEmpty() && session.authType == AuthType::Password) {
        args << QStringLiteral("-o") << QStringLiteral("PreferredAuthentications=keyboard-interactive,password");
        args << QStringLiteral("-o") << QStringLiteral("PubkeyAuthentication=no");
        args << QStringLiteral("-o") << QStringLiteral("NumberOfPasswordPrompts=3");
    }
    args << QStringLiteral("-p") << QString::number(SessionLauncher::sshPort(session));
    args << SessionLauncher::sshTarget(session);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("SSH_ASKPASS"), SessionLauncher::askpassPath());
    env.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
    if (!g_askpassFile.isEmpty())
        env.insert(QStringLiteral("LINHUB_ASKPASS_FILE"), g_askpassFile);
    if (env.value(QStringLiteral("DISPLAY")).isEmpty())
        env.insert(QStringLiteral("DISPLAY"), QStringLiteral(":0"));

    m_proc.setProcessEnvironment(env);
    m_proc.setStandardInputFile(QProcess::nullDevice());
    m_proc.setWorkingDirectory(QDir::homePath());
    m_proc.start(SessionLauncher::sshProgram(), args);
    m_deadline.start();
    m_timer->start();
}

void SshMaster::tick()
{
    if (m_proc.state() == QProcess::Starting)
        return;
    if (m_proc.state() != QProcess::Running) {
        m_error = QString::fromUtf8(m_proc.readAllStandardError()).trimmed();
        if (m_error.isEmpty())
            m_error = QStringLiteral("无法连接到主机");
        const QString err = m_error;
        m_timer->stop();
        QFile::remove(m_path);
        m_path.clear();
        emit failed(err);
        return;
    }
    if (QFileInfo::exists(m_path)) {
        m_timer->stop();
        emit ready();
        return;
    }
    if (m_deadline.elapsed() > 20000) {
        m_error = QStringLiteral("连接超时");
        const QString err = m_error;
        stop();
        emit failed(err);
    }
}

void SshMaster::stop()
{
    if (m_timer)
        m_timer->stop();
    if (!m_path.isEmpty()) {
        QProcess exitp;
        exitp.setStandardInputFile(QProcess::nullDevice());
        exitp.start(SessionLauncher::sshProgram(),
                    QStringList() << QStringLiteral("-O") << QStringLiteral("exit")
                                  << QStringLiteral("-o") << (QStringLiteral("ControlPath=") + m_path)
                                  << QStringLiteral("dummy"));
        exitp.waitForFinished(1500);
        QFile::remove(m_path);
        m_path.clear();
    }
    if (m_proc.state() != QProcess::NotRunning) {
        m_proc.terminate();
        if (!m_proc.waitForFinished(1500)) {
            m_proc.kill();
            m_proc.waitForFinished(1000);
        }
    }
}
