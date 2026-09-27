#pragma once

#include "core/session.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QSpinBox;

class QuickConnectBar : public QWidget
{
    Q_OBJECT
public:
    explicit QuickConnectBar(QWidget *parent = nullptr);

signals:
    void connectRequested(const Session &session, const QString &password);

private:
    QComboBox *m_protocol = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QLineEdit *m_user = nullptr;
    QLineEdit *m_password = nullptr;
};
