#include "storage/historyrepository.h"
#include "storage/databasemanager.h"
#include "util/qtcompat.h"

#include <QSqlQuery>
#include <QVariant>

namespace {

ConnectionRecord recordFromQuery(const QSqlQuery &q)
{
    ConnectionRecord r;
    r.id = q.value(QStringLiteral("id")).toLongLong();
    r.sessionId = q.value(QStringLiteral("session_id")).toLongLong();
    r.sessionName = q.value(QStringLiteral("session_name")).toString();
    r.protocol = protocolFromString(q.value(QStringLiteral("protocol")).toString());
    r.host = q.value(QStringLiteral("host")).toString();
    r.username = q.value(QStringLiteral("username")).toString();
    r.startedAt = parseSqliteDateTime(q.value(QStringLiteral("started_at")).toString());
    r.endedAt = parseSqliteDateTime(q.value(QStringLiteral("ended_at")).toString());
    r.durationSec = q.value(QStringLiteral("duration_sec")).toInt();
    r.success = q.value(QStringLiteral("success")).toInt() != 0;
    r.errorMessage = q.value(QStringLiteral("error_message")).toString();
    r.logFile = q.value(QStringLiteral("log_file")).toString();
    return r;
}

} // namespace

qint64 HistoryRepository::insertStart(const ConnectionRecord &record)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral(
        "INSERT INTO connection_history(session_id, session_name, protocol, host, username, started_at) "
        "VALUES(?, ?, ?, ?, ?, datetime('now'))"));
    q.addBindValue(record.sessionId);
    q.addBindValue(record.sessionName);
    q.addBindValue(protocolToString(record.protocol));
    q.addBindValue(record.host);
    q.addBindValue(record.username);
    if (!q.exec())
        return 0;
    return q.lastInsertId().toLongLong();
}

bool HistoryRepository::finish(qint64 id, bool success, const QString &error, const QString &logFile)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral(
        "UPDATE connection_history SET ended_at=datetime('now'), "
        "duration_sec=CAST(strftime('%s','now') - strftime('%s', started_at) AS INTEGER), "
        "success=?, error_message=?, log_file=? WHERE id=?"));
    q.addBindValue(success ? 1 : 0);
    q.addBindValue(error);
    q.addBindValue(logFile);
    q.addBindValue(id);
    return q.exec();
}

QVector<ConnectionRecord> HistoryRepository::recent(int limit) const
{
    QVector<ConnectionRecord> out;
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral("SELECT * FROM connection_history ORDER BY id DESC LIMIT ?"));
    q.addBindValue(limit);
    q.exec();
    while (q.next())
        out.push_back(recordFromQuery(q));
    return out;
}

QVector<ConnectionRecord> HistoryRepository::bySession(qint64 sessionId, int limit) const
{
    QVector<ConnectionRecord> out;
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral(
        "SELECT * FROM connection_history WHERE session_id=? ORDER BY id DESC LIMIT ?"));
    q.addBindValue(sessionId);
    q.addBindValue(limit);
    q.exec();
    while (q.next())
        out.push_back(recordFromQuery(q));
    return out;
}

bool HistoryRepository::removeOlderThanDays(int days)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral(
        "DELETE FROM connection_history WHERE started_at < datetime('now', ?)"));
    q.addBindValue(QStringLiteral("-%1 days").arg(days));
    return q.exec();
}

int HistoryRepository::count() const
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.exec(QStringLiteral("SELECT COUNT(*) FROM connection_history"));
    if (q.next())
        return q.value(0).toInt();
    return 0;
}
