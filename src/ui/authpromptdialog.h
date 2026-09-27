#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QString>

class AuthPromptDialog : public QDialog
{
    Q_OBJECT
public:
    static bool prompt(QWidget *parent,
                       const QString &title,
                       const QString &label,
                       QLineEdit::EchoMode echo,
                       const QString &checkText,
                       QString *value,
                       bool *remember);
    static bool confirm(QWidget *parent,
                        const QString &title,
                        const QString &text,
                        const QString &checkText,
                        bool *checked);

private:
    AuthPromptDialog(QWidget *parent,
                     const QString &title,
                     const QString &label,
                     QLineEdit::EchoMode echo,
                     const QString &checkText);
    QString value() const;
    bool remember() const;

    QLineEdit *m_edit = nullptr;
    class QCheckBox *m_remember = nullptr;
};
