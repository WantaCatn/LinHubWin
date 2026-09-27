#pragma once

#include <QWidget>

class QTableWidget;

class HistoryPanel : public QWidget
{
    Q_OBJECT
public:
    explicit HistoryPanel(QWidget *parent = nullptr);
    void reload();

signals:
    void reconnectRequested(qint64 sessionId);

private:
    QTableWidget *m_table = nullptr;
};
