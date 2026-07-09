#pragma once

// The QGraphicsView subclass driving pan/zoom/selection/drag for the canvas. Extracted verbatim out of main.cpp.

#include <QtCore/QEvent>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QString>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QKeySequence>
#include <QtGui/QMouseEvent>
#include <QtGui/QNativeGestureEvent>
#include <QtGui/QPainter>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>

#include <algorithm>
#include <functional>

#include "node_item.h"
#include "drag_controller.h"

class BoardView final : public QGraphicsView {
public:
    explicit BoardView(QWidget* parent = nullptr) : QGraphicsView(parent)
    {
        setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        setDragMode(QGraphicsView::NoDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorUnderMouse);
        setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
        setCacheMode(QGraphicsView::CacheBackground);
        setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
        setFocusPolicy(Qt::StrongFocus);
        setAcceptDrops(true);
        viewport()->setAcceptDrops(true);
        fastRenderTimer_.setSingleShot(true);
        fastRenderTimer_.setInterval(120);
        connect(&fastRenderTimer_, &QTimer::timeout, this, [this] { endFastRendering(); });
    }

    void setCheatSheetHandler(std::function<void()> handler)
    {
        cheatSheetHandler_ = std::move(handler);
    }

    void setKeyHandler(std::function<bool(QKeyEvent*)> handler)
    {
        keyHandler_ = std::move(handler);
    }

    void setViewChangedHandler(std::function<void()> handler)
    {
        viewChangedHandler_ = std::move(handler);
    }

    void setDebugHandler(std::function<void(const QString&)> handler)
    {
        debugHandler_ = std::move(handler);
    }

    void cancelTransientNodeInteraction()
    {
        imageResizePath_.clear();
        // A rebuild destroys every NodeItem, so any in-progress drag pointer would
        // dangle. Drop it here (renderCurrentTree calls this before scene_.clear()).
        dragController_.cancel();
    }

    bool isDraggingNode() const { return dragController_.active(); }

protected:
    void wheelEvent(QWheelEvent* event) override
    {
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
        if (!(event->modifiers() & Qt::ControlModifier)) {
            const QPoint pixelDelta = event->pixelDelta();
            if (!pixelDelta.isNull()) {
                beginFastRendering();
                horizontalScrollBar()->setValue(horizontalScrollBar()->value() - pixelDelta.x());
                verticalScrollBar()->setValue(verticalScrollBar()->value() - pixelDelta.y());
                notifyViewChanged();
                event->accept();
                return;
            }
        }
#endif
        const QPoint delta = event->angleDelta();
        if (!delta.isNull()) {
            beginFastRendering();
            const int amount = delta.y();
            zoomAt(event->position(), amount > 0 ? 1.12 : 0.89);
            event->accept();
            return;
        }
        QGraphicsView::wheelEvent(event);
    }

    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (!textInputWidgetHasFocus() && isBoardNavigationKey(keyEvent)) {
                if (keyHandler_ && keyHandler_(keyEvent)) {
                    notifyDebug(QStringLiteral("board event key handled by node navigation: %1").arg(debugKeyName(keyEvent)));
                    event->accept();
                    return true;
                }
                if (scrollWithArrowKey(keyEvent)) {
                    event->accept();
                    return true;
                }
            }
        }
        if (event->type() == QEvent::NativeGesture) {
            auto* gesture = static_cast<QNativeGestureEvent*>(event);
            if (gesture->gestureType() == Qt::ZoomNativeGesture) {
                const qreal factor = 1.0 + gesture->value();
                if (factor > 0.0) {
                    beginFastRendering();
                    zoomAt(gesture->position(), factor);
                }
                event->accept();
                return true;
            }
        }
        return QGraphicsView::event(event);
    }

    void contextMenuEvent(QContextMenuEvent* event) override
    {
        if (NodeItem* nodeItem = nodeItemAt(event->pos())) {
            nodeItem->showContextMenuAt(event->globalPos());
            event->accept();
            return;
        }
        QGraphicsView::contextMenuEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            const bool directItemHit = itemAt(event->pos()) != nullptr;
            notifyDebug(QStringLiteral("view mouse press itemAt: %1")
                            .arg(directItemHit ? QStringLiteral("item") : QStringLiteral("none")));
            if (NodeItem* nodeItem = nodeItemAt(event->pos())) {
                notifyDebug(QStringLiteral("view mouse press node hit: %1 direct=%2")
                                .arg(nodeItem->path(),
                                     directItemHit ? QStringLiteral("true") : QStringLiteral("false")));
                notifyDebug(QStringLiteral("image resize candidate: %1")
                                .arg(nodeItem->imageResizeDebugAtScene(mapToScene(event->pos()))));
                const QPointF scenePos = mapToScene(event->pos());
                if (nodeItem->canResizePreviewAtScene(scenePos)) {
                    imageResizePath_ = nodeItem->path();
                    imageResizeStartScene_ = scenePos;
                    imageResizeStartSize_ = nodeItem->currentPreviewSize();
                    imageResizeStartScale_ = nodeItem->currentImageResizeScale();
                    imageResizeCurrentScale_ = imageResizeStartScale_;
                    notifyDebug(QStringLiteral("image resize begin scale=%1 size=(%2,%3)")
                                    .arg(imageResizeCurrentScale_, 0, 'f', 4)
                                    .arg(nodeItem->currentPreviewSize().width(), 0, 'f', 1)
                                    .arg(nodeItem->currentPreviewSize().height(), 0, 'f', 1));
                    event->accept();
                    return;
                }
                // The view owns the node drag end to end, so the release is always delivered
                // here even if the item's implicit grab is lost. Start a drag when the press
                // is on the node body — that includes the case where itemAt() missed entirely
                // (a frequent quirk that nodeItemAt resolves by geometry). Only pass through
                // when itemAt() reports a different item, i.e. a child preview proxy widget,
                // so the preview stays interactive. Alt+drag is reserved for panning.
                QGraphicsItem* hitItem = itemAt(event->pos());
                const bool onChildWidget = hitItem != nullptr && hitItem != nodeItem;
                if (!(event->modifiers() & Qt::AltModifier) && !onChildWidget) {
                    dragController_.begin(nodeItem, scenePos, event->modifiers());
                    event->accept();
                    return;
                }
            } else {
                notifyDebug(QStringLiteral("image resize candidate: no NodeItem"));
            }
        }
        const bool rightButtonCanvasPan = event->button() == Qt::RightButton &&
                                          itemAt(event->pos()) == nullptr &&
                                          nodeItemAt(event->pos()) == nullptr;
        if ((event->button() == Qt::MiddleButton) ||
            rightButtonCanvasPan ||
            (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier))) {
            panning_ = true;
            panningButton_ = event->button();
            lastPanPoint_ = event->pos();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton && itemAt(event->pos()) == nullptr) {
            rubberBandSelecting_ = true;
            rubberBandStart_ = event->pos();
            setDragMode(QGraphicsView::RubberBandDrag);
            QGraphicsView::mousePressEvent(event);
            return;
        }
        // Right-click on a node: establish/keep its selection and consume the press so the scene's
        // default press handling can't clear the selection (the context menu still fires on
        // right-button release). Without this the highlight frame vanishes on right-click.
        if (event->button() == Qt::RightButton) {
            if (NodeItem* nodeItem = nodeItemAt(event->pos())) {
                nodeItem->selectForContextMenu();
                event->accept();
                return;
            }
        }
        QGraphicsView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!imageResizePath_.isEmpty()) {
            NodeItem* resizeItem = nodeItemForPath(imageResizePath_);
            if (!resizeItem) {
                notifyDebug(QStringLiteral("image resize move canceled: item missing"));
                imageResizePath_.clear();
                event->accept();
                return;
            }
            const QPointF delta = mapToScene(event->pos()) - imageResizeStartScene_;
            const QSizeF targetSize = previewResizeTargetSize(QFileInfo(imageResizePath_),
                                                              imageResizeStartSize_, delta);
            const bool preferHeight = std::abs(delta.y()) > std::abs(delta.x());
            if (isImagePreviewFile(QFileInfo(imageResizePath_))) {
                imageResizeCurrentScale_ = imagePreviewScaleForSize(QFileInfo(imageResizePath_),
                                                                    targetSize,
                                                                    preferHeight);
                resizeItem->applyImagePreviewScale(imageResizeCurrentScale_);
                imageResizeCurrentScale_ = resizeItem->currentImageResizeScale();
            } else {
                resizeItem->applyPreviewSize(targetSize, preferHeight);
            }
            notifyDebug(QStringLiteral("image resize move scale=%1 size=(%2,%3)")
                            .arg(imageResizeCurrentScale_, 0, 'f', 4)
                            .arg(resizeItem->currentPreviewSize().width(), 0, 'f', 1)
                            .arg(resizeItem->currentPreviewSize().height(), 0, 'f', 1));
            event->accept();
            return;
        }
        if (dragController_.active() && (event->buttons() & Qt::LeftButton)) {
            dragController_.update(mapToScene(event->pos()));
            event->accept();
            return;
        }
        if (panning_) {
            beginFastRendering();
            const QPoint delta = event->pos() - lastPanPoint_;
            lastPanPoint_ = event->pos();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            event->accept();
            return;
        }
        QGraphicsView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        notifyDebug(QStringLiteral("view mouse release button=%1 imageResize=%2")
                        .arg(static_cast<int>(event->button()))
                        .arg(imageResizePath_.isEmpty() ? QStringLiteral("(none)") : imageResizePath_));
        if (event->button() == Qt::LeftButton && !imageResizePath_.isEmpty()) {
            if (NodeItem* resizeItem = nodeItemForPath(imageResizePath_)) {
                if (isImagePreviewFile(QFileInfo(imageResizePath_))) {
                    resizeItem->saveImagePreviewScale(imageResizeCurrentScale_);
                } else {
                    const QPointF delta = mapToScene(event->pos()) - imageResizeStartScene_;
                    const bool preferHeight = std::abs(delta.y()) > std::abs(delta.x());
                    const QSizeF targetSize = previewResizeTargetSize(QFileInfo(imageResizePath_),
                                                                      imageResizeStartSize_, delta);
                    resizeItem->savePreviewSize(targetSize, preferHeight);
                }
            } else {
                notifyDebug(QStringLiteral("image resize finish canceled: item missing"));
            }
            imageResizePath_.clear();
            notifyDebug(QStringLiteral("image resize finish"));
            event->accept();
            return;
        }
        if (dragController_.active() && event->button() == Qt::LeftButton) {
            notifyDebug(QStringLiteral("view finalize node drag"));
            dragController_.finish(Qt::LeftButton, mapToScene(event->pos()));
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton && rubberBandSelecting_) {
            QGraphicsView::mouseReleaseEvent(event);
            rememberRubberBandRect(event->pos());
            applyRubberBandSelection(event->modifiers());
            setDragMode(QGraphicsView::NoDrag);
            rubberBandSelecting_ = false;
            return;
        }
        if (event->button() == panningButton_ && panning_) {
            panning_ = false;
            panningButton_ = Qt::NoButton;
            unsetCursor();
            notifyViewChanged();
            event->accept();
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        // The view owns the press, so QGraphicsView no longer delivers double-clicks to the
        // node. Drive the node's default action here. Pass through when the press is on a
        // child preview proxy so double-clicking preview text keeps its normal behaviour.
        if (event->button() == Qt::LeftButton) {
            if (NodeItem* nodeItem = nodeItemAt(event->pos())) {
                QGraphicsItem* hitItem = itemAt(event->pos());
                const bool onChildWidget = hitItem != nullptr && hitItem != nodeItem;
                if (!onChildWidget) {
                    nodeItem->activate();
                    event->accept();
                    return;
                }
            }
        }
        QGraphicsView::mouseDoubleClickEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (textInputWidgetHasFocus()) {
            QGraphicsView::keyPressEvent(event);
            return;
        }
        if (event->key() == Qt::Key_Plus) {
            zoomAt(viewport()->rect().center(), 1.12);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Minus) {
            zoomAt(viewport()->rect().center(), 0.89);
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
            lastRubberBandSceneRect_.isValid()) {
            zoomToRect(lastRubberBandSceneRect_);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Question && cheatSheetHandler_) {
            cheatSheetHandler_();
            event->accept();
            return;
        }
        if (keyHandler_ && keyHandler_(event)) {
            notifyDebug(QStringLiteral("board keyPress handled by node navigation: %1").arg(debugKeyName(event)));
            event->accept();
            return;
        }
        if (scrollWithArrowKey(event)) {
            event->accept();
            return;
        }
        QGraphicsView::keyPressEvent(event);
    }

private:
    NodeItem* nodeItemAt(const QPoint& pos) const
    {
        for (QGraphicsItem* item = itemAt(pos); item; item = item->parentItem()) {
            if (auto* nodeItem = dynamic_cast<NodeItem*>(item)) {
                return nodeItem;
            }
        }
        // QGraphicsView::itemAt() can miss a node (notably when a preview proxy widget is
        // present), which would make a file-name click look like an empty-canvas click. Fall
        // back to a direct geometry test against every visible node.
        if (!scene()) {
            return nullptr;
        }
        const QPointF scenePos = mapToScene(pos);
        for (QGraphicsItem* item : scene()->items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem || !nodeItem->isVisible()) {
                continue;
            }
            const QPointF localPos = nodeItem->mapFromScene(scenePos);
            if (nodeItem->contains(localPos) ||
                nodeItem->boundingRect().adjusted(-2.0, -2.0, 2.0, 2.0).contains(localPos)) {
                return nodeItem;
            }
        }
        return nullptr;
    }

    NodeItem* nodeItemForPath(const QString& path) const
    {
        if (!scene()) {
            return nullptr;
        }
        for (QGraphicsItem* item : scene()->items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->path() == path) {
                return nodeItem;
            }
        }
        return nullptr;
    }

    void rememberRubberBandRect(const QPoint& endPos)
    {
        const QRect viewportRect = QRect(rubberBandStart_, endPos).normalized();
        if (viewportRect.width() < 8 || viewportRect.height() < 8) {
            lastRubberBandSceneRect_ = QRectF();
            return;
        }

        lastRubberBandSceneRect_ = mapToScene(viewportRect).boundingRect();
    }

    void applyRubberBandSelection(Qt::KeyboardModifiers modifiers)
    {
        QGraphicsScene* currentScene = scene();
        if (!currentScene) {
            return;
        }

        if (!lastRubberBandSceneRect_.isValid()) {
            if (!(modifiers & Qt::ControlModifier)) {
                currentScene->clearSelection();
            }
            return;
        }

        if (!(modifiers & Qt::ControlModifier)) {
            currentScene->clearSelection();
        }

        for (QGraphicsItem* item :
             currentScene->items(lastRubberBandSceneRect_, Qt::IntersectsItemBoundingRect)) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem) {
                nodeItem->setSelected(true);
            }
        }
    }

    void zoomToRect(const QRectF& sceneRect)
    {
        if (!sceneRect.isValid() || sceneRect.width() <= 1.0 || sceneRect.height() <= 1.0) {
            return;
        }

        resetTransform();
        fitInView(sceneRect.adjusted(-40.0, -40.0, 40.0, 40.0), Qt::KeepAspectRatio);
        notifyViewChanged();
    }

    void zoomAt(const QPointF& viewportPos, qreal factor)
    {
        const qreal currentScale = transform().m11();
        const qreal targetScale = std::clamp(currentScale * factor, 0.15, 4.5);
        if (std::abs(targetScale - currentScale) < 0.0001) {
            return;
        }

        const QPointF sceneBefore = mapToScene(viewportPos.toPoint());
        const qreal appliedFactor = targetScale / currentScale;
        scale(appliedFactor, appliedFactor);
        const QPointF sceneAfter = mapToScene(viewportPos.toPoint());
        const QPointF delta = sceneAfter - sceneBefore;
        translate(delta.x(), delta.y());
        notifyViewChanged();
    }

    void notifyViewChanged()
    {
        if (viewChangedHandler_) {
            viewChangedHandler_();
        }
    }

    void beginFastRendering()
    {
        if (!g_fastCanvasRendering) {
            g_fastCanvasRendering = true;
            setRenderHints(QPainter::TextAntialiasing);
            viewport()->update();
        }
        fastRenderTimer_.start();
    }

    void endFastRendering()
    {
        if (!g_fastCanvasRendering) {
            return;
        }
        g_fastCanvasRendering = false;
        setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        viewport()->update();
    }

    void notifyDebug(const QString& message)
    {
        if (debugHandler_) {
            debugHandler_(message);
        }
    }

    bool scrollWithArrowKey(QKeyEvent* event)
    {
        if (!event || event->modifiers() != Qt::NoModifier || !isArrowKey(event->key())) {
            return false;
        }

        constexpr int scrollStep = 56;
        const int oldH = horizontalScrollBar()->value();
        const int oldV = verticalScrollBar()->value();
        switch (event->key()) {
        case Qt::Key_Up:
            beginFastRendering();
            verticalScrollBar()->setValue(verticalScrollBar()->value() - scrollStep);
            break;
        case Qt::Key_Down:
            beginFastRendering();
            verticalScrollBar()->setValue(verticalScrollBar()->value() + scrollStep);
            break;
        case Qt::Key_Left:
            beginFastRendering();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - scrollStep);
            break;
        case Qt::Key_Right:
            beginFastRendering();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() + scrollStep);
            break;
        default:
            return false;
        }
        const int newH = horizontalScrollBar()->value();
        const int newV = verticalScrollBar()->value();
        notifyDebug(QStringLiteral("arrow scroll %1: h %2 -> %3 / %4, v %5 -> %6 / %7%8")
                        .arg(debugKeyName(event))
                        .arg(oldH)
                        .arg(newH)
                        .arg(horizontalScrollBar()->maximum())
                        .arg(oldV)
                        .arg(newV)
                        .arg(verticalScrollBar()->maximum())
                        .arg(oldH == newH && oldV == newV ? QStringLiteral(" (unchanged)") : QString()));
        notifyViewChanged();
        return true;
    }

    static QString debugKeyName(QKeyEvent* event)
    {
        if (!event) {
            return QStringLiteral("(none)");
        }
        QString key = QKeySequence(event->key()).toString(QKeySequence::NativeText);
        if (key.isEmpty()) {
            key = QStringLiteral("key=%1").arg(event->key());
        }
        return key;
    }

    static bool isArrowKey(int key)
    {
        return key == Qt::Key_Up || key == Qt::Key_Down ||
               key == Qt::Key_Left || key == Qt::Key_Right;
    }

    static bool isBoardNavigationKey(QKeyEvent* event)
    {
        if (!event) {
            return false;
        }
        const int key = event->key();
        return isArrowKey(key) || key == Qt::Key_Tab || key == Qt::Key_Backtab ||
               key == Qt::Key_Return || key == Qt::Key_Enter;
    }

    bool panning_ = false;
    DragController dragController_;  // owns the in-progress node-drag session
    QString imageResizePath_;
    QPointF imageResizeStartScene_;
    QSizeF imageResizeStartSize_;
    qreal imageResizeStartScale_ = 1.0;
    qreal imageResizeCurrentScale_ = 1.0;
    bool rubberBandSelecting_ = false;
    Qt::MouseButton panningButton_ = Qt::NoButton;
    QPoint lastPanPoint_;
    QPoint rubberBandStart_;
    QRectF lastRubberBandSceneRect_;
    QTimer fastRenderTimer_;
    std::function<void()> cheatSheetHandler_;
    std::function<bool(QKeyEvent*)> keyHandler_;
    std::function<void()> viewChangedHandler_;
    std::function<void(const QString&)> debugHandler_;
};
