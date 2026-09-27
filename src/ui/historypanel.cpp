#include "ui/historypanel.h"
#include "storage/historyrepository.h"
#include "ui/icons.h"

#include <QColor>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

HistoryPanel::HistoryPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    auto *top = new QHBoxLayout;
    auto *title = new QLabel;
    title->setPixmap(AppIcons::get(QStringLiteral("history")).pixmap(16, 16));
    title->setToolTip(QStringLiteral("连接历史（无上限记录）"));
    auto *titleText = new QLabel(QStringLiteral("连接历史"));
    auto *refresh = new QPushButton;
    refresh->setIcon(AppIcons::get(QStringLiteral("reconnect")));
    refresh->setToolTip(QStringLiteral("刷新"));
    refresh->setProperty("secondary", true);
    refresh->setFixedSize(28, 24);
    connect(refresh, &QPushButton::clicked, this, &HistoryPanel::reload);
    top->addWidget(title);
    top->addWidget(titleText);
    top->addStretch();
    top->addWidget(refresh);
    root->addLayout(top);

    m_table = new QTableWidget(0, 6, this);
    m_table->setIconSize(QSize(16, 16));
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("时间"), QStringLiteral("会话"), QStringLiteral("协议"),
        QStringLiteral("主机"), QStringLiteral("时长"), QStringLiteral("结果")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const auto *it = m_table->item(row, 1);
        if (!it)
            return;
        const qint64 sid = it->data(Qt::UserRole).toLongLong();
        if (sid)
            emit reconnectRequested(sid);
    });
    root->addWidget(m_table, 1);
}

void HistoryPanel::reload()
{
    HistoryRepository repo;
    const auto rows = repo.recent(2000);
    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const auto &r = rows[i];
        auto *time = new QTableWidgetItem(r.startedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        auto *name = new QTableWidgetItem(AppIcons::protocol(r.protocol), r.sessionName);
        name->setData(Qt::UserRole, r.sessionId);
        auto *proto = new QTableWidgetItem(protocolDisplayName(r.protocol));
        auto *host = new QTableWidgetItem(r.host);
        const int m = r.durationSec / 60;
        const int s = r.durationSec % 60;
        auto *dur = new QTableWidgetItem(QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0'))
                                             .arg(s, 2, 10, QLatin1Char('0')));
        auto *ok = new QTableWidgetItem(r.success ? QStringLiteral("成功") : QStringLiteral("结束/失败"));
        ok->setForeground(r.success ? QColor(QStringLiteral("#3fb950"))
                                    : QColor(QStringLiteral("#f85149")));
        proto->setForeground(QColor(QStringLiteral("#58a6ff")));
        m_table->setItem(i, 0, time);
        m_table->setItem(i, 1, name);
        m_table->setItem(i, 2, proto);
        m_table->setItem(i, 3, host);
        m_table->setItem(i, 4, dur);
        m_table->setItem(i, 5, ok);
    }
}
