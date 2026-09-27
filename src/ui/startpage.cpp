#include "ui/startpage.h"
#include "storage/historyrepository.h"
#include "storage/sessionrepository.h"
#include "ui/icons.h"

#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QStyle>
#include <QStyleOption>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSet>
#include <QSize>
#include <QHBoxLayout>
#include <QVBoxLayout>

StartPage::StartPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("startPage"));
    setAutoFillBackground(true);
    setAttribute(Qt::WA_StyledBackground, true);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(16);

    auto *hero = new QHBoxLayout;
    auto *logo = new QLabel;
    logo->setPixmap(AppIcons::get(QStringLiteral("home")).pixmap(48, 48));
    auto *titleBox = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("LinHub"));
    title->setObjectName(QStringLiteral("startTitle"));
    auto *sub = new QLabel(QStringLiteral("会话库 · 终端 · SFTP · 连接历史"));
    sub->setObjectName(QStringLiteral("startSub"));
    titleBox->addWidget(title);
    titleBox->addWidget(sub);
    hero->addWidget(logo);
    hero->addSpacing(12);
    hero->addLayout(titleBox);
    hero->addStretch();
    root->addLayout(hero);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(10);
    auto makeBtn = [](const QString &icon, const QString &text) {
        auto *b = new QPushButton(text);
        b->setIcon(AppIcons::get(icon));
        b->setIconSize(QSize(20, 20));
        b->setMinimumHeight(36);
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };
    auto *newSsh = makeBtn(QStringLiteral("ssh"), QStringLiteral("  新建 SSH"));
    auto *local = makeBtn(QStringLiteral("local"), QStringLiteral("  本地 Shell"));
    auto *news = makeBtn(QStringLiteral("new"), QStringLiteral("  新建会话"));
    connect(newSsh, &QPushButton::clicked, this, &StartPage::newSshRequested);
    connect(news, &QPushButton::clicked, this, &StartPage::newSshRequested);
    connect(local, &QPushButton::clicked, this, &StartPage::localShellRequested);
    actions->addWidget(newSsh);
    actions->addWidget(local);
    actions->addWidget(news);
    actions->addStretch();
    root->addLayout(actions);

    auto *cols = new QHBoxLayout;
    auto left = new QVBoxLayout;
    auto *favLab = new QLabel(QStringLiteral("收藏会话"));
    favLab->setObjectName(QStringLiteral("startHead"));
    m_fav = new QListWidget;
    m_fav->setObjectName(QStringLiteral("startList"));
    m_fav->setIconSize(QSize(20, 20));
    left->addWidget(favLab);
    left->addWidget(m_fav, 1);

    auto right = new QVBoxLayout;
    auto *recLab = new QLabel(QStringLiteral("最近连接"));
    recLab->setObjectName(QStringLiteral("startHead"));
    m_recent = new QListWidget;
    m_recent->setObjectName(QStringLiteral("startList"));
    m_recent->setIconSize(QSize(20, 20));
    right->addWidget(recLab);
    right->addWidget(m_recent, 1);

    cols->addLayout(left, 1);
    cols->addLayout(right, 1);
    root->addLayout(cols, 1);

    auto openItem = [this](QListWidgetItem *it) {
        if (!it)
            return;
        const qint64 id = it->data(Qt::UserRole).toLongLong();
        if (id)
            emit openSessionId(id);
    };
    connect(m_fav, &QListWidget::itemDoubleClicked, this, openItem);
    connect(m_recent, &QListWidget::itemDoubleClicked, this, openItem);
    reload();
}

void StartPage::reload()
{
    m_fav->clear();
    m_recent->clear();
    SessionRepository repo;
    for (const auto &s : repo.allSessions()) {
        if (!s.favorite)
            continue;
        auto *it = new QListWidgetItem(AppIcons::protocol(s.protocol), s.displayTitle());
        it->setData(Qt::UserRole, s.id);
        it->setToolTip(s.host);
        m_fav->addItem(it);
    }
    if (m_fav->count() == 0)
        m_fav->addItem(QStringLiteral("双击会话树中的条目，或点右键收藏"));

    QSet<qint64> seen;
    for (const auto &r : HistoryRepository().recent(30)) {
        if (!r.sessionId || seen.contains(r.sessionId))
            continue;
        seen.insert(r.sessionId);
        const Session s = repo.byId(r.sessionId);
        const QString title = s.id ? s.displayTitle() : r.sessionName;
        auto *it = new QListWidgetItem(AppIcons::protocol(s.id ? s.protocol : r.protocol), title);
        it->setData(Qt::UserRole, r.sessionId);
        it->setToolTip(r.host + QLatin1Char(' ') + r.startedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        m_recent->addItem(it);
    }
    if (m_recent->count() == 0)
        m_recent->addItem(QStringLiteral("还没有连接记录"));
}

void StartPage::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), palette().color(QPalette::Window));
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
