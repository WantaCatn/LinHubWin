#include "terminal/terminalwidget.h"

#include "app/appsettings.h"
#include "logging/sessionlogger.h"
#include "net/sessionlauncher.h"
#include "util/crypto.h"
#include "util/qtcompat.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QColor>
#include <QAction>
#include <QMenu>
#include <QEvent>
#include <QFocusEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTimer>
#include <QWheelEvent>
#include <QtGlobal>

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent)
    , m_parser(&m_screen)
{
    setFocusPolicy(Qt::StrongFocus);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    m_vbar = new QScrollBar(Qt::Vertical, this);
    m_vbar->setRange(0, 0);
    connect(m_vbar, &QScrollBar::valueChanged, this, [this](int v) {
        const int max = m_screen.scrollbackSize();
        m_scrollOffset = qBound(0, max - v, max);
        update();
    });
    m_screen.setMaxScrollback(AppSettings::instance().scrollbackLines());
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);

    m_font = AppSettings::instance().termFont();
    if (m_font.family().isEmpty() || m_font.family() == QLatin1String("monospace")) {
        const QStringList fonts = {
#ifdef Q_OS_WIN
            QStringLiteral("Cascadia Mono"),
            QStringLiteral("Cascadia Code"),
            QStringLiteral("Sarasa Mono SC"),
            QStringLiteral("Microsoft YaHei Mono"),
            QStringLiteral("Consolas"),
            QStringLiteral("Courier New"),
#else
            QStringLiteral("Sarasa Mono SC"),
            QStringLiteral("Noto Sans Mono CJK SC"),
            QStringLiteral("Source Han Mono SC"),
            QStringLiteral("WenQuanYi Micro Hei Mono"),
            QStringLiteral("Droid Sans Fallback"),
            QStringLiteral("DejaVu Sans Mono"),
            QStringLiteral("Noto Sans Mono"),
#endif
            QStringLiteral("monospace")
        };
        for (const QString &name : fonts) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            const bool has = QFontDatabase::hasFamily(name);
#else
            const bool has = QFontDatabase().hasFamily(name);
#endif
            if (has) {
                m_font = QFont(name, AppSettings::instance().termFontSize());
                break;
            }
        }
    }
    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);
    updateMetrics();

    m_cursorTimer = new QTimer(this);
    m_cursorTimer->setInterval(530);
    connect(m_cursorTimer, &QTimer::timeout, this, [this] {
        m_cursorOn = !m_cursorOn;
        update();
    });
    m_cursorTimer->start();

    connect(&m_pty, &PtyProcess::readyRead, this, &TerminalWidget::onPtyData);
    connect(&m_pty, &PtyProcess::finished, this, [this](int code) {
        emit sessionFinished(code);
    });
    m_parser.setWriteBack([this](const QByteArray &d) { m_pty.write(d); });
}

bool TerminalWidget::startSession(const Session &session,
                                 const QString &password,
                                 const QString &controlPath,
                                 bool resetScreen)
{
    m_session = session;
    m_destPassword = password;
    m_screen.setMaxScrollback(AppSettings::instance().scrollbackLines());
    if (resetScreen) {
        m_screen.reset(ColorScheme::byName(session.colorScheme));
        m_parser.reset();
        m_scrollOffset = 0;
        m_hasSelection = false;
        m_inAltScreen = false;
    } else {
        m_screen.applyScheme(ColorScheme::byName(session.colorScheme.isEmpty()
                                                     ? QStringLiteral("Moba Dark")
                                                     : session.colorScheme));
    }
    m_cwdHookSent = false;
    m_remoteCwd.clear();
    m_jumpQueue.clear();
    m_jumpIndex = 0;
    m_jumpScan.clear();
    m_jumpWaitPassword = false;
    m_jumpActive = false;
    m_autoPasswordSent = false;
    m_autoScan.clear();
    m_pendingStartup.clear();

    if (session.logEnabled) {
        m_logFile = SessionLogger::createLogPath(session);
        SessionLogger::writeHeader(m_logFile, session);
    }

    const Session entry = sessionEntryTarget(session);
    QString entryPassword = password;
    if (!session.jumps.isEmpty()) {
        entryPassword = Crypto::decrypt(session.jumps.first().passwordEnc);
        if (entryPassword.isEmpty())
            entryPassword = password;
        for (int i = 1; i < session.jumps.size(); ++i)
            m_jumpQueue.append(session.jumps.at(i));
        JumpHop dest;
        dest.host = session.host;
        dest.port = session.port > 0 ? session.port : 22;
        dest.username = session.username;
        if (!password.isEmpty())
            dest.passwordEnc = Crypto::encrypt(password);
        else
            dest.passwordEnc = session.passwordEnc;
        const JumpHop &last = session.jumps.last();
        const bool destIsLastHop = dest.host == last.host
            && dest.port == (last.port > 0 ? last.port : 22)
            && dest.username == last.username;
        if (!dest.host.trimmed().isEmpty() && !destIsLastHop)
            m_jumpQueue.append(dest);
        m_jumpActive = !m_jumpQueue.isEmpty();
        m_pendingStartup = session.startupCommand.trimmed();
    }

    QStringList args;
    QString program;
    QString workDir;
    if (!SessionLauncher::build(entry, entryPassword, &program, &args, &workDir, controlPath))
        return false;

    applySizeToPty();
    if (!m_pty.start(program, args, workDir))
        return false;

    if (!m_jumpActive && !session.startupCommand.trimmed().isEmpty()) {
        QTimer::singleShot(600, this, [this, session] {
            sendText(session.startupCommand, true);
        });
    }
    return true;
}

void TerminalWidget::sendText(const QString &text, bool addNewline)
{
    QByteArray data = text.toUtf8();
    if (addNewline)
        data.append('\n');
    sendBytes(data);
}

void TerminalWidget::sendBytes(const QByteArray &data)
{
    m_pty.write(data);
}

int TerminalWidget::viewToAbs(int viewRow) const
{
    return m_screen.scrollbackSize() - m_scrollOffset + viewRow;
}

int TerminalWidget::absToView(int absLine) const
{
    return absLine - m_screen.scrollbackSize() + m_scrollOffset;
}

QString TerminalWidget::selectedText() const
{
    if (!m_hasSelection)
        return {};
    int a1 = m_selAbs1, c1 = m_selCol1, a2 = m_selAbs2, c2 = m_selCol2;
    if (a1 > a2 || (a1 == a2 && c1 > c2)) {
        qSwap(a1, a2);
        qSwap(c1, c2);
    }
    const int v1 = absToView(a1);
    const int v2 = absToView(a2);
    return m_screen.textInRect(v1, c1, v2, c2, m_scrollOffset);
}

void TerminalWidget::copySelection()
{
    if (!m_hasSelection)
        return;
    QApplication::clipboard()->setText(selectedText());
}

void TerminalWidget::syncScrollBar()
{
    if (!m_vbar)
        return;
    const int max = m_screen.scrollbackSize();
    QSignalBlocker b(m_vbar);
    m_vbar->setRange(0, max);
    m_vbar->setPageStep(qMax(1, m_screen.rows()));
    m_vbar->setValue(max - m_scrollOffset);
}

void TerminalWidget::copyAll()
{
    QApplication::clipboard()->setText(m_screen.allText());
}

void TerminalWidget::pasteClipboard()
{
    sendText(QApplication::clipboard()->text());
}

void TerminalWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *copy = menu.addAction(QStringLiteral("复制"));
    copy->setEnabled(m_hasSelection);
    QAction *paste = menu.addAction(QStringLiteral("粘贴"));
    QAction *all = menu.addAction(QStringLiteral("复制全部"));
    menu.addSeparator();
    QAction *clear = menu.addAction(QStringLiteral("清屏"));
    QAction *a = menu.exec(event->globalPos());
    if (a == copy)
        copySelection();
    else if (a == paste)
        pasteClipboard();
    else if (a == all)
        copyAll();
    else if (a == clear)
        clearScreen();
}

void TerminalWidget::clearScreen()
{
    m_screen.ed(2);
    update();
}

void TerminalWidget::setColorScheme(const QString &name)
{
    m_screen.applyScheme(ColorScheme::byName(name));
    update();
}

void TerminalWidget::applyDisplaySettings()
{
    m_screen.setMaxScrollback(AppSettings::instance().scrollbackLines());
    syncScrollBar();
    update();
}

void TerminalWidget::setTerminalFont(const QFont &font)
{
    m_font = font;
    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);
    updateMetrics();
    applySizeToPty();
    update();
}

void TerminalWidget::zoom(int deltaPoints)
{
    const int sz = qBound(8, m_font.pointSize() + deltaPoints, 32);
    m_font.setPointSize(sz);
    AppSettings::instance().setTermFont(m_font.family(), sz);
    updateMetrics();
    applySizeToPty();
    update();
}

void TerminalWidget::disconnectSession()
{
    m_pty.terminate();
}

void TerminalWidget::writeLocal(const QString &text)
{
    if (text.isEmpty())
        return;
    m_skipAutoPassword = true;
    onPtyData(text.toUtf8());
    m_skipAutoPassword = false;
}

QString TerminalWidget::recentPlainText(int maxLines) const
{
    const QString all = m_screen.allText();
    const QStringList lines = all.split(QLatin1Char('\n'));
    if (lines.size() <= maxLines)
        return all;
    return lines.mid(lines.size() - maxLines).join(QLatin1Char('\n'));
}

void TerminalWidget::enableCwdReporting()
{
    refreshCwdFromScreen();
}

void TerminalWidget::refreshCwdFromScreen()
{
    auto lineText = [this](int r) -> QString {
        if (r < 0 || r >= m_screen.rows())
            return {};
        QString s;
        for (const Cell &cell : m_screen.displayLine(r)) {
            if (cell.flags & Cell::WideCont)
                continue;
            if (!cell.ch || cell.ch == U' ')
                s.append(QLatin1Char(' '));
            else
                s.append(ucs4ToString(cell.ch));
        }
        return s.trimmed();
    };

    QString cwd = m_screen.cwd();
    if (cwd.isEmpty())
        cwd = m_remoteCwd;

    QRegularExpression promptRe(
        QStringLiteral("[:\\s]((?:~|/|\\./)[^\\s\\$#]*)\\s*[\\$#]\\s*$"));
    for (int r = m_screen.rows() - 1; r >= 0 && r >= m_screen.rows() - 8; --r) {
        const QString line = lineText(r);
        if (line.isEmpty())
            continue;
        auto m = promptRe.match(line);
        if (m.hasMatch()) {
            cwd = m.captured(1);
            break;
        }
        if (line.startsWith(QLatin1Char('/')) || line.startsWith(QLatin1String("~/"))) {
            cwd = line.split(QRegularExpression(QStringLiteral("\\s+"))).first();
            break;
        }
    }
    if (!cwd.isEmpty() && cwd != m_remoteCwd) {
        m_remoteCwd = cwd;
        emit cwdChanged(m_remoteCwd);
    }
}

bool TerminalWidget::findText(const QString &needle, bool forward)
{
    const QString n = needle.trimmed();
    if (n.isEmpty())
        return false;

    auto lineText = [this](int abs) -> QString {
        const QVector<Cell> *line = nullptr;
        if (abs < m_screen.scrollbackSize())
            line = &m_screen.scrollbackLine(abs);
        else {
            const int r = abs - m_screen.scrollbackSize();
            if (r >= 0 && r < m_screen.rows())
                line = &m_screen.displayLine(r);
        }
        if (!line)
            return {};
        QString s;
        for (const Cell &cell : *line) {
            if (cell.flags & Cell::WideCont)
                continue;
            if (!cell.ch || cell.ch == U' ')
                s.append(QLatin1Char(' '));
            else
                s.append(ucs4ToString(cell.ch));
        }
        return s;
    };

    const int total = m_screen.scrollbackSize() + m_screen.rows();
    int abs = m_hasFind ? m_findAbs : (forward ? 0 : total - 1);
    int col = m_hasFind ? m_findCol : (forward ? 0 : 10000);
    for (int step = 0; step < total + 2; ++step) {
        const QString line = lineText(abs);
        int found = -1;
        if (forward) {
            found = line.indexOf(n, qMin(col, line.size()), Qt::CaseInsensitive);
        } else {
            const int from = qMin(col, line.size());
            found = line.lastIndexOf(n, from - 1, Qt::CaseInsensitive);
        }
        if (found >= 0) {
            m_hasFind = true;
            m_findAbs = abs;
            m_findCol = forward ? found + n.size() : found;
            const int displayAbs = abs - m_screen.scrollbackSize();
            if (displayAbs < 0)
                m_scrollOffset = -displayAbs;
            else
                m_scrollOffset = 0;
            const int viewRow = displayAbs + m_scrollOffset;
            m_selAbs1 = viewToAbs(viewRow);
            m_selCol1 = found;
            m_selAbs2 = m_selAbs1;
            m_selCol2 = found + n.size() - 1;
            m_hasSelection = true;
            update();
            return true;
        }
        if (forward) {
            ++abs;
            col = 0;
            if (abs >= total)
                abs = 0;
        } else {
            --abs;
            col = 10000;
            if (abs < 0)
                abs = total - 1;
        }
    }
    return false;
}

bool TerminalWidget::isRunning() const
{
    return m_pty.isRunning();
}

void TerminalWidget::onPtyData(const QByteArray &data)
{
    m_parser.feed(data);
    appendLog(data);
    tryAdvanceJump(data);
    tryAutoPassword(data);
    const int dropped = m_screen.consumeDroppedLines();
    if (dropped > 0 && m_hasSelection) {
        m_selAbs1 -= dropped;
        m_selAbs2 -= dropped;
        if (m_selAbs2 < 0)
            m_hasSelection = false;
        else
            m_selAbs1 = qMax(0, m_selAbs1);
    }
    const bool nowAlt = m_screen.altScreen();
    if (m_inAltScreen && !nowAlt)
        m_scrollOffset = 0;
    else if (m_scrollOffset != 0 && m_screen.dirty() && !nowAlt)
        m_scrollOffset = 0;
    m_inAltScreen = nowAlt;
    m_screen.clearDirty();
    if (!m_screen.title().isEmpty())
        emit titleChanged(m_screen.title());
    if (!m_activityQuiet) {
        m_activityQuiet = true;
        emit activity();
        QTimer::singleShot(180, this, [this] { m_activityQuiet = false; });
    }
    if (isVisible()) {
        syncScrollBar();
        update();
    }
    QString cwd = m_screen.cwd();
    if (cwd.isEmpty() && !m_screen.title().isEmpty()) {
        const QString title = m_screen.title();
        const int colon = title.lastIndexOf(QLatin1Char(':'));
        if (colon >= 0) {
            const QString path = title.mid(colon + 1).trimmed();
            if (path.startsWith(QLatin1Char('/')) || path.startsWith(QLatin1Char('~')))
                cwd = path;
        }
    }
    if (!cwd.isEmpty() && cwd != m_remoteCwd) {
        m_remoteCwd = cwd;
        emit cwdChanged(m_remoteCwd);
    }
}

namespace {

QString stripAnsi(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('\x1b')) {
            if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('[')) {
                i += 2;
                while (i < text.size()) {
                    const QChar ch = text.at(i);
                    if (ch.isLetter() || ch == QLatin1Char('@'))
                        break;
                    ++i;
                }
                continue;
            }
            continue;
        }
        if (c == QLatin1Char('\r'))
            continue;
        if (c == QLatin1Char('\b')) {
            if (!out.isEmpty())
                out.chop(1);
            continue;
        }
        out.append(c);
    }
    return out;
}

} // namespace

void TerminalWidget::tryAutoPassword(const QByteArray &data)
{
    if (m_skipAutoPassword || m_jumpActive || m_autoPasswordSent || m_destPassword.isEmpty())
        return;
    if (m_session.protocol != Protocol::Ssh && m_session.protocol != Protocol::Sftp)
        return;
    m_autoScan += QString::fromLocal8Bit(data);
    if (m_autoScan.size() > 8000)
        m_autoScan = m_autoScan.right(4000);
    const QString line = lastPromptLine(stripAnsi(m_autoScan));
    const QString low = line.toLower();
    if (low.contains(QLatin1String("yes/no"))
        || low.contains(QLatin1String("are you sure you want to continue connecting"))) {
        sendText(QStringLiteral("yes"), true);
        m_autoScan.clear();
        return;
    }
    if (low.contains(QLatin1String("password:")) || line.contains(QStringLiteral("密码"))) {
        sendText(m_destPassword, true);
        m_autoPasswordSent = true;
        m_autoScan.clear();
    }
}

void TerminalWidget::tryAdvanceJump(const QByteArray &data)
{
    if (!m_jumpActive)
        return;
    m_jumpScan += QString::fromLocal8Bit(data);
    if (m_jumpScan.size() > 12000)
        m_jumpScan = m_jumpScan.right(6000);
    const QString plain = stripAnsi(m_jumpScan);
    const QString line = lastPromptLine(plain);
    const QString low = line.toLower();

    if (low.contains(QLatin1String("yes/no"))
        || low.contains(QLatin1String("yes/no/[fingerprint]"))
        || low.contains(QLatin1String("are you sure you want to continue connecting"))) {
        sendText(QStringLiteral("yes"), true);
        m_jumpScan.clear();
        return;
    }

    const bool pwPrompt = low.contains(QLatin1String("password:"))
        || line.contains(QStringLiteral("密码"));

    if (m_jumpWaitPassword) {
        if (pwPrompt) {
            if (m_jumpIndex < 0 || m_jumpIndex >= m_jumpQueue.size()) {
                finishJumps();
                return;
            }
            sendText(Crypto::decrypt(m_jumpQueue.at(m_jumpIndex).passwordEnc), true);
            m_jumpWaitPassword = false;
            m_jumpScan.clear();
            ++m_jumpIndex;
            if (m_jumpIndex >= m_jumpQueue.size())
                finishJumps();
            return;
        }
        if (looksLikeShellPrompt(line)) {
            m_jumpWaitPassword = false;
            m_jumpScan.clear();
            ++m_jumpIndex;
            if (m_jumpIndex >= m_jumpQueue.size()) {
                finishJumps();
                return;
            }
            sendNextJump();
        }
        return;
    }

    if (looksLikeShellPrompt(line))
        sendNextJump();
}

void TerminalWidget::sendNextJump()
{
    if (!m_jumpActive || m_jumpWaitPassword)
        return;
    if (m_jumpIndex < 0 || m_jumpIndex >= m_jumpQueue.size()) {
        finishJumps();
        return;
    }
    const JumpHop &h = m_jumpQueue.at(m_jumpIndex);
    QString cmd = QStringLiteral("ssh -t -o StrictHostKeyChecking=accept-new");
    const int port = h.port > 0 ? h.port : 22;
    if (port != 22)
        cmd += QStringLiteral(" -p %1").arg(port);
    cmd += QLatin1Char(' ');
    if (!h.username.trimmed().isEmpty())
        cmd += h.username.trimmed() + QLatin1Char('@');
    cmd += h.host.trimmed();
    sendText(cmd, true);
    m_jumpWaitPassword = true;
    m_jumpScan.clear();
}

void TerminalWidget::finishJumps()
{
    m_jumpActive = false;
    m_jumpWaitPassword = false;
    m_jumpScan.clear();
    if (!m_pendingStartup.isEmpty()) {
        const QString cmd = m_pendingStartup;
        m_pendingStartup.clear();
        QTimer::singleShot(250, this, [this, cmd] { sendText(cmd, true); });
    }
}

QString TerminalWidget::lastPromptLine(const QString &plain) const
{
    QString t = plain;
    while (t.endsWith(QLatin1Char('\n')))
        t.chop(1);
    const int nl = t.lastIndexOf(QLatin1Char('\n'));
    if (nl >= 0)
        t = t.mid(nl + 1);
    return t.trimmed();
}

bool TerminalWidget::looksLikeShellPrompt(const QString &line) const
{
    if (line.isEmpty())
        return false;
    const QString low = line.toLower();
    if (low.contains(QLatin1String("password:")) || line.contains(QStringLiteral("密码")))
        return false;
    if (low.contains(QLatin1String("yes/no")))
        return false;
    const QChar last = line.at(line.size() - 1);
    if (last == QLatin1Char('#') || last == QLatin1Char('$') || last == QLatin1Char('%')
        || last == QLatin1Char('>'))
        return true;
    static const QRegularExpression re(QStringLiteral("[$#%]\\s*$"));
    return re.match(line).hasMatch();
}

void TerminalWidget::appendLog(const QByteArray &data)
{
    if (m_logFile.isEmpty())
        return;
    m_logBuffer.append(data);
    if (m_logBuffer.size() > 4096) {
        SessionLogger::append(m_logFile, m_logBuffer);
        m_logBuffer.clear();
    }
}

void TerminalWidget::updateMetrics()
{
    QFontMetrics fm(m_font);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    m_cellW = fm.horizontalAdvance(QLatin1Char('W'));
#else
    m_cellW = fm.width(QLatin1Char('W'));
#endif
    m_cellH = fm.height();
    m_ascent = fm.ascent();
    if (m_cellW < 6)
        m_cellW = 6;
    if (m_cellH < 12)
        m_cellH = 12;
}

void TerminalWidget::applySizeToPty()
{
    const int sbw = m_vbar ? m_vbar->sizeHint().width() : 0;
    int cols = m_cellW > 0 ? (width() - sbw) / m_cellW : 80;
    int rows = m_cellH > 0 ? height() / m_cellH : 24;
    if (width() < 80 || height() < 40 || cols < 20 || rows < 5) {
        cols = qMax(cols, 80);
        rows = qMax(rows, 24);
    }
    cols = qMax(20, cols);
    rows = qMax(5, rows);
    if (cols != m_screen.cols() || rows != m_screen.rows())
        m_screen.resize(cols, rows);
    if (!m_pty.isRunning()) {
        m_pty.setWinsize(cols, rows);
        return;
    }
    if (!m_resizePtyTimer) {
        m_resizePtyTimer = new QTimer(this);
        m_resizePtyTimer->setSingleShot(true);
        m_resizePtyTimer->setInterval(80);
        connect(m_resizePtyTimer, &QTimer::timeout, this, [this] {
            m_pty.setWinsize(m_screen.cols(), m_screen.rows());
        });
    }
    m_resizePtyTimer->start();
}

QRect TerminalWidget::cellRect(int row, int col) const
{
    return QRect(col * m_cellW, row * m_cellH, m_cellW, m_cellH);
}

QPoint TerminalWidget::cellAt(const QPoint &pos) const
{
    return QPoint(qBound(0, pos.x() / m_cellW, m_screen.cols() - 1),
                  qBound(0, pos.y() / m_cellH, m_screen.rows() - 1));
}

QRect TerminalWidget::cursorRect() const
{
    return cellRect(m_screen.cursorRow(), m_screen.cursorCol());
}

Cell TerminalWidget::visibleCell(int viewRow, int col) const
{
    const int abs = viewRow - m_scrollOffset;
    if (abs < 0) {
        const int sbi = m_screen.scrollbackSize() + abs;
        if (sbi >= 0 && sbi < m_screen.scrollbackSize()) {
            const auto &line = m_screen.scrollbackLine(sbi);
            if (col >= 0 && col < line.size())
                return line[col];
        }
        return {};
    }
    return m_screen.cell(abs, col);
}

void TerminalWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setFont(m_font);
    const ColorScheme scheme = m_screen.scheme();
    auto rgb24 = [](quint32 c) {
        return QColor(int((c >> 16) & 255), int((c >> 8) & 255), int(c & 255));
    };
    p.fillRect(rect(), rgb24(scheme.background));

    auto inSel = [this](int r, int c) {
        if (!m_hasSelection)
            return false;
        int a1 = m_selAbs1, c1 = m_selCol1, a2 = m_selAbs2, c2 = m_selCol2;
        if (a1 > a2 || (a1 == a2 && c1 > c2)) {
            qSwap(a1, a2);
            qSwap(c1, c2);
        }
        const int abs = viewToAbs(r);
        if (abs < a1 || abs > a2)
            return false;
        if (abs == a1 && abs == a2)
            return c >= c1 && c <= c2;
        if (abs == a1)
            return c >= c1;
        if (abs == a2)
            return c <= c2;
        return true;
    };

    const bool doHi = AppSettings::instance().logHighlight();
    auto lineHighlight = [&](int viewRow, int col, quint32 curFg) -> quint32 {
        if (!doHi)
            return curFg;
        if (curFg != scheme.foreground)
            return curFg;
        QString line;
        for (int i = 0; i < m_screen.cols(); ++i) {
            const Cell cell = visibleCell(viewRow, i);
            if (cell.flags & Cell::WideCont)
                continue;
            if (!cell.ch || cell.ch == U' ')
                line.append(QLatin1Char(' '));
            else
                line.append(ucs4ToString(cell.ch));
        }
        const QString low = line.toLower();
        struct Hit { QString key; quint32 color; };
        static const Hit keys[] = {
            {QStringLiteral("fatal"), 0xF85149},
            {QStringLiteral("error"), 0xF85149},
            {QStringLiteral("fail"), 0xF85149},
            {QStringLiteral("denied"), 0xF85149},
            {QStringLiteral("refused"), 0xF85149},
            {QStringLiteral("exception"), 0xF85149},
            {QStringLiteral("warn"), 0xE3B341},
            {QStringLiteral("timeout"), 0xE3B341},
            {QStringLiteral("success"), 0x3FB950},
            {QStringLiteral("info"), 0x58A6FF},
            {QStringLiteral("debug"), 0x8B9CB3},
        };
        for (const Hit &h : keys) {
            int at = 0;
            while ((at = low.indexOf(h.key, at)) >= 0) {
                if (col >= at && col < at + h.key.size())
                    return h.color;
                at += h.key.size();
            }
        }
        return curFg;
    };

    for (int r = 0; r < m_screen.rows(); ++r) {
        int c = 0;
        while (c < m_screen.cols()) {
            const Cell cell = visibleCell(r, c);
            if (cell.flags & Cell::WideCont) {
                ++c;
                continue;
            }
            quint32 fg = cell.fg;
            quint32 bg = cell.bg;
            if (cell.flags & Cell::Inverse)
                qSwap(fg, bg);
            if ((cell.flags & Cell::Bold) && scheme.ansi.size() >= 16) {
                for (int i = 0; i < 8; ++i) {
                    if (fg == scheme.ansi.at(i)) {
                        fg = scheme.ansi.at(i + 8);
                        break;
                    }
                }
            }
            if (inSel(r, c))
                bg = scheme.selection;
            else
                fg = lineHighlight(r, c, fg);
            const int span = (cell.flags & Cell::Wide) ? 2 : 1;
            const QRect rc(c * m_cellW, r * m_cellH, m_cellW * span, m_cellH);
            p.fillRect(rc, rgb24(bg));
            if (cell.ch && cell.ch != U' ') {
                QFont f = m_font;
                f.setBold(cell.flags & Cell::Bold);
                f.setItalic(cell.flags & Cell::Italic);
                f.setUnderline(cell.flags & Cell::Underline);
                p.setFont(f);
                p.setPen(rgb24(fg));
                const QString s = ucs4ToString(cell.ch);
                p.drawText(rc.x(), rc.y() + m_ascent, s);
            }
            c += span;
        }
    }

    if (hasFocus() && m_cursorOn && m_screen.cursorVisible() && m_scrollOffset == 0) {
        QRect rc = cursorRect();
        rc.setWidth(qMax(2, m_cellW / 5));
        p.fillRect(rc, rgb24(scheme.cursor));
    }
}

void TerminalWidget::resizeEvent(QResizeEvent *)
{
    updateMetrics();
    if (m_vbar) {
        const int sbw = m_vbar->sizeHint().width();
        m_vbar->setGeometry(width() - sbw, 0, sbw, height());
    }
    applySizeToPty();
    syncScrollBar();
}

bool TerminalWidget::event(QEvent *event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
        auto *ke = static_cast<QKeyEvent *>(event);
        const int k = ke->key();
        if (k == Qt::Key_Tab || k == Qt::Key_Backtab) {
            if (ke->modifiers() & Qt::ControlModifier)
                return QWidget::event(event);
            if (event->type() == QEvent::ShortcutOverride) {
                event->accept();
                return true;
            }
            keyPressEvent(ke);
            return true;
        }
    }
    return QWidget::event(event);
}

bool TerminalWidget::focusNextPrevChild(bool)
{
    return false;
}

void TerminalWidget::keyPressEvent(QKeyEvent *event)
{
    m_scrollOffset = 0;
    const auto mods = event->modifiers();
    const int k = event->key();
    const bool ctrl = mods & Qt::ControlModifier;
    const bool shift = mods & Qt::ShiftModifier;
    const bool alt = mods & Qt::AltModifier;

    if (ctrl && shift && k == Qt::Key_C) {
        copySelection();
        return;
    }
    if ((ctrl && shift && k == Qt::Key_V) || (shift && k == Qt::Key_Insert)) {
        pasteClipboard();
        return;
    }
    if (ctrl && k == Qt::Key_C) {
        if (m_hasSelection)
            copySelection();
        else
            sendBytes(QByteArray(1, '\x03'));
        return;
    }
    if (ctrl && (k == Qt::Key_Plus || k == Qt::Key_Equal)) {
        zoom(1);
        return;
    }
    if (ctrl && k == Qt::Key_Minus) {
        zoom(-1);
        return;
    }
    if (ctrl && k == Qt::Key_0) {
        m_font.setPointSize(11);
        AppSettings::instance().setTermFont(m_font.family(), 11);
        updateMetrics();
        applySizeToPty();
        update();
        return;
    }

    if (k == Qt::Key_Tab || k == Qt::Key_Backtab) {
        if (shift || k == Qt::Key_Backtab)
            sendBytes(QByteArray("\x1b[Z"));
        else
            sendBytes(QByteArray("\t"));
        return;
    }

    if (ctrl && k >= Qt::Key_A && k <= Qt::Key_Z) {
        const char c = static_cast<char>(k - Qt::Key_A + 1);
        sendBytes(QByteArray(1, c));
        return;
    }

    const bool app = m_screen.applicationCursorKeys();
    auto csi = [app, ctrl, shift, alt](char letter, const char *normal, const char *application) -> QByteArray {
        if (ctrl || shift || alt) {
            int m = 1;
            if (shift) m += 1;
            if (alt) m += 2;
            if (ctrl) m += 4;
            return QByteArray("\x1b[1;") + QByteArray::number(m) + letter;
        }
        return QByteArray(app ? application : normal);
    };

    QByteArray seq;
    switch (k) {
    case Qt::Key_Return:
    case Qt::Key_Enter: seq = "\r"; break;
    case Qt::Key_Backspace: seq = alt ? QByteArray("\x1b\x7f") : QByteArray("\x7f"); break;
    case Qt::Key_Escape: seq = "\x1b"; break;
    case Qt::Key_Up: seq = csi('A', "\x1b[A", "\x1bOA"); break;
    case Qt::Key_Down: seq = csi('B', "\x1b[B", "\x1bOB"); break;
    case Qt::Key_Right: seq = csi('C', "\x1b[C", "\x1bOC"); break;
    case Qt::Key_Left: seq = csi('D', "\x1b[D", "\x1bOD"); break;
    case Qt::Key_Home: seq = app ? QByteArray("\x1bOH") : QByteArray("\x1b[H"); break;
    case Qt::Key_End: seq = app ? QByteArray("\x1bOF") : QByteArray("\x1b[F"); break;
    case Qt::Key_Insert: seq = "\x1b[2~"; break;
    case Qt::Key_Delete: seq = "\x1b[3~"; break;
    case Qt::Key_PageUp: seq = "\x1b[5~"; break;
    case Qt::Key_PageDown: seq = "\x1b[6~"; break;
    case Qt::Key_F1: seq = "\x1bOP"; break;
    case Qt::Key_F2: seq = "\x1bOQ"; break;
    case Qt::Key_F3: seq = "\x1bOR"; break;
    case Qt::Key_F4: seq = "\x1bOS"; break;
    case Qt::Key_F5: seq = "\x1b[15~"; break;
    case Qt::Key_F6: seq = "\x1b[17~"; break;
    case Qt::Key_F7: seq = "\x1b[18~"; break;
    case Qt::Key_F8: seq = "\x1b[19~"; break;
    case Qt::Key_F9: seq = "\x1b[20~"; break;
    case Qt::Key_F10: seq = "\x1b[21~"; break;
    case Qt::Key_F11: seq = "\x1b[23~"; break;
    case Qt::Key_F12: seq = "\x1b[24~"; break;
    default:
        seq = event->text().toUtf8();
        break;
    }
    if (!seq.isEmpty())
        sendBytes(seq);
}

void TerminalWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_selecting = true;
        const QPoint cell = cellAt(event->pos());
        m_selAbs1 = m_selAbs2 = viewToAbs(cell.y());
        m_selCol1 = m_selCol2 = cell.x();
        m_hasSelection = false;
        update();
    } else if (event->button() == Qt::MiddleButton) {
        pasteClipboard();
    }
}

void TerminalWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        selectWordAt(cellAt(event->pos()));
}

void TerminalWidget::selectWordAt(const QPoint &cell)
{
    auto isWord = [](const Cell &c) {
        if (!c.ch || c.ch == U' ')
            return false;
        const QString s = ucs4ToString(c.ch);
        if (s.isEmpty())
            return false;
        const QChar ch = s.at(0);
        return ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('-')
            || ch == QLatin1Char('.') || ch == QLatin1Char('/') || ch == QLatin1Char(':')
            || ch == QLatin1Char('@');
    };
    int left = cell.x();
    int right = cell.x();
    while (left > 0 && isWord(visibleCell(cell.y(), left - 1)))
        --left;
    while (right + 1 < m_screen.cols() && isWord(visibleCell(cell.y(), right + 1)))
        ++right;
    if (!isWord(visibleCell(cell.y(), cell.x())))
        return;
    m_selAbs1 = m_selAbs2 = viewToAbs(cell.y());
    m_selCol1 = left;
    m_selCol2 = right;
    m_hasSelection = true;
    copySelection();
    update();
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_selecting)
        return;
    const QPoint cell = cellAt(event->pos());
    m_selAbs2 = viewToAbs(cell.y());
    m_selCol2 = cell.x();
    m_hasSelection = (m_selAbs1 != m_selAbs2 || m_selCol1 != m_selCol2);
    update();
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_selecting = false;
        if (m_hasSelection)
            copySelection();
    }
}

void TerminalWidget::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y() / 40;
    m_scrollOffset = qBound(0, m_scrollOffset + delta, m_screen.scrollbackSize());
    syncScrollBar();
    update();
}

void TerminalWidget::focusInEvent(QFocusEvent *)
{
    m_cursorOn = true;
    update();
}

void TerminalWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    syncScrollBar();
    if (m_cursorTimer && !m_cursorTimer->isActive())
        m_cursorTimer->start();
}

void TerminalWidget::hideEvent(QHideEvent *event)
{
    if (m_cursorTimer)
        m_cursorTimer->stop();
    QWidget::hideEvent(event);
}

void TerminalWidget::inputMethodEvent(QInputMethodEvent *event)
{
    if (!event->commitString().isEmpty())
        sendText(event->commitString());
    event->accept();
}

QVariant TerminalWidget::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (query == Qt::ImCursorRectangle)
        return cursorRect();
    if (query == Qt::ImEnabled)
        return true;
    return QWidget::inputMethodQuery(query);
}
