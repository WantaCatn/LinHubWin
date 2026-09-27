#include "storage/commandrepository.h"
#include "storage/databasemanager.h"

#include <QSqlQuery>
#include <QString>
#include <QVariant>

QVector<QuickCommand> CommandRepository::all() const
{
    QVector<QuickCommand> out;
    QSqlQuery q(DatabaseManager::instance().database());
    q.exec(QStringLiteral("SELECT id, name, command, shortcut, send_to_all FROM quick_commands ORDER BY id"));
    while (q.next()) {
        QuickCommand c;
        c.id = q.value(0).toLongLong();
        c.name = q.value(1).toString();
        c.command = q.value(2).toString();
        c.shortcut = q.value(3).toString();
        c.sendToAll = q.value(4).toInt() != 0;
        out.push_back(c);
    }
    return out;
}

qint64 CommandRepository::insert(const QuickCommand &cmd)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral(
        "INSERT INTO quick_commands(name, command, shortcut, send_to_all) VALUES(?, ?, ?, ?)"));
    q.addBindValue(cmd.name);
    q.addBindValue(cmd.command);
    q.addBindValue(cmd.shortcut);
    q.addBindValue(cmd.sendToAll ? 1 : 0);
    if (!q.exec())
        return 0;
    return q.lastInsertId().toLongLong();
}

bool CommandRepository::update(const QuickCommand &cmd)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral(
        "UPDATE quick_commands SET name=?, command=?, shortcut=?, send_to_all=? WHERE id=?"));
    q.addBindValue(cmd.name);
    q.addBindValue(cmd.command);
    q.addBindValue(cmd.shortcut);
    q.addBindValue(cmd.sendToAll ? 1 : 0);
    q.addBindValue(cmd.id);
    return q.exec();
}

bool CommandRepository::remove(qint64 id)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral("DELETE FROM quick_commands WHERE id=?"));
    q.addBindValue(id);
    return q.exec();
}

bool CommandRepository::seedDefaultsIfEmpty()
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.exec(QStringLiteral("SELECT COUNT(*) FROM quick_commands"));
    if (q.next() && q.value(0).toInt() > 0)
        return true;

    const struct {
        const char *name;
        const char *cmd;
    } seeds[] = {
        {"查看系统信息", "uname -a; cat /etc/os-release 2>/dev/null | head"},
        {"磁盘空间", "df -hT"},
        {"内存与负载", "free -h; uptime"},
        {"当前用户与路径", "whoami; pwd; hostname"},
        {"监听端口", "ss -lntup 2>/dev/null || netstat -lntup"},
        {"最近登录", "last -n 20"},
    };
    for (const auto &s : seeds) {
        QuickCommand c;
        c.name = QString::fromUtf8(s.name);
        c.command = QString::fromUtf8(s.cmd);
        insert(c);
    }
    return true;
}
