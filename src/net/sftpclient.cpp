#include "net/sftpclient.h"
#include "net/sessionlauncher.h"
#include "util/qtcompat.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QProcess>
#include <QRegularExpression>
#include <QtGlobal>

namespace {

QString shellQuote(const QString &s)
{
    QString t = s;
    t.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + t + QLatin1Char('\'');
}

QString normalizeRemotePath(QString path)
{
    path = path.trimmed();
    if (path.isEmpty() || path == QLatin1String("."))
        return QStringLiteral(".");
    const bool abs = path.startsWith(QLatin1Char('/'));
    QStringList parts;
    for (const QString &seg : splitSkipEmpty(path, QLatin1Char('/'))) {
        if (seg == QLatin1String("."))
            continue;
        if (seg == QLatin1String("..")) {
            if (!parts.isEmpty())
                parts.removeLast();
            continue;
        }
        parts.append(seg);
    }
    QString out = parts.join(QLatin1Char('/'));
    if (abs)
        out.prepend(QLatin1Char('/'));
    if (out.isEmpty())
        out = abs ? QStringLiteral("/") : QStringLiteral(".");
    return out;
}

} // namespace

SftpClient::SftpClient(QObject *parent)
    : QObject(parent)
{
}

SftpClient::~SftpClient()
{
    abortActive();
}

void SftpClient::configure(const Session &session, const QString &password, const QString &controlPath)
{
    m_session = session;
    m_password = password;
    m_controlPath = controlPath;
}

void SftpClient::clear()
{
    abortActive();
    m_session = Session();
    m_password.clear();
    m_controlPath.clear();
}

QStringList SftpClient::sshCmdArgs(bool nullStdin) const
{
    QStringList args;
    args << SessionLauncher::muxArgs(m_controlPath, false);
    args << QStringLiteral("-T") << QStringLiteral("-q");
    if (nullStdin)
        args << QStringLiteral("-n");
    args << QStringLiteral("-o") << QStringLiteral("RequestTTY=no");
    args << QStringLiteral("-o") << QStringLiteral("LogLevel=ERROR");
    args << SessionLauncher::sshOptions(m_session);
    args << QStringLiteral("-p") << QString::number(SessionLauncher::sshPort(m_session));
    args << SessionLauncher::sshTarget(m_session);
    return args;
}

void SftpClient::prepareAuth()
{
    if (!m_password.isEmpty())
        SessionLauncher::preparePassword(m_password);
}

bool SftpClient::isConfigured() const
{
    return !m_session.host.trimmed().isEmpty();
}

bool SftpClient::isTransferring() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

void SftpClient::abortTransfer()
{
    abortActive();
}

void SftpClient::abortActive()
{
    if (m_proc) {
        m_proc->disconnect();
        m_proc->kill();
        m_proc->waitForFinished(500);
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}

bool SftpClient::runRemote(const QString &cmd, QString *out, QString *err, int timeoutMs)
{
    prepareAuth();
    QProcess proc;
    QStringList args = sshCmdArgs(true);
    args << cmd;
    proc.setStandardInputFile(QProcess::nullDevice());
    proc.start(SessionLauncher::sshProgram(), args);
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(2000);
        if (err)
            *err = QStringLiteral("远程命令超时");
        return false;
    }
    if (out)
        *out = QString::fromUtf8(proc.readAllStandardOutput());
    if (err)
        *err = QString::fromUtf8(proc.readAllStandardError());
    return proc.exitCode() == 0;
}

void SftpClient::list(const QString &path)
{
    prepareAuth();
    auto *proc = new QProcess(this);
    const QString want = normalizeRemotePath(path);
    QStringList args = sshCmdArgs(true);
    const QString cmd = QStringLiteral(
                            "cd -- %1 && pwd && { LC_ALL=C ls -la --time-style=long-iso 2>/dev/null || LC_ALL=C ls -la; }")
                            .arg(shellQuote(want));
    args << cmd;
    connect(proc, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [this, proc, want](int code, QProcess::ExitStatus) {
                const QString out = QString::fromUtf8(proc->readAllStandardOutput());
                const QString err = QString::fromUtf8(proc->readAllStandardError());
                proc->deleteLater();
                if (code != 0) {
                    emit errorOccurred(err.trimmed().isEmpty()
                                           ? QStringLiteral("列出远程目录失败")
                                           : err.trimmed());
                    return;
                }
                const QStringList lines = out.split(QLatin1Char('\n'));
                QString abs = want;
                int start = 0;
                for (int i = 0; i < lines.size(); ++i) {
                    const QString t = lines.at(i).trimmed();
                    if (t.startsWith(QLatin1Char('/')) || t.startsWith(QLatin1Char('~'))) {
                        abs = t;
                        start = i + 1;
                        break;
                    }
                }
                abs = normalizeRemotePath(abs);
                QVector<SftpEntry> entries;
                SftpEntry up;
                up.name = QStringLiteral("..");
                up.path = abs == QLatin1String("/") ? QStringLiteral("/")
                                                    : abs + QStringLiteral("/..");
                up.isDir = true;
                entries.push_back(up);
                for (int i = start; i < lines.size(); ++i) {
                    QString line = lines.at(i).trimmed();
                    if (line.isEmpty() || line.startsWith(QLatin1String("total")))
                        continue;
                    const QStringList parts = splitSkipEmpty(line, QRegularExpression(QStringLiteral("\\s+")));
                    if (parts.size() < 8)
                        continue;
                    SftpEntry e;
                    e.mode = parts[0];
                    e.isDir = e.mode.startsWith(QLatin1Char('d'));
                    e.isLink = e.mode.startsWith(QLatin1Char('l'));
                    e.isExec = e.mode.size() >= 10
                        && (e.mode.at(3) == QLatin1Char('x') || e.mode.at(3) == QLatin1Char('s')
                            || e.mode.at(6) == QLatin1Char('x') || e.mode.at(9) == QLatin1Char('x'));
                    e.size = parts[4].toLongLong();
                    if (parts[5].contains(QLatin1Char('-')) && parts[5].size() >= 8) {
                        if (parts.size() < 8)
                            continue;
                        e.mtime = parts[5] + QLatin1Char(' ') + parts[6];
                        e.name = parts.mid(7).join(QLatin1Char(' '));
                    } else {
                        if (parts.size() < 9)
                            continue;
                        e.mtime = parts.mid(5, 3).join(QLatin1Char(' '));
                        e.name = parts.mid(8).join(QLatin1Char(' '));
                    }
                    const int arrow = e.name.indexOf(QLatin1String(" -> "));
                    if (arrow > 0)
                        e.name = e.name.left(arrow);
                    if (e.name == QLatin1String(".") || e.name == QLatin1String("..") || e.name.isEmpty())
                        continue;
                    e.path = abs == QLatin1String("/") ? QLatin1Char('/') + e.name
                                                       : abs + QLatin1Char('/') + e.name;
                    entries.push_back(e);
                }
                emit listed(abs, entries);
            });
    proc->setStandardInputFile(QProcess::nullDevice());
    proc->start(SessionLauncher::sshProgram(), args);
}

void SftpClient::onDownloadReadyRead()
{
    if (!m_proc || !m_file)
        return;
    const QByteArray chunk = m_proc->readAllStandardOutput();
    if (chunk.isEmpty())
        return;
    m_file->write(chunk);
    m_xferDone += chunk.size();
    emit bytesTransferred(m_xferDone, m_xferTotal);
}

void SftpClient::onTransferFinished(int code)
{
    if (m_proc && m_file && !m_uploading)
        onDownloadReadyRead();
    const QString err = m_proc ? QString::fromUtf8(m_proc->readAllStandardError()) : QString();
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    if (m_proc) {
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    emit transferFinished(code == 0, code == 0 ? QString() : err.trimmed());
}

void SftpClient::pumpUpload()
{
    if (!m_proc || !m_file || !m_uploading)
        return;
    if (m_proc->bytesToWrite() > 256 * 1024)
        return;
    if (m_file->atEnd()) {
        m_proc->closeWriteChannel();
        return;
    }
    const QByteArray chunk = m_file->read(64 * 1024);
    if (chunk.isEmpty()) {
        m_proc->closeWriteChannel();
        return;
    }
    m_proc->write(chunk);
    m_xferDone += chunk.size();
    emit bytesTransferred(m_xferDone, m_xferTotal);
}

qint64 SftpClient::remoteFileSize(const QString &path)
{
    QString out;
    if (!runRemote(QStringLiteral("stat -c %s -- %1 2>/dev/null || wc -c < %1")
                       .arg(shellQuote(path)),
                   &out, nullptr, 15000))
        return -1;
    const QString t = out.trimmed();
    const QString last = t.contains(QLatin1Char('\n')) ? t.mid(t.lastIndexOf(QLatin1Char('\n')) + 1)
                                                       : t;
    bool ok = false;
    const qint64 n = last.trimmed().toLongLong(&ok);
    return ok ? n : -1;
}

void SftpClient::startDownload(const QString &remote, const QString &local, qint64 expectedSize,
                               bool resume)
{
    abortActive();
    prepareAuth();
    QDir().mkpath(QFileInfo(local).absolutePath());
    qint64 offset = 0;
    if (resume && QFileInfo::exists(local)) {
        offset = QFileInfo(local).size();
        if (expectedSize > 0 && offset >= expectedSize && offset > 0) {
            emit bytesTransferred(offset, expectedSize);
            emit transferFinished(true, QString());
            return;
        }
    }
    m_file = new QFile(local);
    const QIODevice::OpenMode mode = (resume && offset > 0)
                                         ? (QIODevice::WriteOnly | QIODevice::Append)
                                         : (QIODevice::WriteOnly | QIODevice::Truncate);
    if (!m_file->open(mode)) {
        delete m_file;
        m_file = nullptr;
        emit transferFinished(false, QStringLiteral("无法写入本地文件"));
        return;
    }
    m_uploading = false;
    m_xferTotal = expectedSize;
    m_xferDone = offset;
    emit bytesTransferred(m_xferDone, m_xferTotal);
    m_proc = new QProcess(this);
    QStringList args = sshCmdArgs(true);
    const QString cmd = offset > 0
                            ? QStringLiteral("tail -c +%1 -- %2").arg(offset + 1).arg(shellQuote(remote))
                            : QStringLiteral("cat -- %1").arg(shellQuote(remote));
    args << cmd;
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &SftpClient::onDownloadReadyRead);
    connect(m_proc, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) { onTransferFinished(code); });
    m_proc->setStandardInputFile(QProcess::nullDevice());
    m_proc->start(SessionLauncher::sshProgram(), args);
}

void SftpClient::startUpload(const QString &local, const QString &remote, qint64 expectedSize,
                             bool resume)
{
    abortActive();
    prepareAuth();
    m_file = new QFile(local);
    if (!m_file->open(QIODevice::ReadOnly)) {
        delete m_file;
        m_file = nullptr;
        emit transferFinished(false, QStringLiteral("无法读取本地文件"));
        return;
    }
    m_uploading = true;
    m_xferTotal = expectedSize > 0 ? expectedSize : m_file->size();
    qint64 offset = 0;
    if (resume) {
        const qint64 remoteSz = remoteFileSize(remote);
        if (remoteSz > 0 && remoteSz < m_file->size())
            offset = remoteSz;
        else if (remoteSz >= m_file->size() && m_file->size() > 0) {
            m_file->close();
            delete m_file;
            m_file = nullptr;
            emit bytesTransferred(m_xferTotal, m_xferTotal);
            emit transferFinished(true, QString());
            return;
        }
    }
    m_file->seek(offset);
    m_xferDone = offset;
    emit bytesTransferred(m_xferDone, m_xferTotal);
    m_proc = new QProcess(this);
    QStringList args = sshCmdArgs(false);
    args << (offset > 0 ? QStringLiteral("cat >> %1").arg(shellQuote(remote))
                        : QStringLiteral("cat > %1").arg(shellQuote(remote)));
    connect(m_proc, &QIODevice::bytesWritten, this, [this](qint64) { pumpUpload(); });
    connect(m_proc, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) { onTransferFinished(code); });
    connect(m_proc, &QProcess::started, this, [this] { pumpUpload(); });
    m_proc->start(SessionLauncher::sshProgram(), args);
}

bool SftpClient::downloadBlocking(const QString &remote, const QString &local, int timeoutMs,
                                  qint64 expectedSize, bool resume)
{
    prepareAuth();
    QDir().mkpath(QFileInfo(local).absolutePath());
    qint64 offset = 0;
    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Truncate;
    if (resume && QFileInfo::exists(local)) {
        offset = QFileInfo(local).size();
        if (expectedSize > 0 && offset >= expectedSize && offset > 0)
            return true;
        if (offset > 0)
            mode = QIODevice::WriteOnly | QIODevice::Append;
    }
    QFile out(local);
    if (!out.open(mode))
        return false;
    QProcess proc;
    QStringList args = sshCmdArgs(true);
    args << (offset > 0 ? QStringLiteral("tail -c +%1 -- %2").arg(offset + 1).arg(shellQuote(remote))
                        : QStringLiteral("cat -- %1").arg(shellQuote(remote)));
    proc.setStandardInputFile(QProcess::nullDevice());
    proc.start(SessionLauncher::sshProgram(), args);
    if (!proc.waitForStarted(8000))
        return false;
    qint64 got = offset;
    int idle = 0;
    while (proc.state() != QProcess::NotRunning || proc.bytesAvailable() > 0) {
        if (!proc.waitForReadyRead(200)) {
            idle += 200;
            if (proc.state() == QProcess::NotRunning)
                break;
            if (idle > timeoutMs)
                break;
            continue;
        }
        idle = 0;
        const QByteArray chunk = proc.readAllStandardOutput();
        out.write(chunk);
        got += chunk.size();
        emit bytesTransferred(got, expectedSize);
    }
    out.write(proc.readAllStandardOutput());
    proc.waitForFinished(2000);
    out.close();
    return proc.exitCode() == 0 && QFileInfo::exists(local);
}

bool SftpClient::chmodPath(const QString &remote, const QString &modeOctal, QString *err)
{
    return runRemote(QStringLiteral("chmod %1 -- %2").arg(modeOctal, shellQuote(remote)), nullptr, err);
}

bool SftpClient::removePath(const QString &remote, bool recursive, QString *err)
{
    const QString cmd = recursive ? QStringLiteral("rm -rf -- %1") : QStringLiteral("rm -f -- %1");
    return runRemote(cmd.arg(shellQuote(remote)), nullptr, err);
}

bool SftpClient::mkdirPath(const QString &remote, QString *err)
{
    return runRemote(QStringLiteral("mkdir -p -- %1").arg(shellQuote(remote)), nullptr, err);
}

bool SftpClient::createFile(const QString &remote, QString *err)
{
    return runRemote(QStringLiteral("touch -- %1").arg(shellQuote(remote)), nullptr, err);
}

bool SftpClient::renamePath(const QString &from, const QString &to, QString *err)
{
    return runRemote(QStringLiteral("mv -n -- %1 %2").arg(shellQuote(from), shellQuote(to)), nullptr, err);
}

bool SftpClient::mkdirPaths(const QStringList &remotes, QString *err)
{
    if (remotes.isEmpty())
        return true;
    QStringList pending;
    auto flush = [&]() -> bool {
        if (pending.isEmpty())
            return true;
        QStringList quoted;
        for (const QString &p : pending)
            quoted << shellQuote(p);
        pending.clear();
        return runRemote(QStringLiteral("mkdir -p -- ") + quoted.join(QLatin1Char(' ')), nullptr, err, 60000);
    };
    for (const QString &p : remotes) {
        if (p.trimmed().isEmpty())
            continue;
        pending.append(p);
        if (pending.size() >= 40) {
            if (!flush())
                return false;
        }
    }
    return flush();
}

QString SftpClient::resolvePath(const QString &path, QString *err)
{
    QString out;
    QString e;
    const QString want = normalizeRemotePath(path);
    if (!runRemote(QStringLiteral("cd -- %1 && pwd").arg(shellQuote(want)), &out, &e, 15000)) {
        if (err)
            *err = e.trimmed().isEmpty() ? QStringLiteral("目录不存在") : e.trimmed();
        return {};
    }
    QString abs = out.trimmed();
    const int nl = abs.lastIndexOf(QLatin1Char('\n'));
    if (nl >= 0)
        abs = abs.mid(nl + 1).trimmed();
    if (abs.isEmpty()) {
        if (err)
            *err = QStringLiteral("无法解析远程路径");
        return {};
    }
    return normalizeRemotePath(abs);
}

QStringList SftpClient::listRemoteFiles(const QString &dir)
{
    QString out;
    if (!runRemote(QStringLiteral("find %1 -type f").arg(shellQuote(normalizeRemotePath(dir))), &out, nullptr, 60000))
        return {};
    QStringList files;
    for (const QString &line : out.split(QLatin1Char('\n'))) {
        const QString t = line.trimmed();
        if (!t.isEmpty())
            files.append(t);
    }
    return files;
}

QStringList SftpClient::listRemoteDirs(const QString &dir)
{
    QString out;
    if (!runRemote(QStringLiteral("find %1 -mindepth 1 -type d")
                       .arg(shellQuote(normalizeRemotePath(dir))),
                   &out, nullptr, 60000))
        return {};
    QStringList dirs;
    for (const QString &line : out.split(QLatin1Char('\n'))) {
        const QString t = line.trimmed();
        if (!t.isEmpty())
            dirs.append(t);
    }
    return dirs;
}
