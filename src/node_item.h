#pragma once

// The node QGraphicsItem. Extracted verbatim out of main.cpp; MainWindow-dependent method
// bodies stay defined out-of-line in main.cpp (after MainWindow's own definition) since they
// need MainWindow to be a complete type -- this header only forward-declares it.

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPen>
#include <QtGui/QPixmap>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsSceneContextMenuEvent>
#include <QtWidgets/QGraphicsSceneDragDropEvent>
#include <QtWidgets/QGraphicsView>

#include "tree_model.h"

class MainWindow;

class NodeItem final : public QGraphicsItem {
public:
    NodeItem(Node* node, MainWindow* window) : node_(node), window_(window)
    {
        layoutCenter_ = node_->center;
        setPos(node_->center);
        setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);
        setAcceptDrops(node_->isDir);
        setZValue(NodeLayerZ);
        createPreviewWidget();
    }

    Node* node() const { return node_; }
    QString path() const { return node_ ? node_->path : QString(); }
    QPointF layoutCenter() const { return layoutCenter_; }

    // In-place refresh after a metadata-only relayout: move to the recomputed center, create or
    // drop the inline preview widget to match previewOpen, and repaint. Reusing the live item
    // (instead of recreating the scene) keeps the selection and avoids widget churn.
    void syncFromNode()
    {
        prepareGeometryChange();
        layoutCenter_ = node_->center;
        setPos(node_->center);
        setOpacity(1.0);           // clear any leftover drag visuals
        setZValue(NodeLayerZ);
        const bool wantsPreviewWidget = node_->previewOpen && !node_->isDir;
        if (!wantsPreviewWidget && previewProxy_) {
            delete previewProxy_;
            previewProxy_ = nullptr;
        } else if (wantsPreviewWidget && !previewProxy_) {
            createPreviewWidget();
        } else if (previewProxy_) {
            syncPreviewWidgetGeometry();
        }
        update();
    }
    void setInternalDropHover(bool hover)
    {
        if (internalDropHover_ == hover) {
            return;
        }
        internalDropHover_ = hover;
        update();
    }
    void setLinkDropHover(bool hover)
    {
        if (linkDropHover_ == hover) {
            return;
        }
        linkDropHover_ = hover;
        update();
    }
    QRectF linkDropSceneRect() const
    {
        if (node_->isDir) {
            return {};
        }
        const QRectF box(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                         node_->size.width(), node_->size.height());
        return mapRectToScene(QRectF(box.right() - 28.0, box.top() - 3.0, 34.0, box.height() + 6.0));
    }
    bool containsLinkDropScenePoint(const QPointF& scenePos, qreal margin = 0.0) const
    {
        return linkDropSceneRect().adjusted(-margin, -margin, margin, margin).contains(scenePos);
    }
    bool canResizePreviewAtScene(const QPointF& scenePos) const
    {
        if (!node_->previewOpen || node_->isDir) {
            return false;
        }
        return previewResizeHandle().contains(mapFromScene(scenePos));
    }
    QString imageResizeDebugAtScene(const QPointF& scenePos) const
    {
        const QPointF local = mapFromScene(scenePos);
        const QRectF rect = previewRect();
        const QRectF handle = previewResizeHandle();
        return QStringLiteral("path=%1 previewOpen=%2 isDir=%3 isImage=%4 scene=(%5,%6) local=(%7,%8) rect=(%9,%10,%11,%12) inside=%13 handle=(%14,%15,%16,%17) insideHandle=%18")
            .arg(node_ ? node_->path : QStringLiteral("(null)"))
            .arg(node_ && node_->previewOpen ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(node_ && node_->isDir ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(node_ && isImagePreviewFile(QFileInfo(node_->path)) ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(scenePos.x(), 0, 'f', 1)
            .arg(scenePos.y(), 0, 'f', 1)
            .arg(local.x(), 0, 'f', 1)
            .arg(local.y(), 0, 'f', 1)
            .arg(rect.x(), 0, 'f', 1)
            .arg(rect.y(), 0, 'f', 1)
            .arg(rect.width(), 0, 'f', 1)
            .arg(rect.height(), 0, 'f', 1)
            .arg(rect.contains(local) ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(handle.x(), 0, 'f', 1)
            .arg(handle.y(), 0, 'f', 1)
            .arg(handle.width(), 0, 'f', 1)
            .arg(handle.height(), 0, 'f', 1)
            .arg(handle.contains(local) ? QStringLiteral("true") : QStringLiteral("false"));
    }
    qreal currentImageResizeScale() const
    {
        return imagePreviewScaleForSize(QFileInfo(node_->path), node_->previewSize, false);
    }
    QSizeF currentPreviewSize() const
    {
        return node_ ? node_->previewSize : QSizeF();
    }
    void showContextMenuAt(const QPoint& screenPos);
    void applyImagePreviewScale(qreal scale)
    {
        if (!node_ || node_->isDir || !isImagePreviewFile(QFileInfo(node_->path))) {
            return;
        }
        prepareGeometryChange();
        node_->previewSize = imagePreviewSizeForScale(QFileInfo(node_->path), scale);
        syncPreviewWidgetGeometry();
        update();
    }
    void applyPreviewSize(const QSizeF& size, bool preferHeight)
    {
        if (!node_ || node_->isDir || isImagePreviewFile(QFileInfo(node_->path))) {
            return;
        }
        prepareGeometryChange();
        node_->previewSize = clampedPreviewSizeForFile(QFileInfo(node_->path), size, preferHeight);
        syncPreviewWidgetGeometry();
        update();
    }
    void savePreviewSize(const QSizeF& size, bool preferHeight)
    {
        if (!node_ || node_->isDir || isImagePreviewFile(QFileInfo(node_->path))) {
            return;
        }
        resizePreview(size, preferHeight);
    }
    void saveImagePreviewScale(qreal scale)
    {
        if (!node_ || node_->isDir || !isImagePreviewFile(QFileInfo(node_->path))) {
            return;
        }
        resizeImagePreview(scale);
    }
    bool beginImagePreviewResizeAtScene(const QPointF& scenePos)
    {
        if (!canResizePreviewAtScene(scenePos)) {
            return false;
        }
        resizingPreview_ = true;
        resizePreferHeight_ = false;
        resizeStartScene_ = scenePos;
        resizeStartSize_ = node_->previewSize;
        resizeStartScale_ = imagePreviewScaleForSize(QFileInfo(node_->path), node_->previewSize, false);
        resizeCurrentScale_ = resizeStartScale_;
        setSelected(true);
        update();
        return true;
    }
    void updateImagePreviewResizeAtScene(const QPointF& scenePos)
    {
        if (!resizingPreview_ || node_->isDir) {
            return;
        }
        prepareGeometryChange();
        const QPointF delta = scenePos - resizeStartScene_;
        const QFileInfo info(node_->path);
        const bool preferHeight = std::abs(delta.y()) > std::abs(delta.x());
        const QSizeF targetSize = preferHeight
                                      ? QSizeF(resizeStartSize_.width(),
                                               resizeStartSize_.height() + delta.y())
                                      : QSizeF(resizeStartSize_.width() + delta.x(),
                                               resizeStartSize_.height());
        resizeCurrentScale_ = imagePreviewScaleForSize(info, targetSize, preferHeight);
        node_->previewSize = imagePreviewSizeForScale(info, resizeCurrentScale_);
        resizeCurrentScale_ = imagePreviewScaleForSize(info, node_->previewSize, false);
        syncPreviewWidgetGeometry();
        update();
    }
    void finishImagePreviewResize()
    {
        if (!resizingPreview_) {
            return;
        }
        resizingPreview_ = false;
        resizeImagePreview(resizeCurrentScale_);
    }
    QRectF labelSceneRect() const
    {
        const QRectF box(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                         node_->size.width(), node_->size.height());
        const qreal leftPadding = node_->isDir ? 66.0 : 40.0;
        return mapRectToScene(QRectF(box.left() + leftPadding - 3.0, box.top() + 4.0,
                                     box.width() - leftPadding - 8.0, box.height() - 8.0));
    }
    void refreshPreviewWidget(const QSizeF& previewSize)
    {
        if (!node_->previewOpen || node_->isDir) {
            return;
        }
        if (node_->previewSize != previewSize) {
            prepareGeometryChange();
            node_->previewSize = previewSize;
        }
        delete previewProxy_;
        previewProxy_ = nullptr;
        createPreviewWidget();
        update();
    }

    QRectF boundingRect() const override
    {
        QRectF rect(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                    node_->size.width(), node_->size.height());
        if (node_->collapsed && node_->hiddenChildren > 0) {
            rect.setRight(rect.right() + 104.0);
        }
        if (node_->previewOpen && !node_->isDir) {
            rect = rect.united(previewFrameRect());
        }
        // The selection outline and drop-hover outlines are stroked a few pixels OUTSIDE the
        // node box (see paint(): box.adjusted(-5,-5,5,5) with a 3px pen is the widest). Grow the
        // bounding rect to enclose them, otherwise deselecting only invalidates the inner box and
        // leaves the outline's outer edge painted on the canvas as a stale frame.
        return rect.adjusted(-8.0, -8.0, 8.0, 8.0);
    }

    QPainterPath shape() const override
    {
        QPainterPath path;
        const QRectF box(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                         node_->size.width(), node_->size.height());
        path.addRoundedRect(box, 8.0, 8.0);
        if (node_->previewOpen && !node_->isDir) {
            path.addRect(previewFrameRect());
        }
        if (node_->collapsed && node_->hiddenChildren > 0) {
            path.addRect(QRectF(box.right(), box.top(), 104.0, box.height()));
        }
        return path;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        const ThemeColors colors = currentThemeColors();
        painter->setRenderHint(QPainter::Antialiasing, !fastCanvasRendering());
        const QRectF box(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                         node_->size.width(), node_->size.height());
        if (!node_->isDir && node_->previewOpen) {
            paintPreviewFrame(painter);
            return;
        }

        if (hasUserFill()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(windowFill());
            painter->drawRoundedRect(box.adjusted(-4.0, -3.0, 4.0, 3.0), 8.0, 8.0);
        }
        if (isSelected()) {
            painter->setPen(QPen(colors.nodeSelectedBorder, 2.0));
            painter->setBrush(hasUserFill() ? QColor(colors.nodeSelectedFill.red(), colors.nodeSelectedFill.green(), colors.nodeSelectedFill.blue(), 190)
                                            : colors.nodeSelectedFill);
            painter->drawRoundedRect(box.adjusted(-4.0, -3.0, 4.0, 3.0), 8.0, 8.0);
        }

        if (node_->isDir && (externalDropHover_ || internalDropHover_)) {
            painter->setPen(QPen(colors.highlight, 3.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(QColor(colors.highlight.red(), colors.highlight.green(), colors.highlight.blue(), 34));
            painter->drawRoundedRect(box.adjusted(-5.0, -5.0, 5.0, 5.0), 15.0, 15.0);
        }
        if (!node_->isDir && linkDropHover_) {
            const QRectF zone(box.right() - 25.0, box.top() + 3.0, 30.0, box.height() - 6.0);
            painter->setPen(QPen(colors.linkAccent, 2.4, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(QColor(colors.linkAccent.red(), colors.linkAccent.green(), colors.linkAccent.blue(), 42));
            painter->drawRoundedRect(zone, 8.0, 8.0);
            painter->setPen(QPen(colors.linkAccent.darker(currentAppTheme() == AppTheme::Dark ? 90 : 130), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawLine(QPointF(zone.center().x() - 5.0, zone.center().y()),
                              QPointF(zone.center().x() + 5.0, zone.center().y()));
        }

        if (node_->isDir) {
            paintFolder(painter, QPointF(box.left() + 18.0, -17.0), node_->depth == 0 ? QColor("#46a4ff") : QColor("#ffc94d"));
        } else {
            paintFile(painter, QPointF(box.left() + 6.0, -14.0), QFileInfo(node_->path));
        }

        // A sub-root (child folder with its own .mycel) gets a Mycel badge on its folder icon.
        if (node_->isSubRoot) {
            static const QPixmap mycelBadge = QPixmap(QStringLiteral(":/icons/mycel.png"))
                                                  .scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            if (!mycelBadge.isNull()) {
                painter->drawPixmap(QPointF(box.left() + 40.0, 1.0), mycelBadge);
            }
        }

        QFont font = painter->font();
        font.setPointSize(node_->depth == 0 ? 17 : (node_->isDir ? 12 : 11));
        painter->setFont(font);
        painter->setPen(colors.nodeText);
        painter->drawText(QRectF(box.left() + (node_->isDir ? 66.0 : 40.0), box.top(),
                                 box.width() - (node_->isDir ? 76.0 : 42.0), box.height()),
                          Qt::AlignVCenter | Qt::AlignLeft, shortLabel(node_->name));

        if (node_->collapsed && node_->hiddenChildren > 0) {
            const QRectF badge(box.right() + 8.0, -13.0, 88.0, 26.0);
            painter->setPen(QPen(colors.badgeBorder, 1.1));
            painter->setBrush(colors.badgeBackground);
            painter->drawRoundedRect(badge, 10.0, 10.0);
            QFont badgeFont = painter->font();
            badgeFont.setPointSize(10);
            badgeFont.setBold(true);
            painter->setFont(badgeFont);
            painter->setPen(colors.badgeText);
            painter->drawText(badge, Qt::AlignCenter, QStringLiteral("配下 %1 件").arg(node_->hiddenChildren));
        } else if (node_->hiddenChildren > 0) {
            painter->setPen(colors.badgeText);
            painter->drawText(QRectF(box.right() - 50.0, box.top(), 44.0, box.height()),
                              Qt::AlignCenter, QStringLiteral("+%1").arg(node_->hiddenChildren));
        }

        if (node_->previewOpen) {
            paintInlinePreview(painter, previewRect());
        }
    }

    // Drag gesture driven by BoardView (single owner). The view reliably receives
    // press/move/release, but the item's implicit mouse grab can be lost mid-drag, so
    // the item no longer overrides the mouse events. It just exposes these driver hooks.
    void beginDrag(const QPointF& scenePos, Qt::KeyboardModifiers modifiers);
    void updateDrag(const QPointF& scenePos);
    void finishDrag(Qt::MouseButton button, const QPointF& scenePos);
    // Right-click selects this node (or keeps it if already part of the selection) so the
    // context menu acts on it and the selection frame stays visible.
    void selectForContextMenu();
    // Default action (double-click): toggle a folder's collapse or a file's inline preview.
    // Driven by BoardView, since the view owns the press that would otherwise let
    // QGraphicsView deliver the double-click here.
    void activate();

protected:
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
    void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
    void dropEvent(QGraphicsSceneDragDropEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    QColor windowColor() const;
    QColor windowFill() const;
    bool hasUserFill() const;
    QStringList windowInlinePreviewLines() const;
    QString windowMarkdownPreviewText() const;
    bool windowIsDocumentThumbnail(const QFileInfo& info) const;
    QPixmap windowDocumentThumbnail(const QFileInfo& info) const;
    void updateDragAtScene(const QPointF& scenePos);
    void finishDragAtScene(Qt::MouseButton button, const QPointF& scenePos);
    void resizePreview(const QSizeF& size, bool preferHeight);
    void resizeImagePreview(qreal scale);
    void createPreviewWidget();
    void syncPreviewWidgetGeometry();

    static constexpr qreal PreviewHeaderHeight = 34.0;

    QRectF previewFrameRect() const
    {
        return previewHeaderRect().united(previewRect());
    }

    QRectF previewHeaderRect() const
    {
        const QRectF box(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                         node_->size.width(), node_->size.height());
        return QRectF(box.left(), box.top(), node_->size.width(), PreviewHeaderHeight);
    }

    QRectF previewRect() const
    {
        const QRectF header = previewHeaderRect();
        return QRectF(header.left(), header.bottom(),
                      node_->previewSize.width(), node_->previewSize.height());
    }

    QRectF previewResizeHandle() const
    {
        const QRectF rect = previewRect();
        const qreal grip = isImagePreviewFile(QFileInfo(node_->path)) ? 36.0 : 28.0;
        return QRectF(rect.right() - grip, rect.bottom() - grip, grip, grip);
    }

    void paintFolder(QPainter* painter, const QPointF& at, const QColor& fill)
    {
        QPainterPath path;
        path.moveTo(at.x(), at.y() + 9.0);
        path.quadTo(at.x(), at.y() + 4.0, at.x() + 5.0, at.y() + 4.0);
        path.lineTo(at.x() + 16.0, at.y() + 4.0);
        path.lineTo(at.x() + 22.0, at.y() + 10.0);
        path.lineTo(at.x() + 36.0, at.y() + 10.0);
        path.quadTo(at.x() + 40.0, at.y() + 10.0, at.x() + 40.0, at.y() + 14.0);
        path.lineTo(at.x() + 40.0, at.y() + 31.0);
        path.quadTo(at.x() + 40.0, at.y() + 36.0, at.x() + 35.0, at.y() + 36.0);
        path.lineTo(at.x() + 5.0, at.y() + 36.0);
        path.quadTo(at.x(), at.y() + 36.0, at.x(), at.y() + 31.0);
        path.closeSubpath();

        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawPath(path);
        painter->setPen(QPen(QColor("#f6b93f"), 1.8));
        painter->drawLine(QPointF(at.x() + 3.0, at.y() + 14.0), QPointF(at.x() + 37.0, at.y() + 14.0));
    }

    void paintFile(QPainter* painter, const QPointF& at, const QFileInfo& info)
    {
        const ThemeColors colors = currentThemeColors();
        QPainterPath page;
        page.moveTo(at);
        page.lineTo(at.x() + 16.0, at.y());
        page.lineTo(at.x() + 26.0, at.y() + 10.0);
        page.lineTo(at.x() + 26.0, at.y() + 32.0);
        page.lineTo(at.x(), at.y() + 32.0);
        page.closeSubpath();

        painter->setPen(QPen(colors.fileStroke, 1.3));
        painter->setBrush(colors.filePage);
        painter->drawPath(page);
        painter->drawLine(QPointF(at.x() + 16.0, at.y()), QPointF(at.x() + 16.0, at.y() + 10.0));
        painter->drawLine(QPointF(at.x() + 16.0, at.y() + 10.0), QPointF(at.x() + 26.0, at.y() + 10.0));

        painter->setPen(QPen(colors.fileInk, 1.1));
        const bool image = isImagePreviewFile(info);
        if (image) {
            painter->drawRect(QRectF(at.x() + 5.0, at.y() + 15.0, 15.0, 10.0));
            QPainterPath mountain;
            mountain.moveTo(at.x() + 6.5, at.y() + 24.0);
            mountain.lineTo(at.x() + 11.0, at.y() + 19.5);
            mountain.lineTo(at.x() + 14.0, at.y() + 22.0);
            mountain.lineTo(at.x() + 17.5, at.y() + 18.0);
            mountain.lineTo(at.x() + 20.0, at.y() + 24.0);
            painter->drawPath(mountain);
            return;
        }

        const FileKindStyle kind = fileKindStyleFor(info);
        if (!kind.badge.isEmpty()) {
            QFont badgeFont = painter->font();
            badgeFont.setPointSize(kind.badge.size() > 2 ? 5 : 7);
            badgeFont.setBold(true);
            painter->setFont(badgeFont);
            painter->setPen(QPen(kind.accent.isValid() ? kind.accent : colors.fileInk, 1.1));
            painter->drawText(QRectF(at.x() + 3.0, at.y() + 14.0, 20.0, 13.0), Qt::AlignCenter, kind.badge);
            return;
        }

        painter->drawLine(QPointF(at.x() + 5.0, at.y() + 17.0), QPointF(at.x() + 19.0, at.y() + 17.0));
        painter->drawLine(QPointF(at.x() + 5.0, at.y() + 22.0), QPointF(at.x() + 17.0, at.y() + 22.0));
    }

    void paintInlinePreview(QPainter* painter, const QRectF& rect)
    {
        const ThemeColors colors = currentThemeColors();
        QColor border = hasUserFill() ? colors.inlinePreviewBorder : colors.inlinePreviewBorder;
        border.setAlpha(hasUserFill() ? 180 : 255);
        painter->setPen(QPen(border, 1.1));
        painter->setBrush(hasUserFill() ? windowFill() : colors.inlinePreviewBackground);
        painter->drawRect(rect);

        painter->setPen(QPen(colors.inlinePreviewBorder, 1.2));
        const QPointF corner(rect.right() - 12.0, rect.bottom() - 4.0);
        painter->drawLine(corner, QPointF(rect.right() - 4.0, rect.bottom() - 12.0));
        painter->drawLine(QPointF(rect.right() - 8.0, rect.bottom() - 4.0),
                          QPointF(rect.right() - 4.0, rect.bottom() - 8.0));
    }

    void paintPreviewFrame(QPainter* painter)
    {
        const ThemeColors colors = currentThemeColors();
        const QRectF header = previewHeaderRect();
        const QRectF body = previewRect();
        const bool selected = isSelected();
        const QColor border = selected ? colors.nodeSelectedBorder : (hasUserFill() ? windowColor() : colors.nodeStroke);
        const QColor bodyFill = selected ? colors.previewPanel : colors.inlinePreviewBackground;
        const QColor headerFill = selected ? colors.nodeSelectedFill : (hasUserFill() ? windowFill() : colors.nodeFill);
        constexpr qreal radius = 4.0;

        painter->setPen(QPen(border, selected ? 2.0 : 1.4));
        painter->setBrush(bodyFill);
        painter->drawRoundedRect(body, radius, radius);

        painter->setPen(Qt::NoPen);
        painter->setBrush(headerFill);
        painter->drawRoundedRect(header.adjusted(1.0, 1.0, -1.0, -1.0), radius, radius);

        painter->setPen(QPen(border, selected ? 2.0 : 1.4));
        painter->drawRoundedRect(header, radius, radius);

        painter->save();
        painter->translate(QPointF(header.left() + 12.0, header.top() + 6.0));
        painter->scale(0.68, 0.68);
        paintFile(painter, QPointF(0.0, 0.0), QFileInfo(node_->path));
        painter->restore();

        QFont font = painter->font();
        font.setPointSize(node_->depth == 0 ? 14 : 11);
        font.setBold(true);
        painter->setFont(font);
        painter->setPen(colors.nodeText);
        painter->drawText(QRectF(header.left() + 40.0, header.top(),
                                 header.width() - 48.0, header.height()),
                          Qt::AlignVCenter | Qt::AlignLeft, shortLabel(node_->name));

        const QFileInfo info(node_->path);
        QPixmap previewPixmap;
        if (isImagePreviewFile(info)) {
            previewPixmap = cachedImagePixmapForFile(info);
        } else if (windowIsDocumentThumbnail(info)) {
            previewPixmap = windowDocumentThumbnail(info);  // PDF first-page thumbnail
        }
        if (!previewPixmap.isNull()) {
            const QRectF imageArea = body.adjusted(6.0, 6.0, -6.0, -6.0);  // symmetric → centered
            if (imageArea.width() > 1.0 && imageArea.height() > 1.0) {
                painter->save();
                painter->setRenderHint(QPainter::SmoothPixmapTransform, !fastCanvasRendering());
                const QSizeF scaled = previewPixmap.size().scaled(imageArea.size().toSize(), Qt::KeepAspectRatio);
                const QRectF target(QPointF(imageArea.left() + (imageArea.width() - scaled.width()) / 2.0,
                                            imageArea.top() + (imageArea.height() - scaled.height()) / 2.0),
                                    scaled);
                painter->drawPixmap(target, previewPixmap, QRectF(QPointF(0.0, 0.0), previewPixmap.size()));
                painter->restore();
            }
        }

        painter->setPen(QPen(colors.inlinePreviewBorder, 1.2));
        const QPointF corner(body.right() - 12.0, body.bottom() - 4.0);
        painter->drawLine(corner, QPointF(body.right() - 4.0, body.bottom() - 12.0));
        painter->drawLine(QPointF(body.right() - 8.0, body.bottom() - 4.0),
                          QPointF(body.right() - 4.0, body.bottom() - 8.0));
    }

    Node* node_;
    MainWindow* window_;
    QPointF layoutCenter_;
    QPointF dragStart_;
    QPointF pressStartScene_;
    Qt::KeyboardModifiers pressModifiers_ = Qt::NoModifier;
    bool dragMoveLogged_ = false;
    bool resizingPreview_ = false;
    bool resizePreferHeight_ = false;
    qreal resizeStartScale_ = 1.0;
    qreal resizeCurrentScale_ = 1.0;
    bool externalDropHover_ = false;
    bool internalDropHover_ = false;
    bool linkDropHover_ = false;
    QPointF resizeStartScene_;
    QSizeF resizeStartSize_;
    QGraphicsProxyWidget* previewProxy_ = nullptr;
};
