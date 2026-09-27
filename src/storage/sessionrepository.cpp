#include "storage/sessionrepository.h"
#include "storage/databasemanager.h"
#include "util/crypto.h"
#include "util/qtcompat.h"

#include <QFile>
#include <QIODevice>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

QString tagsJoin(const QStringList &tags)
{
    return tags.join(QLatin1Char(','));
}

QStringList tagsSplit(const QString &s)
{
    QStringList out;
    const auto parts = splitSkipEmpty(s, QLatin1Char(','));
    for (QString p : parts) {
        p = p.trimmed();
        if (!p.isEmpty())
            out.push_back(p);
    }
    return out;
}

Session sessionFromQuery(const QSqlQuery &q)
{
    Session s;
    s.id = q.value(QStringLiteral("id")).toLongLong();
    s.folderId = q.value(QStringLiteral("folder_id")).toLongLong();
    s.name = q.value(QStringLiteral("name")).toString();
    s.protocol = protocolFromString(q.value(QStringLiteral("protocol")).toString());
    s.host = q.value(QStringLiteral("host")).toString();
    s.port = q.value(QStringLiteral("port")).toInt();
    s.username = q.value(QStringLiteral("username")).toString();
    s.authType = authTypeFromString(q.value(QStringLiteral("auth_type")).toString());
    s.passwordEnc = q.value(QStringLiteral("password_enc")).toString();
    s.keyPath = q.value(QStringLiteral("key_path")).toString();
    s.extraArgs = q.value(QStringLiteral("extra_args")).toString();
    s.encoding = q.value(QStringLiteral("encoding")).toString();
    s.colorScheme = q.value(QStringLiteral("color_scheme")).toString();
    s.logEnabled = q.value(QStringLiteral("log_enabled")).toInt() != 0;
    s.logPath = q.value(QStringLiteral("log_path")).toString();
    s.notes = q.value(QStringLiteral("notes")).toString();
    s.tags = tagsSplit(q.value(QStringLiteral("tags")).toString());
    s.jumpHost = q.value(QStringLiteral("jump_host")).toString();
    s.jumps = jumpChainFromJson(q.value(QStringLiteral("jump_chain")).toString());
    s.forwards = portForwardsFromJson(q.value(QStringLiteral("port_forwards")).toString());
    s.x11Forward = q.value(QStringLiteral("x11_forward")).toInt() != 0;
    s.keepAlive = q.value(QStringLiteral("keepalive")).toInt();
    s.proxyCommand = q.value(QStringLiteral("proxy_command")).toString();
    s.startupCommand = q.value(QStringLiteral("startup_command")).toString();
    s.serialDevice = q.value(QStringLiteral("serial_device")).toString();
    s.serialBaud = q.value(QStringLiteral("serial_baud")).toInt();
    s.sshCompat = q.value(QStringLiteral("ssh_compat")).toString();
    if (s.sshCompat.isEmpty())
        s.sshCompat = QStringLiteral("auto");
    s.createdAt = parseSqliteDateTime(q.value(QStringLiteral("created_at")).toString());
    s.updatedAt = parseSqliteDateTime(q.value(QStringLiteral("updated_at")).toString());
    s.lastConnectedAt = parseSqliteDateTime(q.value(QStringLiteral("last_connected_at")).toString());
    s.connectCount = q.value(QStringLiteral("connect_count")).toInt();
    s.favorite = q.value(QStringLiteral("favorite")).toInt() != 0;
    s.sortOrder = q.value(QStringLiteral("sort_order")).toInt();
    return s;
}

void bindSession(QSqlQuery &q, const Session &s, bool includeId)
{
    q.bindValue(QStringLiteral(":folder_id"), s.folderId);
    q.bindValue(QStringLiteral(":name"), s.name);
    q.bindValue(QStringLiteral(":protocol"), protocolToString(s.protocol));
    q.bindValue(QStringLiteral(":host"), s.host);
    q.bindValue(QStringLiteral(":port"), s.port);
    q.bindValue(QStringLiteral(":username"), s.username);
    q.bindValue(QStringLiteral(":auth_type"), authTypeToString(s.authType));
    q.bindValue(QStringLiteral(":password_enc"), s.passwordEnc);
    q.bindValue(QStringLiteral(":key_path"), s.keyPath);
    q.bindValue(QStringLiteral(":extra_args"), s.extraArgs);
    q.bindValue(QStringLiteral(":encoding"), s.encoding);
    q.bindValue(QStringLiteral(":color_scheme"), s.colorScheme);
    q.bindValue(QStringLiteral(":log_enabled"), s.logEnabled ? 1 : 0);
    q.bindValue(QStringLiteral(":log_path"), s.logPath);
    q.bindValue(QStringLiteral(":notes"), s.notes);
    q.bindValue(QStringLiteral(":tags"), tagsJoin(s.tags));
    q.bindValue(QStringLiteral(":jump_host"), s.jumpHost);
    q.bindValue(QStringLiteral(":jump_chain"), jumpChainToJson(s.jumps));
    q.bindValue(QStringLiteral(":port_forwards"), portForwardsToJson(s.forwards));
    q.bindValue(QStringLiteral(":x11_forward"), s.x11Forward ? 1 : 0);
    q.bindValue(QStringLiteral(":keepalive"), s.keepAlive);
    q.bindValue(QStringLiteral(":proxy_command"), s.proxyCommand);
    q.bindValue(QStringLiteral(":startup_command"), s.startupCommand);
    q.bindValue(QStringLiteral(":serial_device"), s.serialDevice);
    q.bindValue(QStringLiteral(":serial_baud"), s.serialBaud);
    q.bindValue(QStringLiteral(":ssh_compat"), s.sshCompat.isEmpty() ? QStringLiteral("auto") : s.sshCompat);
    q.bindValue(QStringLiteral(":favorite"), s.favorite ? 1 : 0);
    q.bindValue(QStringLiteral(":sort_order"), s.sortOrder);
    if (includeId)
        q.bindValue(QStringLiteral(":id"), s.id);
}

const char *kSessionColumns =
    "id, folder_id, name, protocol, host, port, username, auth_type, password_enc, key_path, "
    "extra_args, encoding, color_scheme, log_enabled, log_path, notes, tags, jump_host, jump_chain, "
    "port_forwards, x11_forward, keepalive, proxy_command, startup_command, serial_device, serial_baud, "
    "ssh_compat, created_at, updated_at, last_connected_at, connect_count, favorite, sort_order";

} // namespace

QVector<Folder> SessionRepository::allFolders() const
{
    QVector<Folder> out;
    QSqlQuery q(DatabaseManager::instance().database());
    q.exec(QStringLiteral("SELECT id, parent_id, name, sort_order FROM folders ORDER BY sort_order, name"));
    while (q.next()) {
        Folder f;
        f.id = q.value(0).toLongLong();
        f.parentId = q.value(1).toLongLong();
        f.name = q.value(2).toString();
        f.sortOrder = q.value(3).toInt();
        out.push_back(f);
    }
    return out;
}

qint64 SessionRepository::insertFolder(const Folder &folder)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral("INSERT INTO folders(parent_id, name, sort_order) VALUES(?, ?, ?)"));
    q.addBindValue(folder.parentId);
    q.addBindValue(folder.name);
    q.addBindValue(folder.sortOrder);
    if (!q.exec())
        return 0;
    return q.lastInsertId().toLongLong();
}

bool SessionRepository::updateFolder(const Folder &folder)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral("UPDATE folders SET parent_id=?, name=?, sort_order=? WHERE id=?"));
    q.addBindValue(folder.parentId);
    q.addBindValue(folder.name);
    q.addBindValue(folder.sortOrder);
    q.addBindValue(folder.id);
    return q.exec();
}

bool SessionRepository::removeFolder(qint64 id)
{
    auto db = DatabaseManager::instance().database();
    db.transaction();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE sessions SET folder_id=0 WHERE folder_id=?"));
    q.addBindValue(id);
    q.exec();
    q.prepare(QStringLiteral("UPDATE folders SET parent_id=0 WHERE parent_id=?"));
    q.addBindValue(id);
    q.exec();
    q.prepare(QStringLiteral("DELETE FROM folders WHERE id=?"));
    q.addBindValue(id);
    const bool ok = q.exec();
    if (ok)
        db.commit();
    else
        db.rollback();
    return ok;
}

QVector<Session> SessionRepository::allSessions() const
{
    QVector<Session> out;
    QSqlQuery q(DatabaseManager::instance().database());
    q.exec(QStringLiteral("SELECT %1 FROM sessions ORDER BY favorite DESC, sort_order, name")
               .arg(QString::fromUtf8(kSessionColumns)));
    while (q.next())
        out.push_back(sessionFromQuery(q));
    return out;
}

QVector<Session> SessionRepository::search(const QString &keyword) const
{
    QVector<Session> out;
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral(
        "SELECT %1 FROM sessions WHERE name LIKE :k OR host LIKE :k OR username LIKE :k "
        "OR notes LIKE :k OR tags LIKE :k ORDER BY favorite DESC, name")
                  .arg(QString::fromUtf8(kSessionColumns)));
    q.bindValue(QStringLiteral(":k"), QStringLiteral("%") + keyword + QStringLiteral("%"));
    q.exec();
    while (q.next())
        out.push_back(sessionFromQuery(q));
    return out;
}

Session SessionRepository::byId(qint64 id) const
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral("SELECT %1 FROM sessions WHERE id=?")
                  .arg(QString::fromUtf8(kSessionColumns)));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return sessionFromQuery(q);
    return {};
}

qint64 SessionRepository::insert(const Session &session)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral(
        "INSERT INTO sessions(folder_id, name, protocol, host, port, username, auth_type, "
        "password_enc, key_path, extra_args, encoding, color_scheme, log_enabled, log_path, "
        "notes, tags, jump_host, jump_chain, port_forwards, x11_forward, keepalive, proxy_command, startup_command, "
        "serial_device, serial_baud, ssh_compat, created_at, updated_at, favorite, sort_order) "
        "VALUES(:folder_id, :name, :protocol, :host, :port, :username, :auth_type, "
        ":password_enc, :key_path, :extra_args, :encoding, :color_scheme, :log_enabled, :log_path, "
        ":notes, :tags, :jump_host, :jump_chain, :port_forwards, :x11_forward, :keepalive, :proxy_command, :startup_command, "
        ":serial_device, :serial_baud, :ssh_compat, datetime('now'), datetime('now'), :favorite, :sort_order)"));
    bindSession(q, session, false);
    if (!q.exec())
        return 0;
    return q.lastInsertId().toLongLong();
}

bool SessionRepository::update(const Session &session)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral(
        "UPDATE sessions SET folder_id=:folder_id, name=:name, protocol=:protocol, host=:host, "
        "port=:port, username=:username, auth_type=:auth_type, password_enc=:password_enc, "
        "key_path=:key_path, extra_args=:extra_args, encoding=:encoding, color_scheme=:color_scheme, "
        "log_enabled=:log_enabled, log_path=:log_path, notes=:notes, tags=:tags, jump_host=:jump_host, "
        "jump_chain=:jump_chain, port_forwards=:port_forwards, "
        "x11_forward=:x11_forward, keepalive=:keepalive, proxy_command=:proxy_command, "
        "startup_command=:startup_command, serial_device=:serial_device, serial_baud=:serial_baud, "
        "ssh_compat=:ssh_compat, updated_at=datetime('now'), favorite=:favorite, sort_order=:sort_order WHERE id=:id"));
    bindSession(q, session, true);
    return q.exec();
}

bool SessionRepository::remove(qint64 id)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral("DELETE FROM sessions WHERE id=?"));
    q.addBindValue(id);
    return q.exec();
}

bool SessionRepository::touchConnected(qint64 id)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral(
        "UPDATE sessions SET last_connected_at=datetime('now'), connect_count=connect_count+1 WHERE id=?"));
    q.addBindValue(id);
    return q.exec();
}

bool SessionRepository::setFavorite(qint64 id, bool favorite)
{
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QStringLiteral("UPDATE sessions SET favorite=? WHERE id=?"));
    q.addBindValue(favorite ? 1 : 0);
    q.addBindValue(id);
    return q.exec();
}

namespace {

QJsonObject hopToJson(const JumpHop &h, bool includePasswords)
{
    QJsonObject o;
    o.insert(QStringLiteral("host"), h.host);
    o.insert(QStringLiteral("port"), h.port);
    o.insert(QStringLiteral("username"), h.username);
    if (includePasswords && !h.passwordEnc.isEmpty())
        o.insert(QStringLiteral("password"), Crypto::decrypt(h.passwordEnc));
    return o;
}

JumpHop hopFromJson(const QJsonObject &o)
{
    JumpHop h;
    h.host = o.value(QStringLiteral("host")).toString();
    h.port = o.value(QStringLiteral("port")).toInt(22);
    h.username = o.value(QStringLiteral("username")).toString();
    const QString pw = o.value(QStringLiteral("password")).toString();
    if (!pw.isEmpty())
        h.passwordEnc = Crypto::encrypt(pw);
    return h;
}

QJsonObject sessionToExport(const Session &s, bool includePasswords)
{
    QJsonObject o;
    o.insert(QStringLiteral("folderId"), s.folderId);
    o.insert(QStringLiteral("name"), s.name);
    o.insert(QStringLiteral("protocol"), protocolToString(s.protocol));
    o.insert(QStringLiteral("host"), s.host);
    o.insert(QStringLiteral("port"), s.port);
    o.insert(QStringLiteral("username"), s.username);
    o.insert(QStringLiteral("authType"), authTypeToString(s.authType));
    if (includePasswords && !s.passwordEnc.isEmpty())
        o.insert(QStringLiteral("password"), Crypto::decrypt(s.passwordEnc));
    o.insert(QStringLiteral("keyPath"), s.keyPath);
    o.insert(QStringLiteral("extraArgs"), s.extraArgs);
    o.insert(QStringLiteral("encoding"), s.encoding);
    o.insert(QStringLiteral("colorScheme"), s.colorScheme);
    o.insert(QStringLiteral("logEnabled"), s.logEnabled);
    o.insert(QStringLiteral("notes"), s.notes);
    o.insert(QStringLiteral("tags"), s.tags.join(QLatin1Char(',')));
    o.insert(QStringLiteral("jumpHost"), s.jumpHost);
    o.insert(QStringLiteral("x11Forward"), s.x11Forward);
    o.insert(QStringLiteral("keepAlive"), s.keepAlive);
    o.insert(QStringLiteral("proxyCommand"), s.proxyCommand);
    o.insert(QStringLiteral("startupCommand"), s.startupCommand);
    o.insert(QStringLiteral("serialDevice"), s.serialDevice);
    o.insert(QStringLiteral("serialBaud"), s.serialBaud);
    o.insert(QStringLiteral("sshCompat"), s.sshCompat);
    o.insert(QStringLiteral("favorite"), s.favorite);
    QJsonArray jumps;
    for (const JumpHop &h : s.jumps)
        jumps.append(hopToJson(h, includePasswords));
    o.insert(QStringLiteral("jumps"), jumps);
    QJsonArray fw;
    for (const PortForward &f : s.forwards) {
        QJsonObject fo;
        fo.insert(QStringLiteral("type"), f.type);
        fo.insert(QStringLiteral("bindHost"), f.bindHost);
        fo.insert(QStringLiteral("bindPort"), f.bindPort);
        fo.insert(QStringLiteral("destHost"), f.destHost);
        fo.insert(QStringLiteral("destPort"), f.destPort);
        fw.append(fo);
    }
    o.insert(QStringLiteral("forwards"), fw);
    return o;
}

Session sessionFromExport(const QJsonObject &o)
{
    Session s;
    s.folderId = o.value(QStringLiteral("folderId")).toVariant().toLongLong();
    s.name = o.value(QStringLiteral("name")).toString();
    s.protocol = protocolFromString(o.value(QStringLiteral("protocol")).toString());
    s.host = o.value(QStringLiteral("host")).toString();
    s.port = o.value(QStringLiteral("port")).toInt(22);
    s.username = o.value(QStringLiteral("username")).toString();
    s.authType = authTypeFromString(o.value(QStringLiteral("authType")).toString());
    const QString pw = o.value(QStringLiteral("password")).toString();
    if (!pw.isEmpty())
        s.passwordEnc = Crypto::encrypt(pw);
    s.keyPath = o.value(QStringLiteral("keyPath")).toString();
    s.extraArgs = o.value(QStringLiteral("extraArgs")).toString();
    s.encoding = o.value(QStringLiteral("encoding")).toString(QStringLiteral("UTF-8"));
    s.colorScheme = o.value(QStringLiteral("colorScheme")).toString(QStringLiteral("Moba Dark"));
    s.logEnabled = o.value(QStringLiteral("logEnabled")).toBool(true);
    s.notes = o.value(QStringLiteral("notes")).toString();
    s.tags = tagsSplit(o.value(QStringLiteral("tags")).toString());
    s.jumpHost = o.value(QStringLiteral("jumpHost")).toString();
    s.x11Forward = o.value(QStringLiteral("x11Forward")).toBool(false);
    s.keepAlive = o.value(QStringLiteral("keepAlive")).toInt(60);
    s.proxyCommand = o.value(QStringLiteral("proxyCommand")).toString();
    s.startupCommand = o.value(QStringLiteral("startupCommand")).toString();
    s.serialDevice = o.value(QStringLiteral("serialDevice")).toString();
    s.serialBaud = o.value(QStringLiteral("serialBaud")).toInt(115200);
    s.sshCompat = o.value(QStringLiteral("sshCompat")).toString(QStringLiteral("auto"));
    s.favorite = o.value(QStringLiteral("favorite")).toBool(false);
    for (const QJsonValue &v : o.value(QStringLiteral("jumps")).toArray())
        s.jumps.append(hopFromJson(v.toObject()));
    s.forwards = portForwardsFromJson(QString::fromUtf8(
        QJsonDocument(o.value(QStringLiteral("forwards")).toArray()).toJson(QJsonDocument::Compact)));
    if (s.name.isEmpty())
        s.name = s.displayTitle();
    return s;
}

} // namespace

bool SessionRepository::exportToFile(const QString &path, bool includePasswords, QString *err) const
{
    QJsonObject root;
    root.insert(QStringLiteral("linhub"), QStringLiteral("0.2.0"));
    QJsonArray folders;
    for (const Folder &f : allFolders()) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), f.id);
        o.insert(QStringLiteral("parentId"), f.parentId);
        o.insert(QStringLiteral("name"), f.name);
        o.insert(QStringLiteral("sortOrder"), f.sortOrder);
        folders.append(o);
    }
    root.insert(QStringLiteral("folders"), folders);
    QJsonArray sessions;
    for (const Session &s : allSessions())
        sessions.append(sessionToExport(s, includePasswords));
    root.insert(QStringLiteral("sessions"), sessions);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err)
            *err = QStringLiteral("无法写入文件");
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

int SessionRepository::importFromFile(const QString &path, QString *err)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (err)
            *err = QStringLiteral("无法读取文件");
        return -1;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        if (err)
            *err = QStringLiteral("不是有效的 LinHub 会话导出文件");
        return -1;
    }
    const QJsonObject root = doc.object();
    QHash<qint64, qint64> folderMap;
    folderMap.insert(0, 0);
    for (const QJsonValue &v : root.value(QStringLiteral("folders")).toArray()) {
        const QJsonObject o = v.toObject();
        Folder f;
        f.parentId = folderMap.value(o.value(QStringLiteral("parentId")).toVariant().toLongLong(), 0);
        f.name = o.value(QStringLiteral("name")).toString();
        f.sortOrder = o.value(QStringLiteral("sortOrder")).toInt();
        if (f.name.trimmed().isEmpty())
            continue;
        const qint64 nid = insertFolder(f);
        if (nid)
            folderMap.insert(o.value(QStringLiteral("id")).toVariant().toLongLong(), nid);
    }
    int n = 0;
    for (const QJsonValue &v : root.value(QStringLiteral("sessions")).toArray()) {
        Session s = sessionFromExport(v.toObject());
        if (s.name.trimmed().isEmpty() && s.host.trimmed().isEmpty() && s.protocol != Protocol::Local)
            continue;
        s.id = 0;
        s.folderId = folderMap.value(s.folderId, 0);
        if (insert(s))
            ++n;
    }
    return n;
}
