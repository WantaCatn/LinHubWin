#pragma once

#include "ui/tabterminal.h"

#include <QIcon>
#include <QList>
#include <QWidget>

class QStackedWidget;
class QScrollArea;
class QTabBar;
class QToolButton;
class StartPage;

inline const char *linhubSessionMime()
{
    return "application/x-linhub-session-id";
}

inline const char *linhubTabMime()
{
    return "application/x-linhub-tab";
}

class SessionPane : public QWidget
{
    Q_OBJECT
public:
    explicit SessionPane(QWidget *parent = nullptr);

    QWidget *tabs() const;
    QTabBar *tabBar() const;
    StartPage *startPage() const { return m_startPage; }
    bool isShowingStartPage() const;
    TabTerminal *currentTerminal() const;
    QList<TabTerminal *> terminals() const;
    int addSessionTab(TabTerminal *tab, const QIcon &icon, const QString &title);
    void showStartPage();
    void setCurrentIndex(int index);
    void setTabLed(TabTerminal *tab, int led);
    void setTabTitle(TabTerminal *tab, const QString &title);
    void setActive(bool on, bool splitVisible = false);
    bool isActive() const { return m_active; }
    void closeTabAt(int index);
    void closeOtherTabs();
    void moveSessionTabsTo(SessionPane *dest);
    bool contains(QWidget *w) const;
    void beginTabDrag(int index);

signals:
    void activated();
    void currentChanged();
    void sessionDropRequested(qint64 sessionId);
    void tabTransferred(TabTerminal *tab);
    void startOpenSessionId(qint64 id);
    void startNewSsh();
    void startLocalShell();
    void tabClosed();
    void tabContextMenu(const QPoint &pos);
    void tabDuplicateRequested(TabTerminal *tab);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void wireStartPage();
    void showSessionAt(int index);
    void syncTabBarWidth();
    void updateScrollButtons();
    int indexOfWidget(QWidget *w) const;
    QWidget *widgetAt(int index) const;

    QStackedWidget *m_rootStack = nullptr;
    QStackedWidget *m_sessionStack = nullptr;
    QTabBar *m_bar = nullptr;
    QScrollArea *m_tabScroll = nullptr;
    QToolButton *m_homeBtn = nullptr;
    QToolButton *m_scrollLeft = nullptr;
    QToolButton *m_scrollRight = nullptr;
    StartPage *m_startPage = nullptr;
    bool m_active = false;
    bool m_splitVisible = false;
};
