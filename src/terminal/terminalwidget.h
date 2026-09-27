#pragma once

#include "core/session.h"
#include "pty/ptyprocess.h"
#include "terminal/parser.h"
#include "terminal/screen.h"

#include <QFont>
#include <QWidget>

class QTimer;
class QShowEvent;
class QHideEvent;

class TerminalWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);

    bool startSession(const Session &session,
                      const QString &password,
                      const QString &controlPath = QString(),
                      bool resetScreen = true);
    void sendText(const QString &text, bool addNewline = false);
    void sendBytes(const QByteArray &data);
    void copySelection();
    void copyAll();
    void pasteClipboard();
    void clearScreen();
    void setColorScheme(const QString &name);
    void setTerminalFont(const QFont &font);
    void applyDisplaySettings();
    void zoom(int deltaPoints);
    QFont terminalFont() const { return m_font; }
    bool findText(const QString &needle, bool forward);
    void disconnectSession();
    void writeLocal(const QString &text);
    QString recentPlainText(int maxLines = 12) const;
    void enableCwdReporting();
    QString remoteCwd() const { return m_remoteCwd; }
    void refreshCwdFromScreen();
    int cols() const { return m_screen.cols(); }
    int rows() const { return m_screen.rows(); }
    bool isRunning() const;
    Session session() const { return m_session; }
    QString logFile() const { return m_logFile; }
    qint64 historyId() const { return m_historyId; }
    void setHistoryId(qint64 id) { m_historyId = id; }

signals:
    void titleChanged(const QString &title);
    void cwdChanged(const QString &cwd);
    void sessionFinished(int exitCode);
    void activity();
    void bell();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool event(QEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

private:
    void onPtyData(const QByteArray &data);
    void updateMetrics();
    void applySizeToPty();
    QRect cellRect(int row, int col) const;
    QPoint cellAt(const QPoint &pos) const;
    QRect cursorRect() const;
    void appendLog(const QByteArray &data);
    Cell visibleCell(int viewRow, int col) const;
    void tryAdvanceJump(const QByteArray &data);
    void tryAutoPassword(const QByteArray &data);
    void finishJumps();
    void sendNextJump();
    QString lastPromptLine(const QString &plain) const;
    bool looksLikeShellPrompt(const QString &line) const;

    Session m_session;
    Screen m_screen;
    VtParser m_parser;
    PtyProcess m_pty;
    QFont m_font;
    int m_cellW = 8;
    int m_cellH = 16;
    int m_ascent = 12;
    int m_scrollOffset = 0;
    bool m_selecting = false;
    int m_selAbs1 = 0;
    int m_selCol1 = 0;
    int m_selAbs2 = 0;
    int m_selCol2 = 0;
    bool m_hasSelection = false;
    class QScrollBar *m_vbar = nullptr;
    QTimer *m_resizePtyTimer = nullptr;
    int viewToAbs(int viewRow) const;
    int absToView(int absLine) const;
    void syncScrollBar();
    void selectWordAt(const QPoint &cell);
    QString selectedText() const;
    QTimer *m_cursorTimer = nullptr;
    bool m_cursorOn = true;
    bool m_activityQuiet = false;
    QString m_logFile;
    qint64 m_historyId = 0;
    QByteArray m_logBuffer;
    int m_findAbs = 0;
    int m_findCol = 0;
    bool m_hasFind = false;
    QString m_remoteCwd;
    bool m_cwdHookSent = false;
    QString m_destPassword;
    QVector<JumpHop> m_jumpQueue;
    int m_jumpIndex = 0;
    QString m_jumpScan;
    bool m_jumpWaitPassword = false;
    bool m_jumpActive = false;
    bool m_autoPasswordSent = false;
    bool m_skipAutoPassword = false;
    bool m_inAltScreen = false;
    QString m_autoScan;
    QString m_pendingStartup;
};
