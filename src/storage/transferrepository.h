#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

struct TransferRecord {
    qint64 id = 0;
    QDateTime createdAt;
    bool upload = false;
    bool isDir = false;
    QString name;
    QString localPath;
    QString remotePath;
    qint64 size = 0;
    QString status;
    QString message;
    qint64 sessionId = 0;
};

class TransferRepository
{
public:
    qint64 insert(const TransferRecord &record);
    bool remove(qint64 id);
    bool removeMany(const QVector<qint64> &ids);
    QVector<TransferRecord> recent() const;
    void purgeOlderThanDays(int days);
};
