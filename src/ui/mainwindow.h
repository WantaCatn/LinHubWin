#pragma once

#include "core/session.h"

#include <QList>
#include <QMainWindow>

class QTabWidget;
class QLineEdit;
class QToolBar;
class QAction;
class QDockWidget;
class QWidget;
class SessionTreeWidget;
class QuickConnectBar;
class ComposeBar;
class HistoryPanel;
class SftpWidget;
class TabTerminal;
class StartPage;
class FindBar;
class SessionPane;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    enum SplitMode { SplitSingle, SplitHorizontal, SplitVertical, SplitQuad };

    void openSession(Session session, const QString &password = QString());
    void openLocalShell();
    void createSession(qint64 folderId);
    void editSession(const Session &session);
    void createFolder(qint64 parentId);
    void cloneSession(const Session &session);
    void renameFolder(qint64 id);
    void exportSessions();
    void importSessions();
    TabTerminal *currentTab() const;
    SessionPane *paneOf(QWidget *w) const;
    void sendToCurrentOrAll(const QString &command, bool toAll);
    void updateStatus();
    void applyAppearance();
    void reconnectCurrent();
    void duplicateCurrent();
    void closeOtherTabs();
    void applyTermFontToAll();
    void openSettings();
    void showStartPage();
    void findNext();
    void findPrev();
    void toggleFullscreen();
    void disconnectCurrent();
    void copyCurrent();
    void pasteCurrent();
    QAction *tbAction(const QString &icon, const QString &tip, const QString &label = QString());
    void syncSftpFollow();
    void updateConnectionActions();
    void bindTab(TabTerminal *tab);
    void setTabLed(TabTerminal *tab);
    void reloadStartPages();
    SessionPane *createPane();
    void setActivePane(SessionPane *pane);
    void syncSftpToPane(SessionPane *pane);
    void applySplit(SplitMode mode);
    void rebuildSplitLayout();
    void updateSplitActions();
    void wirePane(SessionPane *pane);

    SessionTreeWidget *m_tree = nullptr;
    QTabWidget *m_tabs = nullptr;
    QuickConnectBar *m_quick = nullptr;
    ComposeBar *m_compose = nullptr;
    HistoryPanel *m_history = nullptr;
    SftpWidget *m_sftp = nullptr;
    QLineEdit *m_search = nullptr;
    QToolBar *m_toolbar = nullptr;
    QAction *m_actQuick = nullptr;
    QAction *m_actCompose = nullptr;
    QAction *m_actReconnect = nullptr;
    QAction *m_actDisconnect = nullptr;
    QAction *m_actSftp = nullptr;
    QAction *m_actSplitH = nullptr;
    QAction *m_actSplitV = nullptr;
    QAction *m_actSplitQ = nullptr;
    QAction *m_actSplitNone = nullptr;
    QDockWidget *m_sftpDock = nullptr;
    StartPage *m_startPage = nullptr;
    FindBar *m_find = nullptr;
    QWidget *m_splitHolder = nullptr;
    QWidget *m_splitRoot = nullptr;
    QList<SessionPane *> m_panes;
    SessionPane *m_activePane = nullptr;
    SplitMode m_splitMode = SplitSingle;
};
