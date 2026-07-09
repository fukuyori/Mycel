#pragma once

// The QGraphicsScene subclass that paints the canvas background grid. Extracted verbatim out of main.cpp.

#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtWidgets/QGraphicsScene>

#include <cmath>

#include "tree_model.h"

class MindMapScene final : public QGraphicsScene {
protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override
    {
        const ThemeColors colors = currentThemeColors();
        painter->fillRect(rect, colors.canvasBackground);
        QColor gridColor = currentAppTheme() == AppTheme::Dark ? QColor(69, 78, 88, 150) : QColor(216, 213, 205, 170);
        painter->setPen(QPen(gridColor, 1.0));
        const int grid = 28;
        const qreal left = std::floor(rect.left() / grid) * grid;
        const qreal top = std::floor(rect.top() / grid) * grid;
        QVector<QPointF> points;
        points.reserve(static_cast<int>((rect.width() / grid + 2.0) * (rect.height() / grid + 2.0)));
        for (qreal x = left; x < rect.right(); x += grid) {
            for (qreal y = top; y < rect.bottom(); y += grid) {
                points.append(QPointF(x, y));
            }
        }
        painter->drawPoints(points.constData(), points.size());
    }
};
