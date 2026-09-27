#pragma once

#include "core/session.h"

#include <QVector>

class HistoryRepository
{
public:
    qint64 insertStart(const ConnectionRecord &record);
    bool finish(qint64 id, bool success, const QString &error, const QString &logFile);
    QVector<ConnectionRecord> recent(int limit = 1000) const;
    QVector<ConnectionRecord> bySession(qint64 sessionId, int limit = 200) const;
    bool removeOlderThanDays(int days);
    int count() const;
};
