#pragma once

// Owns the single in-progress node-drag session. Extracted verbatim out of main.cpp.

#include <QtCore/QPointF>

#include "node_item.h"

// Owns the single in-progress node-drag session. BoardView routes its press/move/release
// here so there is one place that knows which node is being dragged; the per-node motion
// and drop logic still lives on NodeItem (beginDrag/updateDrag/finishDrag).
class DragController {
public:
    bool active() const { return node_ != nullptr; }

    void begin(NodeItem* node, const QPointF& scenePos, Qt::KeyboardModifiers modifiers)
    {
        node_ = node;
        if (node_) {
            node_->beginDrag(scenePos, modifiers);
        }
    }

    void update(const QPointF& scenePos)
    {
        if (node_) {
            node_->updateDrag(scenePos);
        }
    }

    void finish(Qt::MouseButton button, const QPointF& scenePos)
    {
        NodeItem* node = node_;
        node_ = nullptr;  // cleared first: finishDrag may rebuild and delete the item
        if (node) {
            node->finishDrag(button, scenePos);
        }
    }

    // Drop the session without finishing it — used when a rebuild is about to delete the
    // dragged item, so the pointer never dangles.
    void cancel() { node_ = nullptr; }

private:
    NodeItem* node_ = nullptr;
};
