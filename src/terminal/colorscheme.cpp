#include "terminal/cell.h"

#include <initializer_list>

namespace {

quint32 rgb(int r, int g, int b)
{
    return (quint32(r) << 16) | (quint32(g) << 8) | quint32(b);
}

ColorScheme makeScheme(const QString &name,
                       quint32 fg, quint32 bg, quint32 cur,
                       std::initializer_list<quint32> ansi)
{
    ColorScheme s;
    s.name = name;
    s.foreground = fg;
    s.background = bg;
    s.cursor = cur;
    s.ansi = QVector<quint32>(ansi);
    return s;
}

QVector<ColorScheme> allSchemes()
{
    static const QVector<ColorScheme> schemes = {
        makeScheme(QStringLiteral("Moba Dark"),
                   rgb(210, 210, 210), rgb(0, 0, 0), rgb(0, 255, 0),
                   {rgb(0,0,0), rgb(255,85,85), rgb(80,250,123), rgb(241,250,140),
                    rgb(139,233,253), rgb(255,121,198), rgb(80,250,250), rgb(248,248,242),
                    rgb(98,114,164), rgb(255,110,110), rgb(90,255,150), rgb(255,255,120),
                    rgb(120,200,255), rgb(255,146,230), rgb(100,255,255), rgb(255,255,255)}),
        makeScheme(QStringLiteral("Xshell Dark"),
                   rgb(200, 200, 200), rgb(0, 0, 0), rgb(0, 255, 0),
                   {rgb(0,0,0), rgb(187,0,0), rgb(0,187,0), rgb(187,187,0),
                    rgb(0,0,187), rgb(187,0,187), rgb(0,187,187), rgb(187,187,187),
                    rgb(85,85,85), rgb(255,85,85), rgb(85,255,85), rgb(255,255,85),
                    rgb(85,85,255), rgb(255,85,255), rgb(85,255,255), rgb(255,255,255)}),
        makeScheme(QStringLiteral("Solarized Dark"),
                   rgb(131,148,150), rgb(0,43,54), rgb(38,139,210),
                   {rgb(7,54,66), rgb(220,50,47), rgb(133,153,0), rgb(181,137,0),
                    rgb(38,139,210), rgb(211,54,130), rgb(42,161,152), rgb(238,232,213),
                    rgb(0,43,54), rgb(203,75,22), rgb(88,110,117), rgb(101,123,131),
                    rgb(131,148,150), rgb(108,113,196), rgb(147,161,161), rgb(253,246,227)}),
        makeScheme(QStringLiteral("PowerShell"),
                   rgb(238,237,240), rgb(1,36,86), rgb(238,237,240),
                   {rgb(0,0,0), rgb(228,79,79), rgb(30,203,118), rgb(210,153,34),
                    rgb(59,120,255), rgb(180,70,180), rgb(41,184,219), rgb(238,237,240),
                    rgb(80,80,80), rgb(255,120,120), rgb(80,255,160), rgb(255,220,80),
                    rgb(100,160,255), rgb(220,120,220), rgb(80,220,255), rgb(255,255,255)}),
        makeScheme(QStringLiteral("Ubuntu"),
                   rgb(238,238,238), rgb(48,10,36), rgb(255,255,255),
                   {rgb(46,52,54), rgb(204,0,0), rgb(78,154,6), rgb(196,160,0),
                    rgb(52,101,164), rgb(117,80,123), rgb(6,152,154), rgb(211,215,207),
                    rgb(85,87,83), rgb(239,41,41), rgb(138,226,52), rgb(252,233,79),
                    rgb(114,159,207), rgb(173,127,168), rgb(52,226,226), rgb(238,238,238)}),
        makeScheme(QStringLiteral("Kylin Blue"),
                   rgb(230,237,243), rgb(11,18,32), rgb(26,109,255),
                   {rgb(15,23,42), rgb(239,68,68), rgb(34,197,94), rgb(250,204,21),
                    rgb(59,130,246), rgb(168,85,247), rgb(34,211,238), rgb(203,213,225),
                    rgb(71,85,105), rgb(248,113,113), rgb(74,222,128), rgb(253,224,71),
                    rgb(96,165,250), rgb(192,132,252), rgb(103,232,249), rgb(248,250,252)}),
    };
    return schemes;
}

} // namespace

ColorScheme ColorScheme::byName(const QString &name)
{
    const auto schemes = allSchemes();
    for (const auto &s : schemes) {
        if (s.name.compare(name, Qt::CaseInsensitive) == 0)
            return s;
    }
    return schemes.first();
}

QStringList ColorScheme::names()
{
    QStringList n;
    for (const auto &s : allSchemes())
        n.push_back(s.name);
    return n;
}

quint32 ansi256(int index)
{
    index = qBound(0, index, 255);
    if (index < 16) {
        const auto s = ColorScheme::byName(QStringLiteral("Moba Dark"));
        if (index < s.ansi.size())
            return s.ansi[index];
    }
    if (index < 232) {
        const int i = index - 16;
        const int r = i / 36;
        const int g = (i / 6) % 6;
        const int b = i % 6;
        auto level = [](int n) { return n == 0 ? 0 : 55 + n * 40; };
        return (quint32(level(r)) << 16) | (quint32(level(g)) << 8) | quint32(level(b));
    }
    const int gray = 8 + (index - 232) * 10;
    return (quint32(gray) << 16) | (quint32(gray) << 8) | quint32(gray);
}
