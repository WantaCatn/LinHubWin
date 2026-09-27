#pragma once

#include <QWidget>

class QComboBox;
class QLineEdit;

class ComposeBar : public QWidget
{
    Q_OBJECT
public:
    explicit ComposeBar(QWidget *parent = nullptr);
    void reloadCommands();

signals:
    void sendRequested(const QString &command, bool toAll);
    void manageRequested();

private:
    QLineEdit *m_edit = nullptr;
    QComboBox *m_quick = nullptr;
};
