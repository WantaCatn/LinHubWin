#pragma once

#include "core/session.h"

#include <QVector>

class SessionRepository
{
public:
    QVector<Folder> allFolders() const;
    qint64 insertFolder(const Folder &folder);
    bool updateFolder(const Folder &folder);
    bool removeFolder(qint64 id);

    QVector<Session> allSessions() const;
    QVector<Session> search(const QString &keyword) const;
    Session byId(qint64 id) const;
    qint64 insert(const Session &session);
    bool update(const Session &session);
    bool remove(qint64 id);
    bool touchConnected(qint64 id);
    bool setFavorite(qint64 id, bool favorite);
    bool exportToFile(const QString &path, bool includePasswords, QString *err = nullptr) const;
    int importFromFile(const QString &path, QString *err = nullptr);
};
