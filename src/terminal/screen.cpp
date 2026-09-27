#include "terminal/screen.h"
#include "util/qtcompat.h"

#include <QStringList>

#ifdef Q_OS_UNIX
#include <wchar.h>
#endif

Screen::Screen(int cols, int rows)
{
    m_scheme = ColorScheme::byName(QStringLiteral("Moba Dark"));
    reset(m_scheme);
    resize(cols, rows);
}

void Screen::reset(const ColorScheme &scheme)
{
    m_scheme = scheme;
    m_pen = Cell{};
    m_pen.fg = scheme.foreground;
    m_pen.bg = scheme.background;
    m_savedPen = m_pen;
    m_r = m_c = 0;
    m_scrollTop = 0;
    m_scrollBottom = qMax(0, m_rows - 1);
    m_originMode = false;
    m_autoWrap = true;
    m_insertMode = false;
    m_cursorVisible = true;
    m_appCursor = false;
    m_wrapPending = false;
    m_title.clear();
    m_cwd.clear();
    m_altScreen = false;
    m_normalDisplay.clear();
    m_display.resize(m_rows);
    for (int i = 0; i < m_rows; ++i)
        m_display[i] = blankLine();
    m_dirty = true;
}

void Screen::applyScheme(const ColorScheme &scheme)
{
    m_scheme = scheme;
    m_pen.fg = scheme.foreground;
    m_pen.bg = scheme.background;
}

void Screen::resize(int cols, int rows)
{
    cols = qMax(2, cols);
    rows = qMax(1, rows);
    QVector<QVector<Cell>> next(rows);
    const int copyRows = qMin(rows, m_rows);
    const int copyCols = qMin(cols, m_cols);
    for (int r = 0; r < rows; ++r) {
        next[r] = QVector<Cell>(cols, styledBlank());
        if (r < copyRows) {
            for (int c = 0; c < copyCols; ++c)
                next[r][c] = m_display[r][c];
        }
    }
    m_display.swap(next);
    m_cols = cols;
    m_rows = rows;
    m_scrollTop = 0;
    m_scrollBottom = rows - 1;
    m_r = qBound(0, m_r, rows - 1);
    m_c = qBound(0, m_c, cols - 1);
    m_dirty = true;
}

Cell Screen::styledBlank() const
{
    Cell c;
    c.fg = m_pen.fg;
    c.bg = m_pen.bg;
    return c;
}

QVector<Cell> Screen::blankLine() const
{
    return QVector<Cell>(m_cols, styledBlank());
}

void Screen::ensureCursor()
{
    m_r = qBound(0, m_r, m_rows - 1);
    m_c = qBound(0, m_c, m_cols - 1);
}

int Screen::charWidth(char32_t ch) const
{
    if (ch == 0)
        return 0;
#ifdef Q_OS_UNIX
    const int w = wcwidth(static_cast<wchar_t>(ch));
    if (w < 0)
        return 1;
    return w;
#else
    return (ch > 0x7F) ? 2 : 1;
#endif
}

void Screen::wrapIfNeeded()
{
    if (!m_wrapPending)
        return;
    m_wrapPending = false;
    if (!m_autoWrap)
        return;
    m_c = 0;
    index();
}

void Screen::putCodepoint(char32_t ch)
{
    if (ch == 0)
        return;
    wrapIfNeeded();
    const int w = qMax(1, charWidth(ch));
    if (m_insertMode) {
        auto &line = m_display[m_r];
        line.insert(m_c, w, styledBlank());
        while (line.size() > m_cols)
            line.removeLast();
    }
    if (m_c + w > m_cols) {
        if (m_autoWrap) {
            m_c = 0;
            index();
        } else {
            m_c = m_cols - w;
            if (m_c < 0)
                m_c = 0;
        }
    }
    Cell cell = m_pen;
    cell.ch = ch;
    if (w == 2)
        cell.flags |= Cell::Wide;
    m_display[m_r][m_c] = cell;
    if (w == 2 && m_c + 1 < m_cols) {
        Cell cont = m_pen;
        cont.ch = 0;
        cont.flags |= Cell::WideCont;
        m_display[m_r][m_c + 1] = cont;
    }
    m_c += w;
    if (m_c >= m_cols) {
        m_c = m_cols - 1;
        m_wrapPending = m_autoWrap;
    }
    m_dirty = true;
}

void Screen::bel() {}

void Screen::backspace()
{
    m_wrapPending = false;
    if (m_c > 0)
        --m_c;
    m_dirty = true;
}

void Screen::tab()
{
    m_wrapPending = false;
    m_c = qMin(m_cols - 1, ((m_c / 8) + 1) * 8);
    m_dirty = true;
}

void Screen::lineFeed()
{
    m_wrapPending = false;
    index();
}

void Screen::carriageReturn()
{
    m_wrapPending = false;
    m_c = 0;
    m_dirty = true;
}

void Screen::index()
{
    if (m_r == m_scrollBottom)
        scrollUp(1);
    else if (m_r < m_rows - 1)
        ++m_r;
    m_dirty = true;
}

void Screen::reverseIndex()
{
    if (m_r == m_scrollTop)
        scrollDown(1);
    else if (m_r > 0)
        --m_r;
    m_dirty = true;
}

void Screen::setMaxScrollback(int lines)
{
    m_maxScrollback = qBound(200, lines, 50000);
    while (static_cast<int>(m_scrollback.size()) > m_maxScrollback) {
        m_scrollback.pop_front();
        ++m_droppedLines;
    }
}

int Screen::consumeDroppedLines()
{
    const int n = m_droppedLines;
    m_droppedLines = 0;
    return n;
}

void Screen::pushScrollback(const QVector<Cell> &line)
{
    if (m_altScreen)
        return;
    m_scrollback.push_back(line);
    while (static_cast<int>(m_scrollback.size()) > m_maxScrollback) {
        m_scrollback.pop_front();
        ++m_droppedLines;
    }
}

void Screen::scrollUp(int n)
{
    n = qBound(1, n, m_scrollBottom - m_scrollTop + 1);
    for (int i = 0; i < n; ++i) {
        if (m_scrollTop == 0)
            pushScrollback(m_display[m_scrollTop]);
        m_display.removeAt(m_scrollTop);
        m_display.insert(m_scrollBottom, blankLine());
    }
    m_dirty = true;
}

void Screen::scrollDown(int n)
{
    n = qBound(1, n, m_scrollBottom - m_scrollTop + 1);
    for (int i = 0; i < n; ++i) {
        m_display.removeAt(m_scrollBottom);
        m_display.insert(m_scrollTop, blankLine());
    }
    m_dirty = true;
}

void Screen::cup(int row, int col)
{
    m_wrapPending = false;
    int r = qMax(1, row) - 1;
    int c = qMax(1, col) - 1;
    if (m_originMode)
        r += m_scrollTop;
    m_r = qBound(0, r, m_rows - 1);
    m_c = qBound(0, c, m_cols - 1);
    m_dirty = true;
}

void Screen::cuu(int n) { m_wrapPending = false; m_r = qMax(m_originMode ? m_scrollTop : 0, m_r - qMax(1, n)); m_dirty = true; }
void Screen::cud(int n) { m_wrapPending = false; m_r = qMin(m_originMode ? m_scrollBottom : m_rows - 1, m_r + qMax(1, n)); m_dirty = true; }
void Screen::cuf(int n) { m_wrapPending = false; m_c = qMin(m_cols - 1, m_c + qMax(1, n)); m_dirty = true; }
void Screen::cub(int n) { m_wrapPending = false; m_c = qMax(0, m_c - qMax(1, n)); m_dirty = true; }
void Screen::cha(int col) { m_wrapPending = false; m_c = qBound(0, qMax(1, col) - 1, m_cols - 1); m_dirty = true; }
void Screen::vpa(int row)
{
    m_wrapPending = false;
    int r = qMax(1, row) - 1;
    if (m_originMode)
        r += m_scrollTop;
    m_r = qBound(0, r, m_rows - 1);
    m_dirty = true;
}

void Screen::ed(int mode)
{
    if (mode == 2 || mode == 3) {
        for (int r = 0; r < m_rows; ++r)
            m_display[r] = blankLine();
        if (mode == 3)
            m_scrollback.clear();
    } else if (mode == 1) {
        for (int r = 0; r < m_r; ++r)
            m_display[r] = blankLine();
        for (int c = 0; c <= m_c && c < m_cols; ++c)
            m_display[m_r][c] = styledBlank();
    } else {
        for (int c = m_c; c < m_cols; ++c)
            m_display[m_r][c] = styledBlank();
        for (int r = m_r + 1; r < m_rows; ++r)
            m_display[r] = blankLine();
    }
    m_dirty = true;
}

void Screen::el(int mode)
{
    if (mode == 2) {
        m_display[m_r] = blankLine();
    } else if (mode == 1) {
        for (int c = 0; c <= m_c && c < m_cols; ++c)
            m_display[m_r][c] = styledBlank();
    } else {
        for (int c = m_c; c < m_cols; ++c)
            m_display[m_r][c] = styledBlank();
    }
    m_dirty = true;
}

void Screen::il(int n)
{
    n = qMax(1, n);
    for (int i = 0; i < n; ++i) {
        m_display.removeAt(m_scrollBottom);
        m_display.insert(m_r, blankLine());
    }
    m_dirty = true;
}

void Screen::dl(int n)
{
    n = qMax(1, n);
    for (int i = 0; i < n; ++i) {
        m_display.removeAt(m_r);
        m_display.insert(m_scrollBottom, blankLine());
    }
    m_dirty = true;
}

void Screen::ich(int n)
{
    n = qMax(1, n);
    auto &line = m_display[m_r];
    line.insert(m_c, n, styledBlank());
    while (line.size() > m_cols)
        line.removeLast();
    m_dirty = true;
}

void Screen::dch(int n)
{
    n = qMax(1, n);
    auto &line = m_display[m_r];
    for (int i = 0; i < n && m_c < line.size(); ++i)
        line.removeAt(m_c);
    while (line.size() < m_cols)
        line.push_back(styledBlank());
    m_dirty = true;
}

void Screen::ech(int n)
{
    n = qMax(1, n);
    for (int i = 0; i < n && m_c + i < m_cols; ++i)
        m_display[m_r][m_c + i] = styledBlank();
    m_dirty = true;
}

void Screen::su(int n) { scrollUp(qMax(1, n)); }
void Screen::sd(int n) { scrollDown(qMax(1, n)); }

void Screen::setScrollRegion(int top, int bottom)
{
    int t = qMax(1, top) - 1;
    int b = (bottom <= 0 ? m_rows : bottom) - 1;
    if (t >= b)
        return;
    m_scrollTop = qBound(0, t, m_rows - 1);
    m_scrollBottom = qBound(0, b, m_rows - 1);
    m_r = m_originMode ? m_scrollTop : 0;
    m_c = 0;
}

void Screen::sgr(const QVector<int> &params)
{
    auto apply = [this](int p) {
        switch (p) {
        case 0:
            m_pen = Cell{};
            m_pen.fg = m_scheme.foreground;
            m_pen.bg = m_scheme.background;
            break;
        case 1: m_pen.flags |= Cell::Bold; break;
        case 3: m_pen.flags |= Cell::Italic; break;
        case 4: m_pen.flags |= Cell::Underline; break;
        case 5: m_pen.flags |= Cell::Blink; break;
        case 7: m_pen.flags |= Cell::Inverse; break;
        case 22: m_pen.flags &= ~static_cast<quint8>(Cell::Bold); break;
        case 23: m_pen.flags &= ~static_cast<quint8>(Cell::Italic); break;
        case 24: m_pen.flags &= ~static_cast<quint8>(Cell::Underline); break;
        case 25: m_pen.flags &= ~static_cast<quint8>(Cell::Blink); break;
        case 27: m_pen.flags &= ~static_cast<quint8>(Cell::Inverse); break;
        case 39: m_pen.fg = m_scheme.foreground; break;
        case 49: m_pen.bg = m_scheme.background; break;
        default:
            if (p >= 30 && p <= 37)
                m_pen.fg = m_scheme.ansi.value(p - 30, m_pen.fg);
            else if (p >= 90 && p <= 97)
                m_pen.fg = m_scheme.ansi.value(p - 90 + 8, m_pen.fg);
            else if (p >= 40 && p <= 47)
                m_pen.bg = m_scheme.ansi.value(p - 40, m_pen.bg);
            else if (p >= 100 && p <= 107)
                m_pen.bg = m_scheme.ansi.value(p - 100 + 8, m_pen.bg);
            break;
        }
    };

    if (params.isEmpty()) {
        apply(0);
        return;
    }
    for (int i = 0; i < params.size(); ++i) {
        const int p = params[i];
        if (p == 38 || p == 48) {
            if (i + 1 < params.size() && params[i + 1] == 5 && i + 2 < params.size()) {
                const quint32 col = ansi256(params[i + 2]);
                if (p == 38) m_pen.fg = col; else m_pen.bg = col;
                i += 2;
            } else if (i + 1 < params.size() && params[i + 1] == 2 && i + 4 < params.size()) {
                const quint32 col = (quint32(params[i + 2] & 255) << 16)
                    | (quint32(params[i + 3] & 255) << 8)
                    | quint32(params[i + 4] & 255);
                if (p == 38) m_pen.fg = col; else m_pen.bg = col;
                i += 4;
            }
        } else {
            apply(p);
        }
    }
}

void Screen::saveCursor()
{
    m_savedR = m_r;
    m_savedC = m_c;
    m_savedPen = m_pen;
}

void Screen::restoreCursor()
{
    m_r = m_savedR;
    m_c = m_savedC;
    m_pen = m_savedPen;
    ensureCursor();
    m_wrapPending = false;
    m_dirty = true;
}

void Screen::enterAltScreen(bool saveCursorPos)
{
    if (m_altScreen)
        return;
    if (saveCursorPos)
        saveCursor();
    m_normalDisplay = m_display;
    m_normalR = m_r;
    m_normalC = m_c;
    m_normalScrollTop = m_scrollTop;
    m_normalScrollBottom = m_scrollBottom;
    m_altScrollbackMark = static_cast<int>(m_scrollback.size());
    m_altScreen = true;
    for (int i = 0; i < m_rows; ++i)
        m_display[i] = blankLine();
    m_scrollTop = 0;
    m_scrollBottom = qMax(0, m_rows - 1);
    m_r = 0;
    m_c = 0;
    m_wrapPending = false;
    m_dirty = true;
}

void Screen::leaveAltScreen(bool restoreCursorPos)
{
    if (!m_altScreen)
        return;
    while (static_cast<int>(m_scrollback.size()) > m_altScrollbackMark)
        m_scrollback.pop_back();
    if (!m_normalDisplay.isEmpty()) {
        m_display = m_normalDisplay;
        if (m_display.size() != m_rows || (!m_display.isEmpty() && m_display.first().size() != m_cols))
            resize(m_cols, m_rows);
    }
    m_r = qBound(0, m_normalR, m_rows - 1);
    m_c = qBound(0, m_normalC, m_cols - 1);
    m_scrollTop = qBound(0, m_normalScrollTop, qMax(0, m_rows - 1));
    m_scrollBottom = qBound(0, m_normalScrollBottom, qMax(0, m_rows - 1));
    if (m_scrollTop > m_scrollBottom) {
        m_scrollTop = 0;
        m_scrollBottom = qMax(0, m_rows - 1);
    }
    m_normalDisplay.clear();
    m_altScreen = false;
    if (restoreCursorPos)
        restoreCursor();
    m_wrapPending = false;
    m_dirty = true;
}

void Screen::setDecMode(int mode, bool enable)
{
    switch (mode) {
    case 1: m_appCursor = enable; break;
    case 6: m_originMode = enable; if (enable) { m_r = m_scrollTop; m_c = 0; } break;
    case 7: m_autoWrap = enable; break;
    case 25: m_cursorVisible = enable; m_dirty = true; break;
    case 47:
    case 1047:
        if (enable)
            enterAltScreen(false);
        else
            leaveAltScreen(false);
        break;
    case 1048:
        if (enable)
            saveCursor();
        else
            restoreCursor();
        break;
    case 1049:
        if (enable)
            enterAltScreen(true);
        else
            leaveAltScreen(true);
        break;
    default: break;
    }
}

void Screen::reportCursor(QString *outSequence) const
{
    if (!outSequence)
        return;
    *outSequence = QStringLiteral("\x1b[%1;%2R").arg(m_r + 1).arg(m_c + 1);
}

const Cell &Screen::cell(int r, int c) const
{
    static Cell empty;
    if (r < 0 || r >= m_rows || c < 0 || c >= m_cols)
        return empty;
    return m_display[r][c];
}

QString Screen::textInRect(int r1, int c1, int r2, int c2, int scrollOffset) const
{
    if (r2 < r1 || (r2 == r1 && c2 < c1)) {
        qSwap(r1, r2);
        qSwap(c1, c2);
    }
    QString out;
    const int sb = static_cast<int>(m_scrollback.size());
    for (int abs = r1; abs <= r2; ++abs) {
        int lineIndex = abs + scrollOffset;
        const QVector<Cell> *line = nullptr;
        QVector<Cell> tmp;
        if (lineIndex < 0) {
            const int sbi = sb + lineIndex;
            if (sbi >= 0 && sbi < sb)
                line = &m_scrollback[static_cast<size_t>(sbi)];
        } else if (lineIndex < m_rows) {
            line = &m_display[lineIndex];
        }
        if (!line)
            continue;
        const int from = (abs == r1) ? c1 : 0;
        const int to = (abs == r2) ? c2 : (line->size() - 1);
        QString row;
        for (int c = from; c <= to && c < line->size(); ++c) {
            const Cell &cell = (*line)[c];
            if (cell.flags & Cell::WideCont)
                continue;
            if (cell.ch == 0 || cell.ch == U' ')
                row.append(QLatin1Char(' '));
            else
                row.append(ucs4ToString(cell.ch));
        }
        while (row.endsWith(QLatin1Char(' ')))
            row.chop(1);
        out += row;
        if (abs != r2)
            out += QLatin1Char('\n');
    }
    return out;
}

QString Screen::allText() const
{
    auto lineToText = [](const QVector<Cell> &line) {
        QString row;
        for (const Cell &cell : line) {
            if (cell.flags & Cell::WideCont)
                continue;
            if (cell.ch == 0 || cell.ch == U' ')
                row.append(QLatin1Char(' '));
            else
                row.append(ucs4ToString(cell.ch));
        }
        while (row.endsWith(QLatin1Char(' ')))
            row.chop(1);
        return row;
    };
    QStringList rows;
    for (const QVector<Cell> &line : m_scrollback)
        rows.append(lineToText(line));
    for (const QVector<Cell> &line : m_display)
        rows.append(lineToText(line));
    while (!rows.isEmpty() && rows.last().isEmpty())
        rows.removeLast();
    return rows.join(QLatin1Char('\n'));
}
