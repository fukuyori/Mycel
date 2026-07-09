#pragma once

// The folder-style node representing a parent .mycel root, shown to the left of the current root.
// Extracted verbatim out of main.cpp.

#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtGui/QFont>
#include <QtGui/QFontMetricsF>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsSceneMouseEvent>

#include <algorithm>
#include <functional>

#include "tree_model.h"

// A folder-style node shown to the left of the root that represents a parent .mycel root. Rendered
// identically to a sub-root (folder + Mycel badge + name) and scales with zoom like any node, so the
// parent (seen from the child) matches a child (seen from the parent). Single click selects
// (highlights); double click switches up to that parent root.
class ParentRootItem final : public QGraphicsItem {
public:
    ParentRootItem(QString path, QString name)
        : path_(std::move(path)), name_(std::move(name))
    {
        setFlag(ItemIsSelectable, true);
        setZValue(NodeLayerZ);
        QFont font;
        font.setPointSize(12);
        const QFontMetricsF metrics(font);
        const qreal width = std::max(128.0, metrics.horizontalAdvance(shortLabel(name_)) + 46.0 + 34.0);
        box_ = QRectF(-width / 2.0, -23.0, width, 46.0);
    }

    void setActivateHandler(std::function<void(QString)> handler) { handler_ = std::move(handler); }
    qreal boxWidth() const { return box_.width(); }

    QRectF boundingRect() const override { return box_.adjusted(-8.0, -8.0, 8.0, 8.0); }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        const ThemeColors colors = currentThemeColors();
        painter->setRenderHint(QPainter::Antialiasing, !fastCanvasRendering());
        if (isSelected()) {
            painter->setPen(QPen(colors.nodeSelectedBorder, 2.0));
            painter->setBrush(colors.nodeSelectedFill);
            painter->drawRoundedRect(box_.adjusted(-4.0, -3.0, 4.0, 3.0), 8.0, 8.0);
        }
        painter->drawPixmap(QPointF(box_.left() + 15.0, -18.0), subRootFolderPixmap());
        QFont font = painter->font();
        font.setPointSize(12);
        painter->setFont(font);
        painter->setPen(colors.nodeText);
        painter->drawText(QRectF(box_.left() + 66.0, box_.top(), box_.width() - 76.0, box_.height()),
                          Qt::AlignVCenter | Qt::AlignLeft, shortLabel(name_));
    }

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) override
    {
        if (handler_) {
            handler_(path_);
        }
    }

private:
    QString path_;
    QString name_;
    QRectF box_;
    std::function<void(QString)> handler_;
};
