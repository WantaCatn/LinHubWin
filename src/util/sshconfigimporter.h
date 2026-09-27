#pragma once

#include "core/session.h"

#include <QVector>

class SshConfigImporter
{
public:
    static QVector<Session> fromFile(const QString &path);
    static QVector<Session> fromDefault();
};
