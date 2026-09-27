#pragma once

#include "core/session.h"

#include <QSqlDatabase>
#include <QString>

class DatabaseManager
{
public:
    static DatabaseManager &instance();

    bool open();
    void close();
    QSqlDatabase database() const;
    QString filePath() const;
    QString lastError() const;

private:
    DatabaseManager() = default;
    bool migrate(QSqlDatabase &db);

    QString m_path;
    QString m_lastError;
};
