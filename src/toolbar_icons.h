#pragma once

// Monochrome line icons for the main toolbar, drawn with QPainter so they follow the theme's
// text colour and stay crisp at any DPI (no SVG module needed). Each icon is designed on a
// 24 x 24 grid with a 2 px stroke; makeToolbarIcon() renders it at 1x and 2x for the given
// logical size and adds a faded variant for the disabled state.

#include <QtCore/QString>
#include <QtCore/qmath.h>

#include <cmath>
#include <QtGui/QColor>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>

namespace mycel {

namespace detail {

inline void drawArrowHead(QPainter& p, QPointF tip, qreal angleDeg, qreal size)
{
    const qreal a = qDegreesToRadians(angleDeg);
    const QPointF left(tip.x() - size * std::cos(a - M_PI / 5), tip.y() - size * std::sin(a - M_PI / 5));
    const QPointF right(tip.x() - size * std::cos(a + M_PI / 5), tip.y() - size * std::sin(a + M_PI / 5));
    p.drawLine(tip, left);
    p.drawLine(tip, right);
}

// Draws one icon on the 24 x 24 grid. Unknown names draw an empty square so a typo is visible.
inline void drawToolbarIcon(QPainter& p, const QString& name)
{
    if (name == QLatin1String("open")) {
        // Folder with a lifted flap.
        QPainterPath path;
        path.moveTo(3, 7);
        path.lineTo(9, 7);
        path.lineTo(11, 9);
        path.lineTo(21, 9);
        path.lineTo(21, 19);
        path.lineTo(3, 19);
        path.closeSubpath();
        p.drawPath(path);
        p.drawLine(QPointF(3, 12), QPointF(21, 12));
    } else if (name == QLatin1String("search")) {
        p.drawEllipse(QPointF(10.5, 10.5), 6, 6);
        p.drawLine(QPointF(15, 15), QPointF(21, 21));
    } else if (name == QLatin1String("undo") || name == QLatin1String("redo")) {
        const bool redo = name == QLatin1String("redo");
        p.save();
        if (redo) {
            p.translate(24, 0);
            p.scale(-1, 1);
        }
        QPainterPath path;
        path.moveTo(6, 9);
        path.lineTo(16, 9);
        path.arcTo(QRectF(12, 9, 8, 8), 90, -180);
        path.lineTo(9, 17);
        p.drawPath(path);
        drawArrowHead(p, QPointF(5, 9), 180, 4.5);
        p.restore();
    } else if (name == QLatin1String("new-file")) {
        QPainterPath path;
        path.moveTo(6, 3);
        path.lineTo(14, 3);
        path.lineTo(19, 8);
        path.lineTo(19, 21);
        path.lineTo(6, 21);
        path.closeSubpath();
        p.drawPath(path);
        p.drawLine(QPointF(14, 3), QPointF(14, 8));
        p.drawLine(QPointF(14, 8), QPointF(19, 8));
        p.drawLine(QPointF(12.5, 11), QPointF(12.5, 18));
        p.drawLine(QPointF(9, 14.5), QPointF(16, 14.5));
    } else if (name == QLatin1String("new-folder")) {
        QPainterPath path;
        path.moveTo(3, 6);
        path.lineTo(9, 6);
        path.lineTo(11, 8);
        path.lineTo(21, 8);
        path.lineTo(21, 20);
        path.lineTo(3, 20);
        path.closeSubpath();
        p.drawPath(path);
        p.drawLine(QPointF(12, 11), QPointF(12, 17));
        p.drawLine(QPointF(9, 14), QPointF(15, 14));
    } else if (name == QLatin1String("rename")) {
        // Pencil over a baseline.
        p.save();
        p.translate(12, 11);
        p.rotate(45);
        p.drawRect(QRectF(-2, -8, 4, 12));
        p.drawLine(QPointF(-2, 4), QPointF(0, 7));
        p.drawLine(QPointF(0, 7), QPointF(2, 4));
        p.drawLine(QPointF(-2, -5), QPointF(2, -5));
        p.restore();
        p.drawLine(QPointF(4, 21), QPointF(20, 21));
    } else if (name == QLatin1String("refresh")) {
        QPainterPath path;
        path.arcMoveTo(QRectF(5, 5, 14, 14), 50);
        path.arcTo(QRectF(5, 5, 14, 14), 50, 290);
        p.drawPath(path);
        drawArrowHead(p, QPointF(19.6, 7.5), 20, 4.5);
    } else if (name == QLatin1String("fit")) {
        // Corner brackets with a small centre rectangle.
        for (int sx : {-1, 1}) {
            for (int sy : {-1, 1}) {
                const QPointF c(12 + sx * 8.5, 12 + sy * 8.5);
                p.drawLine(c, QPointF(c.x() - sx * 5, c.y()));
                p.drawLine(c, QPointF(c.x(), c.y() - sy * 5));
            }
        }
        p.drawRect(QRectF(9, 9, 6, 6));
    } else if (name == QLatin1String("board")) {
        p.drawRect(QRectF(3, 3, 7.5, 7.5));
        p.drawRect(QRectF(13.5, 3, 7.5, 7.5));
        p.drawRect(QRectF(3, 13.5, 7.5, 7.5));
        p.drawRect(QRectF(13.5, 13.5, 7.5, 7.5));
    } else if (name == QLatin1String("preview-open") || name == QLatin1String("preview-close")) {
        QPainterPath eye;
        eye.moveTo(2.5, 12);
        eye.cubicTo(7, 4.5, 17, 4.5, 21.5, 12);
        eye.cubicTo(17, 19.5, 7, 19.5, 2.5, 12);
        p.drawPath(eye);
        p.drawEllipse(QPointF(12, 12), 3, 3);
        if (name == QLatin1String("preview-close")) {
            p.drawLine(QPointF(4, 20), QPointF(20, 4));
        }
    } else if (name == QLatin1String("pane")) {
        p.drawRect(QRectF(3, 4, 18, 16));
        p.drawLine(QPointF(14, 4), QPointF(14, 20));
        p.drawLine(QPointF(16, 8), QPointF(19, 8));
        p.drawLine(QPointF(16, 11), QPointF(19, 11));
    } else if (name == QLatin1String("export") || name == QLatin1String("import")) {
        // Tray with an arrow leaving (export) or entering (import).
        QPainterPath tray;
        tray.moveTo(4, 14);
        tray.lineTo(4, 20);
        tray.lineTo(20, 20);
        tray.lineTo(20, 14);
        p.drawPath(tray);
        if (name == QLatin1String("export")) {
            p.drawLine(QPointF(12, 15), QPointF(12, 4));
            drawArrowHead(p, QPointF(12, 3.5), -90, 4.5);
        } else {
            p.drawLine(QPointF(12, 3), QPointF(12, 14));
            drawArrowHead(p, QPointF(12, 15), 90, 4.5);
        }
    } else if (name == QLatin1String("toolbar")) {
        p.drawRect(QRectF(3, 5, 18, 14));
        p.drawLine(QPointF(3, 10), QPointF(21, 10));
    } else {
        p.drawRect(QRectF(4, 4, 16, 16));
    }
}

inline QPixmap renderToolbarIcon(const QString& name, const QColor& color, int logicalSize, qreal dpr)
{
    QPixmap pixmap(QSize(logicalSize, logicalSize) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(logicalSize / 24.0, logicalSize / 24.0);
    QPen pen(color, 1.9);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    drawToolbarIcon(p, name);
    return pixmap;
}

}  // namespace detail

// Icon in `color`, with a faded copy for the disabled state. `logicalSize` is the toolbar's
// icon size; 1x and 2x pixmaps are supplied so high-DPI screens pick the sharp one.
inline QIcon makeToolbarIcon(const QString& name, const QColor& color, int logicalSize = 20)
{
    QColor disabled = color;
    disabled.setAlphaF(0.35);
    QIcon icon;
    for (const qreal dpr : {1.0, 2.0}) {
        icon.addPixmap(detail::renderToolbarIcon(name, color, logicalSize, dpr), QIcon::Normal);
        icon.addPixmap(detail::renderToolbarIcon(name, disabled, logicalSize, dpr), QIcon::Disabled);
    }
    return icon;
}

}  // namespace mycel
