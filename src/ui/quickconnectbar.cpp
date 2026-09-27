#include "ui/quickconnectbar.h"
#include "ui/icons.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

QuickConnectBar::QuickConnectBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("compactBar"));
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 2, 8, 2);
    lay->setSpacing(4);

    m_protocol = new QComboBox;
    m_protocol->addItem(AppIcons::protocol(Protocol::Ssh), QStringLiteral("SSH"),
                        static_cast<int>(Protocol::Ssh));
    m_protocol->addItem(AppIcons::protocol(Protocol::Telnet), QStringLiteral("Telnet"),
                        static_cast<int>(Protocol::Telnet));
    m_protocol->addItem(AppIcons::protocol(Protocol::Local), QStringLiteral("本地"),
                        static_cast<int>(Protocol::Local));
    m_protocol->setFixedWidth(96);
    m_protocol->setIconSize(QSize(16, 16));

    m_host = new QLineEdit;
    m_host->setPlaceholderText(QStringLiteral("主机 / IP"));
    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(22);
    m_port->setFixedWidth(70);
    m_user = new QLineEdit;
    m_user->setPlaceholderText(QStringLiteral("用户"));
    m_user->setFixedWidth(96);
    m_password = new QLineEdit;
    m_password->setPlaceholderText(QStringLiteral("密码"));
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setFixedWidth(110);

    auto *go = new QPushButton;
    go->setIcon(AppIcons::get(QStringLiteral("connect")));
    go->setIconSize(QSize(16, 16));
    go->setToolTip(QStringLiteral("连接"));
    go->setFixedSize(32, 26);
    connect(go, &QPushButton::clicked, this, [this] {
        Session s;
        s.protocol = static_cast<Protocol>(m_protocol->currentData().toInt());
        s.host = m_host->text().trimmed();
        s.port = m_port->value();
        s.username = m_user->text().trimmed();
        emit connectRequested(s, m_password->text());
    });
    connect(m_host, &QLineEdit::returnPressed, go, &QPushButton::click);
    connect(m_password, &QLineEdit::returnPressed, go, &QPushButton::click);
    connect(m_protocol, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int) {
        const auto p = static_cast<Protocol>(m_protocol->currentData().toInt());
        if (p == Protocol::Telnet)
            m_port->setValue(23);
        else if (p == Protocol::Ssh)
            m_port->setValue(22);
    });

    auto *lab = new QLabel;
    lab->setPixmap(AppIcons::get(QStringLiteral("quick")).pixmap(16, 16));
    lab->setToolTip(QStringLiteral("快速连接"));
    lay->addWidget(lab);
    lay->addWidget(m_protocol);
    lay->addWidget(m_host, 1);
    lay->addWidget(m_port);
    lay->addWidget(m_user);
    lay->addWidget(m_password);
    lay->addWidget(go);
}
