#pragma once

#include "core/session.h"

#include <QByteArray>
#include <QString>

class SessionLogger
{
public:
    static QString createLogPath(const Session &session);
    static void writeHeader(const QString &path, const Session &session);
    static void append(const QString &path, const QByteArray &data);
};
