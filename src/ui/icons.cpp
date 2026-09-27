#include "ui/icons.h"

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

QPixmap paintIcon(const QString &name, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(1, 1, size - 2, size - 2);

    auto rounded = [&](const QColor &c) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(r, 5, 5);
    };
    auto pen = [&](const QColor &c, qreal w) {
        QPen pn(c, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pn);
        p.setBrush(Qt::NoBrush);
    };

    if (name == QLatin1String("new")) {
        rounded(QColor(QStringLiteral("#1a6dff")));
        pen(Qt::white, 2.2);
        p.drawLine(QPointF(size * 0.5, size * 0.28), QPointF(size * 0.5, size * 0.72));
        p.drawLine(QPointF(size * 0.28, size * 0.5), QPointF(size * 0.72, size * 0.5));
    } else if (name == QLatin1String("folder") || name == QLatin1String("sftp")) {
        p.setPen(Qt::NoPen);
        p.setBrush(name == QLatin1String("sftp") ? QColor(QStringLiteral("#8b5cf6"))
                                                 : QColor(QStringLiteral("#f5b942")));
        p.drawRoundedRect(QRectF(2, size * 0.32, size - 4, size * 0.52), 3, 3);
        p.drawRoundedRect(QRectF(2, size * 0.22, size * 0.42, size * 0.2), 2, 2);
        if (name == QLatin1String("sftp")) {
            pen(Qt::white, 1.8);
            p.drawLine(QPointF(size * 0.35, size * 0.62), QPointF(size * 0.5, size * 0.72));
            p.drawLine(QPointF(size * 0.5, size * 0.72), QPointF(size * 0.7, size * 0.48));
        }
    } else if (name == QLatin1String("ssh")) {
        rounded(QColor(QStringLiteral("#1a6dff")));
        p.setBrush(QColor(QStringLiteral("#7ee787")));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(size * 0.34, size * 0.5), size * 0.12, size * 0.12);
        pen(Qt::white, 1.7);
        p.drawArc(QRectF(size * 0.28, size * 0.28, size * 0.55, size * 0.44), 30 * 16, 120 * 16);
        p.drawArc(QRectF(size * 0.22, size * 0.22, size * 0.7, size * 0.56), 20 * 16, 80 * 16);
    } else if (name == QLatin1String("telnet")) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#0ea5e9")));
        p.drawEllipse(r);
        pen(Qt::white, 1.7);
        p.drawLine(QPointF(size * 0.5, size * 0.22), QPointF(size * 0.5, size * 0.38));
        p.drawLine(QPointF(size * 0.5, size * 0.62), QPointF(size * 0.5, size * 0.78));
        p.drawLine(QPointF(size * 0.22, size * 0.5), QPointF(size * 0.38, size * 0.5));
        p.drawLine(QPointF(size * 0.62, size * 0.5), QPointF(size * 0.78, size * 0.5));
        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(size * 0.5, size * 0.5), 2.4, 2.4);
    } else if (name == QLatin1String("local") || name == QLatin1String("terminal")) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#334155")));
        p.drawRoundedRect(QRectF(2, 3, size - 4, size * 0.58), 3, 3);
        p.setBrush(QColor(QStringLiteral("#0b1220")));
        p.drawRoundedRect(QRectF(4, 5, size - 8, size * 0.42), 2, 2);
        pen(QColor(QStringLiteral("#7ee787")), 1.4);
        p.drawLine(QPointF(6, size * 0.32), QPointF(size * 0.45, size * 0.32));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#64748b")));
        p.drawRoundedRect(QRectF(size * 0.28, size * 0.72, size * 0.44, 3), 1, 1);
    } else if (name == QLatin1String("serial")) {
        rounded(QColor(QStringLiteral("#f59e0b")));
        p.setBrush(QColor(QStringLiteral("#0b1220")));
        p.drawEllipse(QPointF(size * 0.35, size * 0.5), 2.2, 2.2);
        p.drawEllipse(QPointF(size * 0.65, size * 0.5), 2.2, 2.2);
    } else if (name == QLatin1String("reconnect") || name == QLatin1String("refresh")) {
        const QColor col = name == QLatin1String("refresh")
                               ? QColor(QStringLiteral("#38bdf8"))
                               : QColor(QStringLiteral("#22c55e"));
        pen(col, 2.0);
        p.drawArc(QRectF(4, 4, size - 8, size - 8), 50 * 16, 260 * 16);
        p.setBrush(col);
        p.setPen(Qt::NoPen);
        QPolygonF tip;
        tip << QPointF(size * 0.70, size * 0.22) << QPointF(size * 0.88, size * 0.38)
            << QPointF(size * 0.58, size * 0.40);
        p.drawPolygon(tip);
    } else if (name == QLatin1String("disconnect")) {
        const QColor col(QStringLiteral("#22c55e"));
        pen(col, 2.0);
        p.drawArc(QRectF(4, 4, size - 8, size - 8), 50 * 16, 260 * 16);
        p.setBrush(col);
        p.setPen(Qt::NoPen);
        QPolygonF tip;
        tip << QPointF(size * 0.70, size * 0.22) << QPointF(size * 0.88, size * 0.38)
            << QPointF(size * 0.58, size * 0.40);
        p.drawPolygon(tip);
        p.setPen(QPen(QColor(QStringLiteral("#0b1220")), 3.2, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(size * 0.22, size * 0.78), QPointF(size * 0.78, size * 0.22));
        p.setPen(QPen(QColor(QStringLiteral("#f87171")), 2.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(size * 0.22, size * 0.78), QPointF(size * 0.78, size * 0.22));
    } else if (name == QLatin1String("copy")) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#38bdf8")));
        p.drawRoundedRect(QRectF(7, 6, size * 0.52, size * 0.64), 2, 2);
        p.setBrush(QColor(QStringLiteral("#0ea5e9")));
        p.drawRoundedRect(QRectF(3, 3, size * 0.52, size * 0.6), 2, 2);
        pen(Qt::white, 1.4);
        p.drawLine(QPointF(6, size * 0.32), QPointF(size * 0.42, size * 0.32));
        p.drawLine(QPointF(6, size * 0.46), QPointF(size * 0.38, size * 0.46));
    } else if (name == QLatin1String("paste")) {
        rounded(QColor(QStringLiteral("#22c55e")));
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(size * 0.32, size * 0.22, size * 0.36, size * 0.16), 2, 2);
        p.drawRoundedRect(QRectF(size * 0.28, size * 0.32, size * 0.44, size * 0.46), 2, 2);
    } else if (name == QLatin1String("duplicate")) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#64748b")));
        p.drawRoundedRect(QRectF(6, 6, size * 0.58, size * 0.58), 2, 2);
        p.setBrush(QColor(QStringLiteral("#38bdf8")));
        p.drawRoundedRect(QRectF(3, 3, size * 0.58, size * 0.58), 2, 2);
    } else if (name == QLatin1String("find-prev") || name == QLatin1String("find-next")) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#334155")));
        p.drawEllipse(r);
        p.setBrush(Qt::white);
        QPolygonF tri;
        if (name == QLatin1String("find-prev"))
            tri << QPointF(size * 0.62, size * 0.3) << QPointF(size * 0.32, size * 0.5)
                << QPointF(size * 0.62, size * 0.7);
        else
            tri << QPointF(size * 0.38, size * 0.3) << QPointF(size * 0.68, size * 0.5)
                << QPointF(size * 0.38, size * 0.7);
        p.drawPolygon(tri);
    } else if (name == QLatin1String("find")) {
        pen(QColor(QStringLiteral("#e6edf3")), 2);
        p.drawEllipse(QPointF(size * 0.44, size * 0.44), size * 0.26, size * 0.26);
        p.setPen(QPen(QColor(QStringLiteral("#1a6dff")), 2.3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(size * 0.64, size * 0.64), QPointF(size * 0.84, size * 0.84));
    } else if (name == QLatin1String("zoom-in") || name == QLatin1String("zoom-out")) {
        p.setPen(Qt::NoPen);
        p.setBrush(name == QLatin1String("zoom-in") ? QColor(QStringLiteral("#1a6dff"))
                                                    : QColor(QStringLiteral("#334155")));
        p.drawEllipse(r);
        pen(Qt::white, 2);
        p.drawLine(QPointF(size * 0.32, size * 0.5), QPointF(size * 0.68, size * 0.5));
        if (name == QLatin1String("zoom-in"))
            p.drawLine(QPointF(size * 0.5, size * 0.32), QPointF(size * 0.5, size * 0.68));
    } else if (name == QLatin1String("settings")) {
        const QPointF c(size * 0.5, size * 0.5);
        const qreal outer = size * 0.40;
        const qreal inner = size * 0.27;
        const qreal hole = size * 0.12;
        const int teeth = 8;
        QPainterPath path;
        for (int i = 0; i < teeth; ++i) {
            const qreal a0 = (i - 0.18) * 2 * M_PI / teeth - M_PI / 2;
            const qreal a1 = (i + 0.18) * 2 * M_PI / teeth - M_PI / 2;
            const qreal b0 = (i + 0.32) * 2 * M_PI / teeth - M_PI / 2;
            const qreal b1 = (i + 0.68) * 2 * M_PI / teeth - M_PI / 2;
            auto pt = [&](qreal a, qreal rad) {
                return QPointF(c.x() + std::cos(a) * rad, c.y() + std::sin(a) * rad);
            };
            if (i == 0)
                path.moveTo(pt(a0, outer));
            else
                path.lineTo(pt(a0, outer));
            path.lineTo(pt(a1, outer));
            path.lineTo(pt(b0, inner));
            path.lineTo(pt(b1, inner));
        }
        path.closeSubpath();
        QPainterPath holePath;
        holePath.addEllipse(c, hole, hole);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#94a3b8")));
        p.drawPath(path.subtracted(holePath));
    } else if (name == QLatin1String("home")) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#1a6dff")));
        QPolygonF poly;
        poly << QPointF(size * 0.5, 3) << QPointF(size - 3, size * 0.46)
             << QPointF(size - 6, size * 0.46) << QPointF(size - 6, size - 3)
             << QPointF(6, size - 3) << QPointF(6, size * 0.46) << QPointF(3, size * 0.46);
        p.drawPolygon(poly);
    } else if (name == QLatin1String("star")) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#f5b942")));
        p.translate(size / 2.0, size / 2.0);
        QPolygonF star;
        for (int i = 0; i < 5; ++i) {
            const qreal a = -M_PI / 2 + i * 2 * M_PI / 5;
            const qreal b = a + M_PI / 5;
            star << QPointF(std::cos(a) * size * 0.38, std::sin(a) * size * 0.38);
            star << QPointF(std::cos(b) * size * 0.16, std::sin(b) * size * 0.16);
        }
        p.drawPolygon(star);
    } else if (name == QLatin1String("fullscreen")) {
        rounded(QColor(QStringLiteral("#0f172a")));
        pen(QColor(QStringLiteral("#1a6dff")), 1.6);
        p.drawRoundedRect(r.adjusted(2, 2, -2, -2), 3, 3);
        pen(QColor(QStringLiteral("#7ee787")), 1.4);
        p.drawLine(QPointF(7, 10), QPointF(size - 7, 10));
    } else if (name == QLatin1String("connect") || name == QLatin1String("send")) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#22c55e")));
        p.drawEllipse(r);
        p.setBrush(Qt::white);
        QPolygonF tri;
        tri << QPointF(size * 0.38, size * 0.3) << QPointF(size * 0.74, size * 0.5)
            << QPointF(size * 0.38, size * 0.7);
        p.drawPolygon(tri);
    } else if (name == QLatin1String("clear") || name == QLatin1String("close")) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#ef4444")));
        p.drawEllipse(r);
        pen(Qt::white, 2);
        p.drawLine(QPointF(size * 0.32, size * 0.32), QPointF(size * 0.68, size * 0.68));
        p.drawLine(QPointF(size * 0.68, size * 0.32), QPointF(size * 0.32, size * 0.68));
    } else if (name == QLatin1String("history")) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#6366f1")));
        p.drawEllipse(r);
        pen(Qt::white, 1.8);
        p.drawLine(QPointF(size * 0.5, size * 0.3), QPointF(size * 0.5, size * 0.52));
        p.drawLine(QPointF(size * 0.5, size * 0.52), QPointF(size * 0.68, size * 0.62));
    } else if (name == QLatin1String("broadcast") || name == QLatin1String("compose") || name == QLatin1String("quick")) {
        rounded(QColor(QStringLiteral("#1a6dff")));
        pen(Qt::white, 1.7);
        p.drawLine(QPointF(5, size * 0.38), QPointF(size - 5, size * 0.38));
        p.drawLine(QPointF(5, size * 0.52), QPointF(size - 8, size * 0.52));
        p.drawLine(QPointF(5, size * 0.66), QPointF(size - 11, size * 0.66));
    } else if (name == QLatin1String("download") || name == QLatin1String("upload")) {
        rounded(name == QLatin1String("download") ? QColor(QStringLiteral("#1a6dff"))
                                                 : QColor(QStringLiteral("#22c55e")));
        pen(Qt::white, 2);
        p.drawLine(QPointF(size * 0.5, size * 0.26), QPointF(size * 0.5, size * 0.7));
        if (name == QLatin1String("download")) {
            p.drawLine(QPointF(size * 0.34, size * 0.54), QPointF(size * 0.5, size * 0.7));
            p.drawLine(QPointF(size * 0.66, size * 0.54), QPointF(size * 0.5, size * 0.7));
        } else {
            p.drawLine(QPointF(size * 0.34, size * 0.42), QPointF(size * 0.5, size * 0.26));
            p.drawLine(QPointF(size * 0.66, size * 0.42), QPointF(size * 0.5, size * 0.26));
        }
        p.drawLine(QPointF(size * 0.3, size * 0.76), QPointF(size * 0.7, size * 0.76));
    } else if (name == QLatin1String("file")) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#e2e8f0")));
        p.drawRoundedRect(QRectF(5, 3, size - 10, size - 6), 2, 2);
        p.setBrush(QColor(QStringLiteral("#94a3b8")));
        p.drawPolygon(QPolygonF() << QPointF(size - 5, 3) << QPointF(size - 5, 9) << QPointF(size - 11, 3));
        pen(QColor(QStringLiteral("#64748b")), 1.2);
        p.drawLine(QPointF(8, size * 0.45), QPointF(size - 8, size * 0.45));
        p.drawLine(QPointF(8, size * 0.6), QPointF(size - 8, size * 0.6));
    } else if (name == QLatin1String("follow")) {
        rounded(QColor(QStringLiteral("#8b5cf6")));
        pen(Qt::white, 1.7);
        p.drawRoundedRect(QRectF(size * 0.22, size * 0.28, size * 0.38, size * 0.28), 2, 2);
        p.drawLine(QPointF(size * 0.55, size * 0.42), QPointF(size * 0.72, size * 0.58));
        p.drawRoundedRect(QRectF(size * 0.42, size * 0.5, size * 0.38, size * 0.28), 2, 2);
    } else if (name == QLatin1String("up")) {
        rounded(QColor(QStringLiteral("#f5b942")));
        pen(Qt::white, 2);
        p.drawLine(QPointF(size * 0.5, size * 0.7), QPointF(size * 0.5, size * 0.3));
        p.drawLine(QPointF(size * 0.32, size * 0.46), QPointF(size * 0.5, size * 0.3));
        p.drawLine(QPointF(size * 0.68, size * 0.46), QPointF(size * 0.5, size * 0.3));
    } else if (name == QLatin1String("split-h") || name == QLatin1String("split-v")
               || name == QLatin1String("split-quad") || name == QLatin1String("split-join")) {
        rounded(QColor(QStringLiteral("#334155")));
        pen(QColor(QStringLiteral("#93c5fd")), 1.5);
        const QRectF box = r.adjusted(3, 3, -3, -3);
        p.drawRoundedRect(box, 2, 2);
        if (name == QLatin1String("split-h")) {
            p.drawLine(QPointF(box.left() + 1, box.center().y()),
                       QPointF(box.right() - 1, box.center().y()));
        } else if (name == QLatin1String("split-v")) {
            p.drawLine(QPointF(box.center().x(), box.top() + 1),
                       QPointF(box.center().x(), box.bottom() - 1));
        } else if (name == QLatin1String("split-quad")) {
            p.drawLine(QPointF(box.left() + 1, box.center().y()),
                       QPointF(box.right() - 1, box.center().y()));
            p.drawLine(QPointF(box.center().x(), box.top() + 1),
                       QPointF(box.center().x(), box.bottom() - 1));
        }
    } else {
        rounded(QColor(QStringLiteral("#1a6dff")));
    }
    p.end();
    return pm;
}

} // namespace

QIcon AppIcons::get(const QString &name)
{
    QIcon svg(QStringLiteral(":/icons/%1.svg").arg(name));
    const QPixmap fromSvg = svg.pixmap(toolbarSize());
    if (!fromSvg.isNull() && !fromSvg.toImage().isNull()) {
        // SVG plugin may yield an empty/transparent pixmap
        const QImage img = fromSvg.toImage();
        bool hasInk = false;
        for (int y = 0; y < img.height() && !hasInk; ++y) {
            for (int x = 0; x < img.width(); ++x) {
                if (qAlpha(img.pixel(x, y)) > 20) {
                    hasInk = true;
                    break;
                }
            }
        }
        if (hasInk)
            return svg;
    }
    QIcon icon;
    icon.addPixmap(paintIcon(name, 16));
    icon.addPixmap(paintIcon(name, 22));
    icon.addPixmap(paintIcon(name, 32));
    return icon;
}

QIcon AppIcons::protocol(Protocol p)
{
    switch (p) {
    case Protocol::Ssh:    return get(QStringLiteral("ssh"));
    case Protocol::Telnet: return get(QStringLiteral("telnet"));
    case Protocol::Local:  return get(QStringLiteral("local"));
    case Protocol::Serial: return get(QStringLiteral("serial"));
    case Protocol::Sftp:   return get(QStringLiteral("sftp"));
    }
    return get(QStringLiteral("ssh"));
}
