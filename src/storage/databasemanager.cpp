#include "storage/databasemanager.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

DatabaseManager &DatabaseManager::instance()
{
    static DatabaseManager mgr;
    return mgr;
}

bool DatabaseManager::open()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    m_path = base + QLatin1String("/linhub.sqlite");

    if (QSqlDatabase::contains(QStringLiteral("linhub"))) {
        auto db = QSqlDatabase::database(QStringLiteral("linhub"));
        if (db.isOpen())
            return true;
    }

    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("linhub"));
    db.setDatabaseName(m_path);
    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }

    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    return migrate(db);
}

void DatabaseManager::close()
{
    if (QSqlDatabase::contains(QStringLiteral("linhub"))) {
        auto db = QSqlDatabase::database(QStringLiteral("linhub"));
        db.close();
    }
}

QSqlDatabase DatabaseManager::database() const
{
    return QSqlDatabase::database(QStringLiteral("linhub"));
}

QString DatabaseManager::filePath() const
{
    return m_path;
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

bool DatabaseManager::migrate(QSqlDatabase &db)
{
    QSqlQuery q(db);

    const char *ddl[] = {
        R"(CREATE TABLE IF NOT EXISTS meta (
            key TEXT PRIMARY KEY,
            value TEXT
        ))",
        R"(CREATE TABLE IF NOT EXISTS folders (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            parent_id INTEGER NOT NULL DEFAULT 0,
            name TEXT NOT NULL,
            sort_order INTEGER NOT NULL DEFAULT 0
        ))",
        R"(CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            folder_id INTEGER NOT NULL DEFAULT 0,
            name TEXT NOT NULL,
            protocol TEXT NOT NULL DEFAULT 'ssh',
            host TEXT,
            port INTEGER DEFAULT 22,
            username TEXT,
            auth_type TEXT DEFAULT 'password',
            password_enc TEXT,
            key_path TEXT,
            extra_args TEXT,
            encoding TEXT DEFAULT 'UTF-8',
            color_scheme TEXT DEFAULT 'Moba Dark',
            log_enabled INTEGER DEFAULT 1,
            log_path TEXT,
            notes TEXT,
            tags TEXT,
            jump_host TEXT,
            x11_forward INTEGER DEFAULT 0,
            keepalive INTEGER DEFAULT 60,
            proxy_command TEXT,
            startup_command TEXT,
            serial_device TEXT,
            serial_baud INTEGER DEFAULT 115200,
            created_at TEXT,
            updated_at TEXT,
            last_connected_at TEXT,
            connect_count INTEGER DEFAULT 0,
            favorite INTEGER DEFAULT 0,
            sort_order INTEGER NOT NULL DEFAULT 0
        ))",
        R"(CREATE TABLE IF NOT EXISTS connection_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id INTEGER,
            session_name TEXT,
            protocol TEXT,
            host TEXT,
            username TEXT,
            started_at TEXT,
            ended_at TEXT,
            duration_sec INTEGER DEFAULT 0,
            success INTEGER DEFAULT 0,
            error_message TEXT,
            log_file TEXT
        ))",
        R"(CREATE TABLE IF NOT EXISTS quick_commands (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            command TEXT NOT NULL,
            shortcut TEXT,
            send_to_all INTEGER DEFAULT 0
        ))",
        R"(CREATE INDEX IF NOT EXISTS idx_sessions_folder ON sessions(folder_id))",
        R"(CREATE INDEX IF NOT EXISTS idx_sessions_host ON sessions(host))",
        R"(CREATE INDEX IF NOT EXISTS idx_sessions_name ON sessions(name))",
        R"(CREATE INDEX IF NOT EXISTS idx_history_started ON connection_history(started_at))",
        R"(CREATE INDEX IF NOT EXISTS idx_history_session ON connection_history(session_id))",
        R"(CREATE TABLE IF NOT EXISTS transfer_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            created_at TEXT,
            upload INTEGER DEFAULT 0,
            is_dir INTEGER DEFAULT 0,
            name TEXT,
            local_path TEXT,
            remote_path TEXT,
            size INTEGER DEFAULT 0,
            status TEXT,
            message TEXT,
            session_id INTEGER DEFAULT 0
        ))",
        R"(CREATE INDEX IF NOT EXISTS idx_transfer_created ON transfer_history(created_at))"
    };

    for (const char *sql : ddl) {
        if (!q.exec(QString::fromUtf8(sql))) {
            m_lastError = q.lastError().text();
            return false;
        }
    }

    QSqlQuery cols(db);
    cols.exec(QStringLiteral("PRAGMA table_info(sessions)"));
    bool hasCompat = false;
    bool hasJumpChain = false;
    bool hasForwards = false;
    while (cols.next()) {
        const QString name = cols.value(1).toString();
        if (name == QLatin1String("ssh_compat"))
            hasCompat = true;
        if (name == QLatin1String("jump_chain"))
            hasJumpChain = true;
        if (name == QLatin1String("port_forwards"))
            hasForwards = true;
    }
    if (!hasCompat) {
        QSqlQuery alter(db);
        if (!alter.exec(QStringLiteral("ALTER TABLE sessions ADD COLUMN ssh_compat TEXT DEFAULT 'auto'"))) {
            m_lastError = alter.lastError().text();
            return false;
        }
    }
    if (!hasJumpChain) {
        QSqlQuery alter(db);
        if (!alter.exec(QStringLiteral("ALTER TABLE sessions ADD COLUMN jump_chain TEXT"))) {
            m_lastError = alter.lastError().text();
            return false;
        }
    }
    if (!hasForwards) {
        QSqlQuery alter(db);
        if (!alter.exec(QStringLiteral("ALTER TABLE sessions ADD COLUMN port_forwards TEXT"))) {
            m_lastError = alter.lastError().text();
            return false;
        }
    }

    QSqlQuery x11Meta(db);
    if (x11Meta.exec(QStringLiteral("SELECT value FROM meta WHERE key = 'x11_off_default'"))
        && !x11Meta.next()) {
        QSqlQuery upd(db);
        upd.exec(QStringLiteral("UPDATE sessions SET x11_forward = 0"));
        QSqlQuery ins(db);
        ins.exec(QStringLiteral("INSERT INTO meta(key, value) VALUES('x11_off_default', '1')"));
    }

    if (!q.exec(QStringLiteral("SELECT value FROM meta WHERE key = 'schema_version'"))) {
        m_lastError = q.lastError().text();
        return false;
    }
    if (!q.next()) {
        QSqlQuery ins(db);
        ins.prepare(QStringLiteral("INSERT INTO meta(key, value) VALUES('schema_version', '1')"));
        ins.exec();

        QSqlQuery seed(db);
        seed.prepare(QStringLiteral("INSERT INTO folders(parent_id, name, sort_order) VALUES(0, ?, 0)"));
        seed.addBindValue(QStringLiteral("默认分组"));
        seed.exec();
        const qint64 folderId = seed.lastInsertId().toLongLong();

        seed.prepare(QStringLiteral(
            "INSERT INTO sessions(folder_id, name, protocol, host, port, created_at, updated_at) "
            "VALUES(?, ?, 'local', '', 0, datetime('now'), datetime('now'))"));
        seed.addBindValue(folderId);
        seed.addBindValue(QStringLiteral("本地 Shell"));
        seed.exec();
    }
    return true;
}
