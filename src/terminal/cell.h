#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

struct Cell {
    char32_t ch = U' ';
    quint32 fg = 0xDCDCDC;
    quint32 bg = 0x0C0C0C;
    quint8 flags = 0;

    enum Flag : quint8 {
        Bold      = 1 << 0,
        Underline = 1 << 1,
        Inverse   = 1 << 2,
        Italic    = 1 << 3,
        Blink     = 1 << 4,
        Wide      = 1 << 5,
        WideCont  = 1 << 6
    };

    bool operator==(const Cell &o) const
    {
        return ch == o.ch && fg == o.fg && bg == o.bg && flags == o.flags;
    }
};

struct ColorScheme {
    QString name;
    quint32 foreground = 0xDCDCDC;
    quint32 background = 0x0C0C0C;
    quint32 cursor = 0x1A6DFF;
    quint32 selection = 0x264F78;
    QVector<quint32> ansi; // 16 colors

    static ColorScheme byName(const QString &name);
    static QStringList names();
};

quint32 ansi256(int index);
