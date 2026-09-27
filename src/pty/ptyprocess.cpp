#include "pty/ptyprocess.h"
#ifdef Q_OS_WIN
#include "pty/winptyreader.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifndef HPCON
typedef VOID *HPCON;
#endif
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif
#endif

#include <QSocketNotifier>
#include <QtGlobal>
#include <QByteArray>
#include <QVector>
#include <string>
#include <vector>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#ifdef __linux__
#include <pty.h>
#include <utmp.h>
#else
#include <util.h>
#endif
#endif

PtyProcess::PtyProcess(QObject *parent)
    : QObject(parent)
{
}

PtyProcess::~PtyProcess()
{
    terminate(false);
}

bool PtyProcess::start(const QString &program,
                       const QStringList &arguments,
                       const QString &workingDir)
{
    if (isRunning())
        terminate(false);
    return spawn(program, arguments, workingDir);
}

void PtyProcess::write(const QByteArray &data)
{
#ifdef Q_OS_UNIX
    if (m_masterFd < 0 || data.isEmpty())
        return;
    const char *p = data.constData();
    int left = data.size();
    while (left > 0) {
        const ssize_t n = ::write(m_masterFd, p, static_cast<size_t>(left));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        p += n;
        left -= static_cast<int>(n);
    }
#elif defined(Q_OS_WIN)
    if (data.isEmpty() || !m_inWrite)
        return;
    HANDLE h = reinterpret_cast<HANDLE>(m_inWrite);
    const char *p = data.constData();
    int left = data.size();
    while (left > 0) {
        DWORD n = 0;
        if (!WriteFile(h, p, static_cast<DWORD>(left), &n, nullptr))
            break;
        p += n;
        left -= int(n);
    }
#endif
}

void PtyProcess::setWinsize(int cols, int rows)
{
    cols = qMax(2, cols);
    rows = qMax(1, rows);
    if (cols == m_cols && rows == m_rows && isRunning())
        return;
    m_cols = cols;
    m_rows = rows;
#ifdef Q_OS_UNIX
    if (m_masterFd < 0)
        return;
    struct winsize ws {};
    ws.ws_col = static_cast<unsigned short>(m_cols);
    ws.ws_row = static_cast<unsigned short>(m_rows);
    ioctl(m_masterFd, TIOCSWINSZ, &ws);
#elif defined(Q_OS_WIN)
    if (m_pty) {
        typedef HRESULT (WINAPI *ResizeFn)(HPCON, COORD);
        static ResizeFn resizeFn = reinterpret_cast<ResizeFn>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "ResizePseudoConsole"));
        if (resizeFn) {
            COORD sz;
            sz.X = static_cast<SHORT>(m_cols);
            sz.Y = static_cast<SHORT>(m_rows);
            resizeFn(reinterpret_cast<HPCON>(m_pty), sz);
        }
    }
#endif
}

void PtyProcess::terminate(bool notify)
{
#ifdef Q_OS_UNIX
    const bool was = m_pid > 0 || m_masterFd >= 0;
    if (m_pid > 0) {
        ::kill(m_pid, SIGHUP);
        ::kill(m_pid, SIGTERM);
        int status = 0;
        waitpid(m_pid, &status, WNOHANG);
        m_pid = -1;
    }
    closeMaster();
    if (notify && was)
        emit finished(0);
#elif defined(Q_OS_WIN)
    const bool was = m_running;
    closeWindows();
    if (notify && was)
        emit finished(0);
#else
    Q_UNUSED(notify)
#endif
}

bool PtyProcess::isRunning() const
{
#ifdef Q_OS_UNIX
    if (m_pid <= 0)
        return false;
    if (::kill(m_pid, 0) == 0)
        return true;
    return errno == EPERM;
#elif defined(Q_OS_WIN)
    return m_running && m_process != 0;
#else
    return false;
#endif
}

void PtyProcess::onMasterReadable()
{
#ifdef Q_OS_UNIX
    char buf[4096];
    const ssize_t n = ::read(m_masterFd, buf, sizeof(buf));
    if (n > 0) {
        emit readyRead(QByteArray(buf, static_cast<int>(n)));
        return;
    }
    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) {
        int status = 0;
        int code = 0;
        if (m_pid > 0) {
            waitpid(m_pid, &status, WNOHANG);
            if (WIFEXITED(status))
                code = WEXITSTATUS(status);
        }
        closeMaster();
        m_pid = -1;
        emit finished(code);
    }
#endif
}

void PtyProcess::closeMaster()
{
#ifdef Q_OS_UNIX
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    if (m_masterFd >= 0) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }
#endif
}

bool PtyProcess::spawn(const QString &program, const QStringList &arguments, const QString &workingDir)
{
#ifdef Q_OS_WIN
    return spawnWindows(program, arguments, workingDir);
#elif !defined(Q_OS_UNIX)
    Q_UNUSED(program)
    Q_UNUSED(arguments)
    Q_UNUSED(workingDir)
    m_lastError = QStringLiteral("当前系统不支持伪终端");
    return false;
#else
    struct winsize ws {};
    ws.ws_col = static_cast<unsigned short>(m_cols);
    ws.ws_row = static_cast<unsigned short>(m_rows);

    int slaveFd = -1;
    const int ret = openpty(&m_masterFd, &slaveFd, nullptr, nullptr, &ws);
    if (ret != 0) {
        m_lastError = QStringLiteral("openpty 失败: %1").arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }

    fcntl(m_masterFd, F_SETFL, O_NONBLOCK);

    const pid_t pid = fork();
    if (pid < 0) {
        m_lastError = QStringLiteral("fork 失败");
        ::close(m_masterFd);
        ::close(slaveFd);
        m_masterFd = -1;
        return false;
    }

    if (pid == 0) {
        ::close(m_masterFd);
        setsid();
        ioctl(slaveFd, TIOCSCTTY, nullptr);
        dup2(slaveFd, STDIN_FILENO);
        dup2(slaveFd, STDOUT_FILENO);
        dup2(slaveFd, STDERR_FILENO);
        if (slaveFd > STDERR_FILENO)
            ::close(slaveFd);

        if (!workingDir.isEmpty())
            chdir(workingDir.toLocal8Bit().constData());

        unsetenv("SSH_ASKPASS_REQUIRE");
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        setenv("CLICOLOR", "1", 1);
        setenv("FORCE_COLOR", "1", 0);
        setenv("CLICOLOR_FORCE", "0", 0);
        if (!getenv("LS_COLORS") || QByteArray(getenv("LS_COLORS")).isEmpty()) {
            setenv("LS_COLORS",
                   "rs=0:di=01;34:ln=01;36:mh=00:pi=40;33:so=01;35:do=01;35:"
                   "bd=40;33;01:cd=40;33;01:or=40;31;01:ex=01;32:"
                   "*.tar=01;31:*.tgz=01;31:*.zip=01;31:*.gz=01;31:*.bz2=01;31:"
                   "*.xz=01;31:*.7z=01;31:*.rar=01;31:"
                   "*.jpg=01;35:*.jpeg=01;35:*.png=01;35:*.gif=01;35:*.bmp=01;35:*.svg=01;35:"
                   "*.mp3=00;36:*.mp4=00;36:*.mkv=00;36:*.avi=00;36:"
                   "*.pdf=01;33:*.doc=01;33:*.txt=00;33:"
                   "*.c=00;32:*.h=00;32:*.cpp=00;32:*.py=00;32:*.sh=01;32",
                   1);
        }
        if (!getenv("LANG") || QByteArray(getenv("LANG")).isEmpty())
            setenv("LANG", "zh_CN.UTF-8", 1);

        QStringList argv = arguments;
        argv.prepend(program);
        QVector<QByteArray> storage;
        storage.reserve(argv.size());
        std::vector<char *> cargv;
        cargv.reserve(static_cast<size_t>(argv.size()) + 1);
        for (const QString &a : argv) {
            storage.push_back(a.toLocal8Bit());
            cargv.push_back(storage.back().data());
        }
        cargv.push_back(nullptr);
        execvp(cargv[0], cargv.data());
        _exit(127);
    }

    ::close(slaveFd);
    m_pid = pid;
    m_notifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this] { onMasterReadable(); });
    return true;
#endif
}

#ifdef Q_OS_WIN

bool PtyProcess::hasConPty()
{
    return GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "CreatePseudoConsole") != nullptr;
}

void PtyProcess::startWinReader()
{
    auto *reader = new WinPtyReader(m_outRead, this);
    m_reader = reader;
    connect(reader, &WinPtyReader::chunk, this, &PtyProcess::onWinChunk, Qt::QueuedConnection);
    connect(reader, &WinPtyReader::done, this, &PtyProcess::onWinDone, Qt::QueuedConnection);
    reader->start();
}

bool PtyProcess::spawnWindowsPipes(const QString &program,
                                   const QStringList &arguments,
                                   const QString &workingDir,
                                   const QString &cmdLine)
{
    Q_UNUSED(arguments)
    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE inRead = INVALID_HANDLE_VALUE;
    HANDLE inWrite = INVALID_HANDLE_VALUE;
    HANDLE outRead = INVALID_HANDLE_VALUE;
    HANDLE outWrite = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&inRead, &inWrite, &sa, 0) || !CreatePipe(&outRead, &outWrite, &sa, 0)) {
        m_lastError = QStringLiteral("创建管道失败");
        return false;
    }
    SetHandleInformation(inWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = inRead;
    si.hStdOutput = outWrite;
    si.hStdError = outWrite;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    std::wstring cmdW = cmdLine.toStdWString();
    std::wstring cwdW = workingDir.toStdWString();
    SetEnvironmentVariableW(L"TERM", L"xterm-256color");
    SetEnvironmentVariableW(L"COLORTERM", L"truecolor");

    const BOOL ok = CreateProcessW(
        nullptr,
        cmdW.empty() ? nullptr : &cmdW[0],
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
        nullptr,
        cwdW.empty() ? nullptr : cwdW.c_str(),
        &si,
        &pi);

    CloseHandle(inRead);
    CloseHandle(outWrite);
    if (!ok) {
        CloseHandle(inWrite);
        CloseHandle(outRead);
        m_lastError = QStringLiteral("无法启动 %1").arg(program);
        return false;
    }

    m_inWrite = reinterpret_cast<quintptr>(inWrite);
    m_outRead = reinterpret_cast<quintptr>(outRead);
    m_process = reinterpret_cast<quintptr>(pi.hProcess);
    m_thread = reinterpret_cast<quintptr>(pi.hThread);
    m_pid = int(pi.dwProcessId);
    m_running = true;
    startWinReader();
    return true;
}

void PtyProcess::onWinChunk(const QByteArray &data)
{
    if (!data.isEmpty())
        emit readyRead(data);
}

void PtyProcess::onWinDone()
{
    DWORD code = 0;
    if (m_process)
        GetExitCodeProcess(reinterpret_cast<HANDLE>(m_process), &code);
    closeWindows();
    emit finished(int(code));
}

void PtyProcess::closeWindows()
{
    m_running = false;
    if (m_reader) {
        m_reader->disconnect(this);
        if (m_outRead) {
            CancelIoEx(reinterpret_cast<HANDLE>(m_outRead), nullptr);
            CloseHandle(reinterpret_cast<HANDLE>(m_outRead));
            m_outRead = 0;
        }
        m_reader->wait(1500);
        m_reader->deleteLater();
        m_reader = nullptr;
    } else if (m_outRead) {
        CloseHandle(reinterpret_cast<HANDLE>(m_outRead));
        m_outRead = 0;
    }
    if (m_inWrite) {
        CloseHandle(reinterpret_cast<HANDLE>(m_inWrite));
        m_inWrite = 0;
    }
    typedef void (WINAPI *CloseFn)(HPCON);
    static auto closeFn = reinterpret_cast<CloseFn>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "ClosePseudoConsole"));
    if (m_pty && closeFn)
        closeFn(reinterpret_cast<HPCON>(m_pty));
    m_pty = 0;
    if (m_thread) {
        CloseHandle(reinterpret_cast<HANDLE>(m_thread));
        m_thread = 0;
    }
    if (m_process) {
        HANDLE p = reinterpret_cast<HANDLE>(m_process);
        if (WaitForSingleObject(p, 0) == WAIT_TIMEOUT)
            TerminateProcess(p, 1);
        CloseHandle(p);
        m_process = 0;
    }
    m_pid = -1;
}

static QString quoteWinArg(const QString &s)
{
    if (s.isEmpty())
        return QStringLiteral("\"\"");
    bool need = s.contains(QLatin1Char(' ')) || s.contains(QLatin1Char('\t'))
        || s.contains(QLatin1Char('"'));
    if (!need)
        return s;
    QString out(QLatin1Char('"'));
    for (int i = 0; i < s.size(); ++i) {
        int slashes = 0;
        while (i < s.size() && s.at(i) == QLatin1Char('\\')) {
            ++slashes;
            ++i;
        }
        if (i >= s.size()) {
            out += QString(slashes * 2, QLatin1Char('\\'));
            break;
        }
        if (s.at(i) == QLatin1Char('"')) {
            out += QString(slashes * 2 + 1, QLatin1Char('\\'));
            out += QLatin1Char('"');
        } else {
            out += QString(slashes, QLatin1Char('\\'));
            out += s.at(i);
        }
    }
    out += QLatin1Char('"');
    return out;
}

bool PtyProcess::spawnWindows(const QString &program,
                              const QStringList &arguments,
                              const QString &workingDir)
{
    closeWindows();

    QString cmd = quoteWinArg(program);
    for (const QString &a : arguments)
        cmd += QLatin1Char(' ') + quoteWinArg(a);

    typedef HRESULT (WINAPI *CreateFn)(COORD, HANDLE, HANDLE, DWORD, HPCON *);
    auto createFn = reinterpret_cast<CreateFn>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "CreatePseudoConsole"));
    if (!createFn)
        return spawnWindowsPipes(program, arguments, workingDir, cmd);

    HANDLE pipePtyIn = INVALID_HANDLE_VALUE;
    HANDLE pipeOurWrite = INVALID_HANDLE_VALUE;
    HANDLE pipeOurRead = INVALID_HANDLE_VALUE;
    HANDLE pipePtyOut = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&pipePtyIn, &pipeOurWrite, nullptr, 0)
        || !CreatePipe(&pipeOurRead, &pipePtyOut, nullptr, 0)) {
        m_lastError = QStringLiteral("创建管道失败");
        return false;
    }

    COORD size;
    size.X = static_cast<SHORT>(qMax(2, m_cols));
    size.Y = static_cast<SHORT>(qMax(1, m_rows));
    HPCON hpc = nullptr;
    const HRESULT hr = createFn(size, pipePtyIn, pipePtyOut, 0, &hpc);
    CloseHandle(pipePtyIn);
    CloseHandle(pipePtyOut);
    if (FAILED(hr) || !hpc) {
        CloseHandle(pipeOurWrite);
        CloseHandle(pipeOurRead);
        m_lastError = QStringLiteral("CreatePseudoConsole 失败");
        return false;
    }

    SIZE_T attrBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrBytes);
    auto *attrList = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, attrBytes));
    if (!attrList
        || !InitializeProcThreadAttributeList(attrList, 1, 0, &attrBytes)
        || !UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                      hpc, sizeof(hpc), nullptr, nullptr)) {
        if (attrList)
            HeapFree(GetProcessHeap(), 0, attrList);
        typedef void (WINAPI *CloseFn)(HPCON);
        auto closeFn = reinterpret_cast<CloseFn>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "ClosePseudoConsole"));
        if (closeFn)
            closeFn(hpc);
        CloseHandle(pipeOurWrite);
        CloseHandle(pipeOurRead);
        m_lastError = QStringLiteral("初始化进程属性失败");
        return false;
    }

    STARTUPINFOEXW siex;
    ZeroMemory(&siex, sizeof(siex));
    siex.StartupInfo.cb = sizeof(siex);
    siex.lpAttributeList = attrList;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    const QString cwd = workingDir;
    std::wstring cmdW = cmd.toStdWString();
    std::wstring cwdW = cwd.toStdWString();
    SetEnvironmentVariableW(L"TERM", L"xterm-256color");
    SetEnvironmentVariableW(L"COLORTERM", L"truecolor");

    const BOOL ok = CreateProcessW(
        nullptr,
        cmdW.empty() ? nullptr : &cmdW[0],
        nullptr,
        nullptr,
        FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        nullptr,
        cwdW.empty() ? nullptr : cwdW.c_str(),
        &siex.StartupInfo,
        &pi);

    DeleteProcThreadAttributeList(attrList);
    HeapFree(GetProcessHeap(), 0, attrList);

    if (!ok) {
        typedef void (WINAPI *CloseFn)(HPCON);
        auto closeFn = reinterpret_cast<CloseFn>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "ClosePseudoConsole"));
        if (closeFn)
            closeFn(hpc);
        CloseHandle(pipeOurWrite);
        CloseHandle(pipeOurRead);
        m_lastError = QStringLiteral("无法启动 %1").arg(program);
        return false;
    }

    m_pty = reinterpret_cast<quintptr>(hpc);
    m_inWrite = reinterpret_cast<quintptr>(pipeOurWrite);
    m_outRead = reinterpret_cast<quintptr>(pipeOurRead);
    m_process = reinterpret_cast<quintptr>(pi.hProcess);
    m_thread = reinterpret_cast<quintptr>(pi.hThread);
    m_pid = int(pi.dwProcessId);
    m_running = true;
    startWinReader();
    return true;
}

#endif
