#pragma once

#include "core/session.h"

#include <QVector>

class CommandRepository
{
public:
    QVector<QuickCommand> all() const;
    qint64 insert(const QuickCommand &cmd);
    bool update(const QuickCommand &cmd);
    bool remove(qint64 id);
    bool seedDefaultsIfEmpty();
};
