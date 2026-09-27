#pragma once

#include <QByteArray>
#include <QString>

class Crypto
{
public:
    static QString encrypt(const QString &plain);
    static QString decrypt(const QString &encoded);
    static bool isBlank(const QString &encoded);
};
