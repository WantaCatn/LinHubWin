#pragma once

#include <QWidget>

class QListWidget;

class StartPage : public QWidget
{
    Q_OBJECT
public:
    explicit StartPage(QWidget *parent = nullptr);
    void reload();

protected:
    void paintEvent(QPaintEvent *event) override;

signals:
    void openSessionId(qint64 sessionId);
    void newSshRequested();
    void localShellRequested();

private:
    QListWidget *m_recent = nullptr;
    QListWidget *m_fav = nullptr;
};
