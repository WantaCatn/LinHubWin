#pragma once

#include <QByteArray>
#include <QThread>
#include <QtGlobal>

class WinPtyReader : public QThread
{
    Q_OBJECT
public:
    explicit WinPtyReader(quintptr readHandle, QObject *parent = nullptr)
        : QThread(parent)
        , m_handle(readHandle)
    {
    }

signals:
    void chunk(const QByteArray &data);
    void done();

protected:
    void run() override;

private:
    quintptr m_handle = 0;
};
