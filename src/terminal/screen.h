#pragma once

#include "terminal/cell.h"

#include <QRect>
#include <QString>
#include <QVector>
#include <deque>

class Screen
{
public:
    explicit Screen(int cols = 80, int rows = 24);

    void reset(const ColorScheme &scheme);
    void applyScheme(const ColorScheme &scheme);
    void resize(int cols, int rows);

    void putCodepoint(char32_t ch);
    void bel();
    void backspace();
    void tab();
    void lineFeed();
    void carriageReturn();
    void reverseIndex();
    void index();

    void cup(int row, int col);
    void cuu(int n);
    void cud(int n);
    void cuf(int n);
    void cub(int n);
    void cha(int col);
    void vpa(int row);
    void ed(int mode);
    void el(int mode);
    void il(int n);
    void dl(int n);
    void ich(int n);
    void dch(int n);
    void ech(int n);
    void su(int n);
    void sd(int n);
    void setScrollRegion(int top, int bottom);
    void sgr(const QVector<int> &params);
    void saveCursor();
    void restoreCursor();
    void setDecMode(int mode, bool enable);
    void reportCursor(QString *outSequence) const;
    void setMaxScrollback(int lines);
    int maxScrollback() const { return m_maxScrollback; }
    int consumeDroppedLines();
    bool altScreen() const { return m_altScreen; }

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
    int cursorRow() const { return m_r; }
    int cursorCol() const { return m_c; }
    bool cursorVisible() const { return m_cursorVisible; }
    QString title() const { return m_title; }
    void setTitle(const QString &t) { m_title = t; }
    QString cwd() const { return m_cwd; }
    void setCwd(const QString &p) { if (!p.isEmpty()) m_cwd = p; }
    bool applicationCursorKeys() const { return m_appCursor; }

    const Cell &cell(int r, int c) const;
    int scrollbackSize() const { return static_cast<int>(m_scrollback.size()); }
    const QVector<Cell> &scrollbackLine(int i) const { return m_scrollback[static_cast<size_t>(i)]; }
    const QVector<Cell> &displayLine(int r) const { return m_display[r]; }
    ColorScheme scheme() const { return m_scheme; }

    QString textInRect(int r1, int c1, int r2, int c2, int scrollOffset) const;
    QString allText() const;
    bool dirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }

private:
    QVector<Cell> blankLine() const;
    void ensureCursor();
    void scrollUp(int n);
    void scrollDown(int n);
    void pushScrollback(const QVector<Cell> &line);
    void wrapIfNeeded();
    int charWidth(char32_t ch) const;
    Cell styledBlank() const;

    int m_cols = 80;
    int m_rows = 24;
    int m_r = 0;
    int m_c = 0;
    int m_scrollTop = 0;
    int m_scrollBottom = 23;
    int m_savedR = 0;
    int m_savedC = 0;
    bool m_originMode = false;
    bool m_autoWrap = true;
    bool m_insertMode = false;
    bool m_cursorVisible = true;
    bool m_appCursor = false;
    bool m_dirty = true;
    bool m_wrapPending = false;
    Cell m_pen;
    Cell m_savedPen;
    ColorScheme m_scheme;
    QString m_title;
    QString m_cwd;
    void enterAltScreen(bool saveCursorPos);
    void leaveAltScreen(bool restoreCursorPos);

    QVector<QVector<Cell>> m_display;
    QVector<QVector<Cell>> m_normalDisplay;
    std::deque<QVector<Cell>> m_scrollback;
    int m_maxScrollback = 20000;
    int m_droppedLines = 0;
    bool m_altScreen = false;
    int m_normalR = 0;
    int m_normalC = 0;
    int m_normalScrollTop = 0;
    int m_normalScrollBottom = 23;
    int m_altScrollbackMark = 0;
};
