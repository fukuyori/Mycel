#pragma once

// Pure hit-testing over the scene for drag-and-drop: what folder a drop would move into, or
// what file it would link beside. Extracted verbatim out of main.cpp.

#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsScene>

#include <vector>

#include "mycel_fileops.hpp"
#include "node_item.h"

// Decides what a drag would drop onto (a folder to move into, or a file to link to)
// from the scene contents and the pointer position. Pure hit-testing over the scene;
// it holds no drag state and performs no mutation, so the gesture orchestration in
// BoardView / NodeItem only has to ask "what is under here" rather than reimplement it.
class DropTargetResolver {
public:
    // The folder node the dragged items would move into, or nullptr. dragItems are the
    // items being moved (excluded as candidates, along with their descendants).
    static NodeItem* folderTarget(const QGraphicsScene& scene, const NodeItem* source,
                                  const std::vector<NodeItem*>& dragItems, const QPointF& scenePos)
    {
        if (!source) {
            return nullptr;
        }

        const auto isMovingItem = [&dragItems](const NodeItem* item) {
            if (!item || !item->node()) {
                return false;
            }
            for (const NodeItem* dragItem : dragItems) {
                if (!dragItem || !dragItem->node()) {
                    continue;
                }
                if (item == dragItem || isDescendantPath(dragItem->node()->path, item->node()->path)) {
                    return true;
                }
            }
            return false;
        };

        constexpr qreal FolderDropIntentMargin = 28.0;
        for (QGraphicsItem* item : scene.items(scenePos)) {
            auto* candidate = dynamic_cast<NodeItem*>(item);
            if (!candidate || candidate == source || !candidate->node()->isDir) {
                continue;
            }
            if (!isMovingItem(candidate)) {
                return candidate;
            }
        }

        const QRectF dropRect = source->sceneBoundingRect().adjusted(-10.0, -10.0, 10.0, 10.0);
        NodeItem* best = nullptr;
        qreal bestOverlap = 0.0;
        const QRectF intentRect = dropRect.united(QRectF(scenePos, QSizeF(1.0, 1.0))
                                                      .adjusted(-FolderDropIntentMargin, -FolderDropIntentMargin,
                                                                FolderDropIntentMargin, FolderDropIntentMargin));
        for (QGraphicsItem* item : scene.items(intentRect, Qt::IntersectsItemBoundingRect)) {
            auto* candidate = dynamic_cast<NodeItem*>(item);
            if (!candidate || candidate == source || !candidate->node()->isDir) {
                continue;
            }
            if (isMovingItem(candidate)) {
                continue;
            }

            const QRectF overlap = dropRect.intersected(candidate->sceneBoundingRect());
            qreal area = overlap.width() * overlap.height();
            if (candidate->sceneBoundingRect().adjusted(-FolderDropIntentMargin, -FolderDropIntentMargin,
                                                        FolderDropIntentMargin, FolderDropIntentMargin)
                    .contains(scenePos)) {
                area += 1.0e9;
            }
            if (area > bestOverlap) {
                bestOverlap = area;
                best = candidate;
            }
        }
        return best;
    }

    // The file node the dragged item would be linked beside, or nullptr. Links require metadata
    // storage, a single dragged source (file or folder), and a non-directory link target (the file
    // whose right edge is hovered). The dragged item is placed to the right of that file.
    static NodeItem* linkTarget(const QGraphicsScene& scene, const NodeItem* source,
                                bool multiDrag, bool mycelStorageEnabled, const QPointF& scenePos)
    {
        constexpr qreal LinkDropIntentMargin = 72.0;
        if (!mycelStorageEnabled || !source || !source->node() || multiDrag) {
            return nullptr;
        }

        for (QGraphicsItem* item : scene.items()) {
            auto* candidate = dynamic_cast<NodeItem*>(item);
            if (!candidate || candidate == source || !candidate->node() || candidate->node()->isDir) {
                continue;  // the link anchor (right-edge target) is always a file
            }
            if (candidate->containsLinkDropScenePoint(scenePos, LinkDropIntentMargin)) {
                return candidate;
            }
        }
        return nullptr;
    }
};
