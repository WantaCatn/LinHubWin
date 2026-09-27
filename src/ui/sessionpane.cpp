#include "ui/sessionpane.h"

#include "ui/icons.h"
#include "ui/startpage.h"

#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QVector>
#include <QWheelEvent>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

namespace {

QPointer<QWidget> g_dragTab;
QPointer<SessionPane> g_dragSource;
QString g_dragTitle;
QIcon g_dragIcon;

class SessionTabBar : public QTabBar
{
public:
    explicit SessionTabBar(QWidget *parent = nullptr)
        : QTabBar(parent)
    {
        setElideMode(Qt::ElideRight);
        setUsesScrollButtons(false);
        setExpanding(false);
        setMovable(true);
        setDocumentMode(true);
        setDrawBase(false);
        setTabsClosable(true);
        m_ledTimer = new QTimer(this);
        m_ledTimer->setInterval(100);
        connect(m_ledTimer, &QTimer::timeout, this, [this] {
            if (!isVisible())
                return;
            bool pulse = false;
            for (int led : m_leds) {
                if (led == 1 || led == 2) {
                    pulse = true;
                    break;
                }
            }
            if (!pulse) {
                m_ledTimer->stop();
                return;
            }
            m_phase += 0.22;
            update();
        });
        connect(this, &QTabBar::tabMoved, this, [this](int from, int to) {
            if (from < 0 || to < 0)
                return;
            const int last = qMax(from, to);
            while (m_leds.size() <= last)
                m_leds.append(0);
            m_leds.move(from, to);
        });
    }

    int contentWidth() const
    {
        int w = 4;
        for (int i = 0; i < count(); ++i)
            w += tabSizeHint(i).width();
        return count() == 0 ? 8 : w;
    }
    int contentHeight() const
    {
        int h = 30;
        for (int i = 0; i < count(); ++i)
            h = qMax(h, tabSizeHint(i).height());
        return h;
    }

    void setLed(int index, int led)
    {
        if (index < 0)
            return;
        while (m_leds.size() <= index)
            m_leds.append(0);
        if (m_leds[index] == led)
            return;
        m_leds[index] = led;
        bool pulse = false;
        for (int v : m_leds) {
            if (v == 1 || v == 2) {
                pulse = true;
                break;
            }
        }
        if (pulse && m_ledTimer && !m_ledTimer->isActive() && isVisible())
            m_ledTimer->start();
        if (!pulse && m_ledTimer)
            m_ledTimer->stop();
        update();
    }

protected:
    void tabInserted(int index) override
    {
        QTabBar::tabInserted(index);
        if (index < 0)
            index = 0;
        if (index > m_leds.size())
            index = m_leds.size();
        m_leds.insert(index, 0);
    }
    void tabRemoved(int index) override
    {
        QTabBar::tabRemoved(index);
        if (index >= 0 && index < m_leds.size())
            m_leds.removeAt(index);
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        m_pressIndex = tabAt(event->pos());
        QTabBar::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_pressIndex >= 0
            && (event->buttons() & Qt::LeftButton)
            && !rect().adjusted(-12, -12, 12, 20).contains(event->pos())) {
            SessionPane *pane = nullptr;
            for (QWidget *w = parentWidget(); w; w = w->parentWidget()) {
                pane = qobject_cast<SessionPane *>(w);
                if (pane)
                    break;
            }
            if (pane) {
                const int index = m_pressIndex;
                m_pressIndex = -1;
                QMouseEvent rel(QEvent::MouseButtonRelease, event->pos(), event->globalPos(),
                                Qt::LeftButton, Qt::NoButton, event->modifiers());
                QTabBar::mouseReleaseEvent(&rel);
                pane->beginTabDrag(index);
                return;
            }
        }
        QTabBar::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_pressIndex = -1;
        QTabBar::mouseReleaseEvent(event);
    }
    QSize tabSizeHint(int index) const override
    {
        QSize s = QTabBar::tabSizeHint(index);
        s.setWidth(qBound(88, s.width() + 8, 168));
        return s;
    }
    void paintEvent(QPaintEvent *event) override
    {
        QTabBar::paintEvent(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        for (int i = 0; i < count(); ++i) {
            const QRect r = tabRect(i);
            const int led = (i < m_leds.size()) ? m_leds.at(i) : 0;
            if (led < 0)
                continue;
            QColor c(26, 109, 255);
            if (led == 1) {
                const qreal a = 0.40 + 0.60 * (0.5 + 0.5 * qSin(m_phase));
                c = QColor(34, 197, 94, int(255 * a));
            } else if (led == 2) {
                const qreal a = 0.35 + 0.65 * (0.5 + 0.5 * qSin(m_phase * 0.9));
                c = QColor(239, 68, 68, int(255 * a));
            }
            const QPoint center(r.left() + 12, r.center().y());
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(c.red(), c.green(), c.blue(), qMax(30, c.alpha() / 3)));
            p.drawEllipse(center, 8, 8);
            p.setBrush(c);
            p.drawEllipse(center, 6, 6);
        }
    }

private:
    int m_pressIndex = -1;
    QVector<int> m_leds;
    qreal m_phase = 0;
    QTimer *m_ledTimer = nullptr;
};

} // namespace

SessionPane::SessionPane(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sessionPane"));
    setAcceptDrops(true);
    setFocusPolicy(Qt::ClickFocus);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(0);

    auto *strip = new QWidget(this);
    strip->setObjectName(QStringLiteral("sessionTabStrip"));
    auto *hl = new QHBoxLayout(strip);
    hl->setContentsMargins(0, 0, 2, 0);
    hl->setSpacing(4);

    m_homeBtn = new QToolButton(strip);
    m_homeBtn->setObjectName(QStringLiteral("startPageTab"));
    m_homeBtn->setIcon(AppIcons::get(QStringLiteral("home")));
    m_homeBtn->setIconSize(QSize(16, 16));
    m_homeBtn->setText(QStringLiteral("起始页"));
    m_homeBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_homeBtn->setCheckable(true);
    m_homeBtn->setChecked(true);
    m_homeBtn->setAutoRaise(false);
    m_homeBtn->setCursor(Qt::PointingHandCursor);
    m_homeBtn->setFixedHeight(30);
    m_homeBtn->setMinimumWidth(88);
    m_homeBtn->setFocusPolicy(Qt::NoFocus);

    m_bar = new SessionTabBar;
    m_bar->setObjectName(QStringLiteral("sessionTabBar"));
    m_bar->setIconSize(QSize(16, 16));
    m_bar->setFocusPolicy(Qt::ClickFocus);
    m_bar->setContextMenuPolicy(Qt::CustomContextMenu);

    m_tabScroll = new QScrollArea(strip);
    m_tabScroll->setObjectName(QStringLiteral("sessionTabScroll"));
    m_tabScroll->setWidget(m_bar);
    m_tabScroll->setWidgetResizable(false);
    m_tabScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tabScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tabScroll->setFrameShape(QFrame::NoFrame);
    m_tabScroll->setFocusPolicy(Qt::NoFocus);
    m_tabScroll->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_tabScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_scrollLeft = new QToolButton(strip);
    m_scrollLeft->setObjectName(QStringLiteral("tabScrollBtn"));
    m_scrollLeft->setArrowType(Qt::LeftArrow);
    m_scrollLeft->setFixedSize(28, 28);
    m_scrollLeft->setAutoRepeat(true);
    m_scrollLeft->setAutoRepeatDelay(180);
    m_scrollLeft->setAutoRepeatInterval(70);
    m_scrollLeft->setCursor(Qt::PointingHandCursor);
    m_scrollLeft->setFocusPolicy(Qt::NoFocus);
    m_scrollLeft->setToolTip(QStringLiteral("向前切换标签"));

    m_scrollRight = new QToolButton(strip);
    m_scrollRight->setObjectName(QStringLiteral("tabScrollBtn"));
    m_scrollRight->setArrowType(Qt::RightArrow);
    m_scrollRight->setFixedSize(28, 28);
    m_scrollRight->setAutoRepeat(true);
    m_scrollRight->setAutoRepeatDelay(180);
    m_scrollRight->setAutoRepeatInterval(70);
    m_scrollRight->setCursor(Qt::PointingHandCursor);
    m_scrollRight->setFocusPolicy(Qt::NoFocus);
    m_scrollRight->setToolTip(QStringLiteral("向后切换标签"));

    hl->addWidget(m_homeBtn, 0, Qt::AlignVCenter);
    hl->addWidget(m_tabScroll, 1);
    hl->addWidget(m_scrollLeft, 0, Qt::AlignVCenter);
    hl->addWidget(m_scrollRight, 0, Qt::AlignVCenter);

    m_startPage = new StartPage;
    m_sessionStack = new QStackedWidget;
    m_sessionStack->setObjectName(QStringLiteral("sessionPages"));
    m_rootStack = new QStackedWidget;
    m_rootStack->setObjectName(QStringLiteral("sessionRoot"));
    m_rootStack->addWidget(m_startPage);
    m_rootStack->addWidget(m_sessionStack);

    lay->addWidget(strip, 0);
    lay->addWidget(m_rootStack, 1);

    wireStartPage();
    showStartPage();

    connect(m_homeBtn, &QToolButton::clicked, this, [this] {
        showStartPage();
        emit activated();
    });
    connect(m_bar, &QTabBar::currentChanged, this, [this](int index) {
        if (index < 0)
            return;
        showSessionAt(index);
    });
    connect(m_bar, &QTabBar::tabCloseRequested, this, [this](int index) {
        closeTabAt(index);
    });
    connect(m_bar, &QTabBar::tabMoved, this, [this](int from, int to) {
        if (from < 0 || to < 0 || from == to)
            return;
        QWidget *w = m_sessionStack->widget(from);
        if (!w)
            return;
        m_sessionStack->removeWidget(w);
        m_sessionStack->insertWidget(to, w);
        syncTabBarWidth();
    });
    connect(m_bar, &QTabBar::tabBarDoubleClicked, this, [this](int index) {
        if (index < 0)
            return;
        auto *tab = qobject_cast<TabTerminal *>(widgetAt(index));
        if (tab)
            emit tabDuplicateRequested(tab);
    });
    connect(m_bar, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        emit activated();
        emit tabContextMenu(pos);
    });
    connect(m_scrollLeft, &QToolButton::clicked, this, [this] {
        if (!m_tabScroll)
            return;
        QScrollBar *sb = m_tabScroll->horizontalScrollBar();
        sb->setValue(sb->value() - 160);
    });
    connect(m_scrollRight, &QToolButton::clicked, this, [this] {
        if (!m_tabScroll)
            return;
        QScrollBar *sb = m_tabScroll->horizontalScrollBar();
        sb->setValue(sb->value() + 160);
    });
    connect(m_tabScroll->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        updateScrollButtons();
    });
    connect(m_tabScroll->horizontalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int) {
        updateScrollButtons();
    });

    m_rootStack->installEventFilter(this);
    m_sessionStack->installEventFilter(this);
    m_startPage->installEventFilter(this);
    m_bar->installEventFilter(this);
    m_tabScroll->viewport()->installEventFilter(this);
    QTimer::singleShot(0, this, [this] { syncTabBarWidth(); });
}

QWidget *SessionPane::tabs() const
{
    return m_sessionStack;
}

QTabBar *SessionPane::tabBar() const
{
    return m_bar;
}

void SessionPane::wireStartPage()
{
    connect(m_startPage, &StartPage::openSessionId, this, [this](qint64 id) {
        emit activated();
        emit startOpenSessionId(id);
    });
    connect(m_startPage, &StartPage::newSshRequested, this, [this] {
        emit activated();
        emit startNewSsh();
    });
    connect(m_startPage, &StartPage::localShellRequested, this, [this] {
        emit activated();
        emit startLocalShell();
    });
}

bool SessionPane::isShowingStartPage() const
{
    return m_rootStack && m_rootStack->currentWidget() == m_startPage;
}

TabTerminal *SessionPane::currentTerminal() const
{
    if (isShowingStartPage())
        return nullptr;
    return qobject_cast<TabTerminal *>(m_sessionStack->currentWidget());
}

QList<TabTerminal *> SessionPane::terminals() const
{
    QList<TabTerminal *> out;
    for (int i = 0; i < m_sessionStack->count(); ++i) {
        if (auto *t = qobject_cast<TabTerminal *>(m_sessionStack->widget(i)))
            out.append(t);
    }
    return out;
}

int SessionPane::indexOfWidget(QWidget *w) const
{
    return w ? m_sessionStack->indexOf(w) : -1;
}

QWidget *SessionPane::widgetAt(int index) const
{
    return m_sessionStack->widget(index);
}

void SessionPane::showSessionAt(int index)
{
    QWidget *w = widgetAt(index);
    if (!w)
        return;
    m_sessionStack->setCurrentWidget(w);
    m_rootStack->setCurrentWidget(m_sessionStack);
    if (m_homeBtn)
        m_homeBtn->setChecked(false);
    if (m_bar && m_bar->currentIndex() != index)
        m_bar->setCurrentIndex(index);
    const QRect r = m_bar->tabRect(index);
    if (r.isValid())
        m_tabScroll->ensureVisible(r.center().x(), 1, 40, 0);
    emit currentChanged();
    emit activated();
}

int SessionPane::addSessionTab(TabTerminal *tab, const QIcon &icon, const QString &title)
{
    m_sessionStack->addWidget(tab);
    const int index = m_bar->addTab(icon, title);
    tab->installEventFilter(this);
    if (tab->terminal())
        tab->terminal()->installEventFilter(this);
    syncTabBarWidth();
    m_bar->setCurrentIndex(index);
    showSessionAt(index);
    return index;
}

void SessionPane::showStartPage()
{
    if (m_startPage)
        m_startPage->reload();
    m_rootStack->setCurrentWidget(m_startPage);
    if (m_homeBtn)
        m_homeBtn->setChecked(true);
    emit currentChanged();
}

void SessionPane::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_bar->count())
        return;
    m_bar->setCurrentIndex(index);
    showSessionAt(index);
}

void SessionPane::setTabLed(TabTerminal *tab, int led)
{
    const int i = indexOfWidget(tab);
    if (i >= 0)
        static_cast<SessionTabBar *>(m_bar)->setLed(i, led);
}

void SessionPane::setTabTitle(TabTerminal *tab, const QString &title)
{
    const int i = indexOfWidget(tab);
    if (i >= 0) {
        m_bar->setTabText(i, title);
        syncTabBarWidth();
    }
}

void SessionPane::setActive(bool on, bool splitVisible)
{
    if (m_active == on && m_splitVisible == splitVisible)
        return;
    m_active = on;
    m_splitVisible = splitVisible;
    setProperty("active", on ? QStringLiteral("true") : QStringLiteral("false"));
    if (style()) {
        style()->unpolish(this);
        style()->polish(this);
    }
    update();
}

void SessionPane::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    if (!m_splitVisible)
        return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    const QRect r = rect().adjusted(1, 1, -2, -2);
    if (m_active) {
        p.setPen(QPen(QColor(26, 109, 255), 3));
        p.drawRect(r);
        p.setPen(QPen(QColor(147, 197, 253), 1));
        p.drawRect(r.adjusted(3, 3, -3, -3));
    } else {
        p.setPen(QPen(QColor(43, 59, 85), 1));
        p.drawRect(r);
    }
}

void SessionPane::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    syncTabBarWidth();
}

void SessionPane::closeTabAt(int index)
{
    auto *w = widgetAt(index);
    if (!w)
        return;
    m_bar->removeTab(index);
    m_sessionStack->removeWidget(w);
    delete w;
    syncTabBarWidth();
    if (m_bar->count() <= 0)
        showStartPage();
    emit tabClosed();
}

void SessionPane::closeOtherTabs()
{
    auto *keep = m_sessionStack->currentWidget();
    for (int i = m_sessionStack->count() - 1; i >= 0; --i) {
        auto *w = m_sessionStack->widget(i);
        if (w == keep)
            continue;
        m_bar->removeTab(i);
        m_sessionStack->removeWidget(w);
        delete w;
    }
    syncTabBarWidth();
    emit tabClosed();
}

void SessionPane::moveSessionTabsTo(SessionPane *dest)
{
    if (!dest || dest == this)
        return;
    for (int i = m_sessionStack->count() - 1; i >= 0; --i) {
        auto *w = m_sessionStack->widget(i);
        if (!w)
            continue;
        const QString text = m_bar->tabText(i);
        const QIcon icon = m_bar->tabIcon(i);
        m_bar->removeTab(i);
        m_sessionStack->removeWidget(w);
        dest->m_sessionStack->addWidget(w);
        dest->m_bar->addTab(icon, text);
        dest->m_bar->setCurrentIndex(dest->m_bar->count() - 1);
        dest->m_sessionStack->setCurrentWidget(w);
        dest->m_rootStack->setCurrentWidget(dest->m_sessionStack);
        if (dest->m_homeBtn)
            dest->m_homeBtn->setChecked(false);
        if (auto *term = qobject_cast<TabTerminal *>(w)) {
            term->installEventFilter(dest);
            if (term->terminal())
                term->terminal()->installEventFilter(dest);
        }
    }
    syncTabBarWidth();
    dest->syncTabBarWidth();
    if (m_bar->count() <= 0)
        showStartPage();
}

void SessionPane::beginTabDrag(int index)
{
    auto *w = widgetAt(index);
    if (!w)
        return;

    g_dragTab = w;
    g_dragSource = this;
    g_dragTitle = m_bar->tabText(index);
    g_dragIcon = m_bar->tabIcon(index);

    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(linhubTabMime()), QByteArray::number(quintptr(w)));
    mime->setText(g_dragTitle);

    QDrag drag(this);
    drag.setMimeData(mime);
    const QRect r = m_bar->tabRect(index);
    if (r.isValid())
        drag.setPixmap(m_bar->grab(r));
    drag.exec(Qt::MoveAction);
    g_dragTab.clear();
    g_dragSource.clear();
    g_dragTitle.clear();
    g_dragIcon = QIcon();
}

bool SessionPane::contains(QWidget *w) const
{
    return w && (w == this || isAncestorOf(w));
}

void SessionPane::syncTabBarWidth()
{
    if (!m_bar || !m_tabScroll)
        return;
    auto *bar = static_cast<SessionTabBar *>(m_bar);
    const int w = bar->contentWidth();
    const int h = bar->contentHeight();
    m_bar->resize(w, h);
    m_tabScroll->setFixedHeight(h);
    updateScrollButtons();
}

void SessionPane::updateScrollButtons()
{
    if (!m_tabScroll || !m_scrollLeft || !m_scrollRight)
        return;
    QScrollBar *sb = m_tabScroll->horizontalScrollBar();
    const bool need = m_bar && (m_bar->width() > m_tabScroll->viewport()->width() + 2);
    m_scrollLeft->setVisible(need);
    m_scrollRight->setVisible(need);
    if (!need)
        return;
    m_scrollLeft->setEnabled(sb->value() > sb->minimum());
    m_scrollRight->setEnabled(sb->value() < sb->maximum());
}

namespace {

bool hasSessionDrag(const QMimeData *mime)
{
    return mime && mime->hasFormat(QString::fromLatin1(linhubSessionMime()));
}

bool hasTabDrag(const QMimeData *mime)
{
    return mime && mime->hasFormat(QString::fromLatin1(linhubTabMime()));
}

} // namespace

void SessionPane::dragEnterEvent(QDragEnterEvent *event)
{
    if (hasSessionDrag(event->mimeData()) || hasTabDrag(event->mimeData())) {
        event->acceptProposedAction();
        emit activated();
        return;
    }
    event->ignore();
}

void SessionPane::dragMoveEvent(QDragMoveEvent *event)
{
    if (hasSessionDrag(event->mimeData()) || hasTabDrag(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void SessionPane::dropEvent(QDropEvent *event)
{
    if (hasTabDrag(event->mimeData()) && g_dragTab && g_dragSource) {
        emit activated();
        if (g_dragSource == this) {
            event->acceptProposedAction();
            return;
        }
        auto *w = g_dragTab.data();
        const int srcIndex = g_dragSource->indexOfWidget(w);
        if (srcIndex < 0) {
            event->ignore();
            return;
        }
        const QString title = g_dragTitle.isEmpty() ? g_dragSource->m_bar->tabText(srcIndex)
                                                    : g_dragTitle;
        const QIcon icon = g_dragIcon.isNull() ? g_dragSource->m_bar->tabIcon(srcIndex)
                                               : g_dragIcon;
        g_dragSource->m_bar->removeTab(srcIndex);
        g_dragSource->m_sessionStack->removeWidget(w);
        g_dragSource->syncTabBarWidth();
        if (g_dragSource->m_bar->count() <= 0)
            g_dragSource->showStartPage();

        int at = m_bar->count();
        const QPoint barPos = m_bar->mapFrom(this, event->pos());
        const int hit = m_bar->tabAt(barPos);
        if (hit >= 0)
            at = hit;
        m_sessionStack->insertWidget(at, w);
        m_bar->insertTab(at, icon, title);
        syncTabBarWidth();
        m_bar->setCurrentIndex(at);
        showSessionAt(at);
        if (auto *term = qobject_cast<TabTerminal *>(w)) {
            term->installEventFilter(this);
            if (term->terminal())
                term->terminal()->installEventFilter(this);
            emit tabTransferred(term);
        }
        event->acceptProposedAction();
        return;
    }

    if (!hasSessionDrag(event->mimeData())) {
        event->ignore();
        return;
    }
    const QString raw = QString::fromUtf8(
        event->mimeData()->data(QString::fromLatin1(linhubSessionMime())));
    bool any = false;
    for (const QString &part : raw.split(QLatin1Char('\n'), QString::SkipEmptyParts)) {
        bool ok = false;
        const qint64 id = part.trimmed().toLongLong(&ok);
        if (ok && id) {
            if (!any)
                emit activated();
            emit sessionDropRequested(id);
            any = true;
        }
    }
    if (any) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

bool SessionPane::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn)
        emit activated();
    if (event->type() == QEvent::Resize && m_tabScroll && watched == m_tabScroll->viewport())
        updateScrollButtons();
    if (event->type() == QEvent::Wheel && (watched == m_bar || watched == m_tabScroll->viewport())) {
        auto *we = static_cast<QWheelEvent *>(event);
        const int delta = we->angleDelta().y() != 0 ? we->angleDelta().y() : we->angleDelta().x();
        if (delta != 0 && m_tabScroll) {
            QScrollBar *sb = m_tabScroll->horizontalScrollBar();
            sb->setValue(sb->value() - delta);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
