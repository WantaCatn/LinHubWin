#include "ui/findbar.h"
#include "ui/icons.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("findBar"));
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 2, 8, 2);
    lay->setSpacing(4);

    auto *lab = new QLabel;
    lab->setPixmap(AppIcons::get(QStringLiteral("find")).pixmap(16, 16));
    m_edit = new QLineEdit;
    m_edit->setPlaceholderText(QStringLiteral("在终端中查找"));
    auto *prev = new QPushButton;
    prev->setIcon(AppIcons::get(QStringLiteral("find-prev")));
    prev->setToolTip(QStringLiteral("上一个"));
    prev->setProperty("secondary", true);
    prev->setFixedSize(28, 24);
    auto *next = new QPushButton;
    next->setIcon(AppIcons::get(QStringLiteral("find-next")));
    next->setToolTip(QStringLiteral("下一个"));
    next->setProperty("secondary", true);
    next->setFixedSize(28, 24);
    auto *close = new QPushButton;
    close->setIcon(AppIcons::get(QStringLiteral("close")));
    close->setToolTip(QStringLiteral("关闭"));
    close->setProperty("secondary", true);
    close->setFixedSize(28, 24);

    connect(m_edit, &QLineEdit::returnPressed, this, &FindBar::findNext);
    connect(next, &QPushButton::clicked, this, &FindBar::findNext);
    connect(prev, &QPushButton::clicked, this, &FindBar::findPrev);
    connect(close, &QPushButton::clicked, this, [this] {
        hide();
        emit closed();
    });

    lay->addWidget(lab);
    lay->addWidget(m_edit, 1);
    lay->addWidget(prev);
    lay->addWidget(next);
    lay->addWidget(close);
    hide();
}

void FindBar::focusInput()
{
    show();
    m_edit->setFocus();
    m_edit->selectAll();
}

QString FindBar::text() const
{
    return m_edit->text();
}

void FindBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        emit closed();
        return;
    }
    QWidget::keyPressEvent(event);
}
