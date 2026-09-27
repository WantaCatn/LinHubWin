#pragma once

#include <QWidget>

class QKeyEvent;
class QLineEdit;

class FindBar : public QWidget
{
    Q_OBJECT
public:
    explicit FindBar(QWidget *parent = nullptr);
    void focusInput();
    QString text() const;

signals:
    void findNext();
    void findPrev();
    void closed();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QLineEdit *m_edit = nullptr;
};
