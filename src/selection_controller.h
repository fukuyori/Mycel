#pragma once

// Owns every mutation of the scene's node selection. Extracted verbatim out of main.cpp.

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsScene>

#include "node_item.h"

// Owns every mutation of the scene's node selection so the rule "what becomes selected"
// lives in one place instead of being duplicated across NodeItem and MainWindow. View
// concerns (focus, scrolling a node into view) stay in MainWindow.
class SelectionController {
public:
    explicit SelectionController(QGraphicsScene& scene) : scene_(scene) {}

    void clear() { scene_.clearSelection(); }

    // Make `item` the selection. additive keeps the current selection (Cmd/Ctrl/Shift-click).
    void select(NodeItem* item, bool additive = false)
    {
        if (!additive) {
            scene_.clearSelection();
        }
        if (item) {
            item->setSelected(true);
        }
    }

    // Select the single node with this path (clearing others). Returns it, or nullptr.
    NodeItem* selectByPath(const QString& path)
    {
        scene_.clearSelection();
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node() && nodeItem->node()->path == path) {
                nodeItem->setSelected(true);
                return nodeItem;
            }
        }
        return nullptr;
    }

    // Select every node whose path is in `paths`. Returns the first selected, or nullptr.
    NodeItem* selectByPaths(const QStringList& paths, bool additive = false)
    {
        if (!additive) {
            scene_.clearSelection();
        }
        NodeItem* first = nullptr;
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node() && paths.contains(nodeItem->node()->path)) {
                nodeItem->setSelected(true);
                if (!first) {
                    first = nodeItem;
                }
            }
        }
        return first;
    }

    // Select every visible node. Returns true if anything was selected.
    bool selectAll()
    {
        scene_.clearSelection();
        bool any = false;
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node()) {
                nodeItem->setSelected(true);
                any = true;
            }
        }
        return any;
    }

    QStringList selectedPaths() const
    {
        QStringList paths;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node()) {
                paths.append(nodeItem->node()->path);
            }
        }
        paths.removeDuplicates();
        return paths;
    }

private:
    QGraphicsScene& scene_;
};
