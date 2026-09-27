#include "ui/authpromptdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AuthPromptDialog::AuthPromptDialog(QWidget *parent,
                                   const QString &title,
                                   const QString &label,
                                   QLineEdit::EchoMode echo,
                                   const QString &checkText)
    : QDialog(parent)
{
    setWindowTitle(title);
    setModal(true);
    setWindowFlags((windowFlags() | Qt::MSWindowsFixedSizeDialogHint) & ~Qt::WindowContextHelpButtonHint);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(8);
    root->setSizeConstraint(QLayout::SetFixedSize);

    auto *lab = new QLabel(label);
    lab->setWordWrap(true);
    lab->setMaximumWidth(360);
    root->addWidget(lab);

    m_edit = new QLineEdit;
    m_edit->setEchoMode(echo);
    m_edit->setMinimumWidth(320);
    root->addWidget(m_edit);

    m_remember = new QCheckBox(checkText);
    m_remember->setChecked(true);
    root->addWidget(m_remember);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto *ok = box->button(QDialogButtonBox::Ok);
    ok->setText(QStringLiteral("确定"));
    ok->setDefault(true);
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);

    m_edit->setFocus();
}

QString AuthPromptDialog::value() const
{
    return m_edit->text();
}

bool AuthPromptDialog::remember() const
{
    return m_remember->isChecked();
}

bool AuthPromptDialog::prompt(QWidget *parent,
                              const QString &title,
                              const QString &label,
                              QLineEdit::EchoMode echo,
                              const QString &checkText,
                              QString *value,
                              bool *remember)
{
    AuthPromptDialog dlg(parent, title, label, echo, checkText);
    if (value && !value->isEmpty())
        dlg.m_edit->setText(*value);
    if (dlg.exec() != QDialog::Accepted)
        return false;
    if (value) {
        *value = (echo == QLineEdit::Password) ? dlg.value() : dlg.value().trimmed();
    }
    if (remember)
        *remember = dlg.remember();
    return true;
}

bool AuthPromptDialog::confirm(QWidget *parent,
                               const QString &title,
                               const QString &text,
                               const QString &checkText,
                               bool *checked)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setModal(true);
    dlg.setWindowFlags((dlg.windowFlags() | Qt::MSWindowsFixedSizeDialogHint)
                       & ~Qt::WindowContextHelpButtonHint);
    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 14, 16, 12);
    root->setSpacing(10);
    root->setSizeConstraint(QLayout::SetFixedSize);
    auto *lab = new QLabel(text);
    lab->setWordWrap(true);
    lab->setMinimumWidth(280);
    lab->setMaximumWidth(420);
    root->addWidget(lab);
    auto *boxCheck = new QCheckBox(checkText);
    boxCheck->setChecked(checked && *checked);
    root->addWidget(boxCheck);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    box->button(QDialogButtonBox::Ok)->setDefault(true);
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    root->addWidget(box);
    if (dlg.exec() != QDialog::Accepted)
        return false;
    if (checked)
        *checked = boxCheck->isChecked();
    return true;
}
