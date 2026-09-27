#include "storage/transferrepository.h"
#include "storage/databasemanager.h"
#include "util/qtcompat.h"

#include <QSqlQuery>
#include <QVariant>

namespace {

TransferRecord recordFromQuery(const QSqlQuery &q)
{
    TransferRecord r;
    r.id = q.value(QStringLiteral("id")).toLongLong();
    r.createdAt = parseSqliteDateTime(q.value(QStringLiteral("created_at")).toString());
    r.upload = q.value(QStringLiteral("upload")).toInt() != 0;
    r.isDir = q.value(QStringLiteral("is_dir")).toInt() != 0;
    r.name = q.value(QStringLiteral("name")).toString();
    r.localPath = q.value(QStringLiteral("local_path")).toString();
    r.remotePath = q.value(QStringLiteral("remote_path")).toString();
    r.size = q.value(QStringLiteral("size")).toLongLong();
    r.status = q.value(QStringLiteral("status")).toString();
    r.message = q.value(QStringLiteral("message")).toString();
    r.sessionId = q.value(QStringLiteral("session_id")).toLongLong();
    return r;
}

} // namespace

qint64 TransferRepository::insert(const TransferRecord &record)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral(
        "INSERT INTO transfer_history(created_at, upload, is_dir, name, local_path, remote_path, "
        "size, status, message, session_id) "
        "VALUES(datetime('now'), ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(record.upload ? 1 : 0);
    q.addBindValue(record.isDir ? 1 : 0);
    q.addBindValue(record.name);
    q.addBindValue(record.localPath);
    q.addBindValue(record.remotePath);
    q.addBindValue(record.size);
    q.addBindValue(record.status);
    q.addBindValue(record.message);
    q.addBindValue(record.sessionId);
    if (!q.exec())
        return 0;
    purgeOlderThanDays(7);
    return q.lastInsertId().toLongLong();
}

bool TransferRepository::remove(qint64 id)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral("DELETE FROM transfer_history WHERE id=?"));
    q.addBindValue(id);
    return q.exec();
}

bool TransferRepository::removeMany(const QVector<qint64> &ids)
{
    auto db = DatabaseManager::instance().database();
    db.transaction();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM transfer_history WHERE id=?"));
    for (qint64 id : ids) {
        q.addBindValue(id);
        if (!q.exec()) {
            db.rollback();
            return false;
        }
    }
    db.commit();
    return true;
}

QVector<TransferRecord> TransferRepository::recent() const
{
    QVector<TransferRecord> out;
    QSqlQuery q(DatabaseManager::instance().database());
    q.exec(QStringLiteral(
        "SELECT * FROM transfer_history WHERE created_at >= datetime('now', '-7 days') "
        "ORDER BY id DESC"));
    while (q.next())
        out.push_back(recordFromQuery(q));
    return out;
}

void TransferRepository::purgeOlderThanDays(int days)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral(
        "DELETE FROM transfer_history WHERE created_at < datetime('now', ?)"));
    q.addBindValue(QStringLiteral("-%1 days").arg(days));
    q.exec();
}
