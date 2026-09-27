#include "ui/composebar.h"
#include "storage/commandrepository.h"
#include "ui/icons.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>

ComposeBar::ComposeBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("compactBarBottom"));
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 2, 8, 2);
    lay->setSpacing(4);

    auto *lab = new QLabel;
    lab->setPixmap(AppIcons::get(QStringLiteral("send")).pixmap(16, 16));
    lab->setToolTip(QStringLiteral("发送命令"));
    m_edit = new QLineEdit;
    m_edit->setPlaceholderText(QStringLiteral("命令，Enter 发送"));
    m_quick = new QComboBox;
    m_quick->setMinimumWidth(140);
    m_quick->setMaximumWidth(220);
    m_quick->setEditable(false);
    auto *toAll = new QToolButton;
    toAll->setIcon(AppIcons::get(QStringLiteral("broadcast")));
    toAll->setIconSize(QSize(16, 16));
    toAll->setCheckable(true);
    toAll->setAutoRaise(true);
    toAll->setToolTip(QStringLiteral("广播到全部标签"));
    toAll->setFixedSize(28, 26);
    auto *send = new QPushButton;
    send->setIcon(AppIcons::get(QStringLiteral("send")));
    send->setIconSize(QSize(16, 16));
    send->setToolTip(QStringLiteral("发送"));
    send->setFixedSize(32, 26);
    auto *manage = new QPushButton;
    manage->setIcon(AppIcons::get(QStringLiteral("settings")));
    manage->setIconSize(QSize(14, 14));
    manage->setProperty("secondary", true);
    manage->setFixedSize(28, 26);
    manage->setToolTip(QStringLiteral("管理快速命令"));

    auto fire = [this, toAll] {
        QString cmd = m_edit->text();
        if (cmd.isEmpty() && m_quick->currentIndex() > 0)
            cmd = m_quick->currentData().toString();
        if (!cmd.isEmpty()) {
            emit sendRequested(cmd, toAll->isChecked());
            m_edit->clear();
        }
    };
    connect(send, &QPushButton::clicked, this, fire);
    connect(m_edit, &QLineEdit::returnPressed, this, fire);
    connect(m_quick, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [this, fire](int index) {
        if (index > 0)
            fire();
    });
    connect(manage, &QPushButton::clicked, this, &ComposeBar::manageRequested);

    lay->addWidget(lab);
    lay->addWidget(m_edit, 1);
    lay->addWidget(m_quick);
    lay->addWidget(toAll);
    lay->addWidget(send);
    lay->addWidget(manage);
    reloadCommands();
}

void ComposeBar::reloadCommands()
{
    m_quick->clear();
    m_quick->addItem(AppIcons::get(QStringLiteral("compose")), QStringLiteral("快速命令"));
    CommandRepository repo;
    repo.seedDefaultsIfEmpty();
    for (const auto &c : repo.all())
        m_quick->addItem(c.name, c.command);
}
