#pragma once

#include <QString>

class AskPass
{
public:
    static int run();
    static void store(const QString &password, QString *outFile);
    static void clear(const QString &file);
};
