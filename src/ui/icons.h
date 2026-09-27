#pragma once

#include "core/session.h"

#include <QIcon>
#include <QSize>
#include <QString>

class AppIcons
{
public:
    static QIcon get(const QString &name);
    static QIcon protocol(Protocol p);
    static QSize toolbarSize() { return QSize(22, 22); }
};
