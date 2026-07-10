#include "main_window.h"

void MainWindow::ensureNodeItemVisible(NodeItem* item)
{
        if (!item || !view_) {
            return;
        }
        const QRectF rect = item->sceneBoundingRect().adjusted(-36.0, -36.0, 36.0, 36.0);
        view_->ensureVisible(rect, 80, 80);
        scheduleViewStateSave();
    }

void MainWindow::ensureNodePathVisible(const QString& path)
{
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node() && nodeItem->node()->path == path) {
                ensureNodeItemVisible(nodeItem);
                return;
            }
        }
    }

Node* MainWindow::findVisibleNodeByPath(Node* node, const QString& path) const
{
        if (!node) {
            return nullptr;
        }
        if (node->path == path) {
            return node;
        }
        for (const auto& child : node->children) {
            if (Node* found = findVisibleNodeByPath(child.get(), path)) {
                return found;
            }
        }
        return nullptr;
    }

QStringList MainWindow::visibleNodePathsInOrder() const
{
        QStringList paths;
        if (!root_) {
            return paths;
        }
        std::function<void(const Node&)> visit = [&](const Node& node) {
            paths.append(node.path);
            for (const auto& child : node.children) {
                visit(*child);
            }
        };
        visit(*root_);
        return paths;
    }

void MainWindow::setVisualPosition(NodeItem* item, const QPointF& position)
{
        if (!item) {
            return;
        }

        item->setPos(position);
        if (!item->node()->isDir) {
            return;
        }

        const QPointF delta = position - item->layoutCenter();
        for (QGraphicsItem* sceneItem : scene_.items()) {
            auto* descendant = dynamic_cast<NodeItem*>(sceneItem);
            if (!descendant || descendant == item) {
                continue;
            }
            if (isDescendantPath(item->node()->path, descendant->node()->path)) {
                descendant->setPos(descendant->layoutCenter() + delta);
            }
        }
    }

void MainWindow::restackNodeItemsByVisualOrder()
{
        if (!root_) {
            return;
        }
        std::vector<Node*> nodesByVisualOrder;
        visitNodes(*root_, [&nodesByVisualOrder](Node& node) {
            nodesByVisualOrder.push_back(&node);
        });
        std::stable_sort(nodesByVisualOrder.begin(), nodesByVisualOrder.end(), [](const Node* a, const Node* b) {
            return a->center.y() < b->center.y();
        });
        NodeItem* previous = nullptr;
        for (Node* node : nodesByVisualOrder) {
            NodeItem* item = nodeItemsByPath_.value(node->path, nullptr);
            if (!item) {
                continue;
            }
            if (previous) {
                previous->stackBefore(item);
            }
            previous = item;
        }
    }

void MainWindow::renderCurrentTree(bool fitAfterRender)
{
        setWindowTitle(windowTitleForRoot(rootPath_, mycelStorageEnabled_));

        const QTransform previousTransform = view_->transform();
        const int previousHScroll = view_->horizontalScrollBar()->value();
        const int previousVScroll = view_->verticalScrollBar()->value();

        const bool previousSuppressSelectionUpdate = suppressSideEditorSelectionUpdate_;
        suppressSideEditorSelectionUpdate_ = true;
        resetDropHoverState();
        if (view_) {
            view_->cancelTransientNodeInteraction();
        }
        nodeItemsByPath_.clear();  // items are about to be destroyed with the scene
        connectionLayer_ = nullptr;
        scene_.clear();
        if (!root_) {
            suppressSideEditorSelectionUpdate_ = previousSuppressSelectionUpdate;
            resetFileSystemWatcher();
            return;
        }
        assignTopLevelBranches(*root_);

        qreal yCursor = 0.0;
        const QSet<QString> linkedTargets = TreeLayoutEngine::linkedTargetPaths(fileLinks_);
        TreeLayoutEngine::layoutTree(*root_, 0.0, yCursor, linkedTargets, *root_, fileLinks_);
        TreeLayoutEngine::layoutFileLinks(*root_, fileLinks_);
        TreeLayoutEngine::translateTree(*root_, QPointF(140.0, 120.0 - root_->center.y()));
        TreeLayoutEngine::updateSubtreeBounds(*root_, linkedTargets);

        connectionLayer_ = new ConnectionLayerItem(root_.get(), &fileLinks_);
        scene_.addItem(connectionLayer_);
        visitNodes(*root_, [this](Node& node) {
            auto* item = new NodeItem(&node, this);
            scene_.addItem(item);
            nodeItemsByPath_.insert(node.path, item);
        });
        restackNodeItemsByVisualOrder();
        suppressSideEditorSelectionUpdate_ = previousSuppressSelectionUpdate;

        QRectF bounds = root_->subtreeBounds;
        bounds = bounds.united(addParentRootItems());

        scene_.setSceneRect(bounds.adjusted(-FreeCanvasMargin, -FreeCanvasMargin,
                                            FreeCanvasMargin, FreeCanvasMargin));
        if (fitAfterRender) {
            scheduleRestoreViewStateOrFit();
        } else {
            view_->setTransform(previousTransform);
            view_->horizontalScrollBar()->setValue(previousHScroll);
            view_->verticalScrollBar()->setValue(previousVScroll);
        }
        resetFileSystemWatcher();
    }

QRectF MainWindow::addParentRootItems()
{
        parentRootItemsBounds_ = QRectF();
        const QStringList parents = parentRootChain();  // [outermost ... immediate parent]
        if (parents.isEmpty() || !root_) {
            return {};
        }
        const qreal centerY = root_->center.y();
        const qreal gap = 90.0;
        QPointF connectTo(root_->center.x() - root_->size.width() / 2.0, centerY);  // left edge to join
        QRectF combined;
        for (int i = parents.size() - 1; i >= 0; --i) {
            const QString dir = parents.at(i);
            auto* item = new ParentRootItem(dir, QFileInfo(dir).fileName());
            item->setActivateHandler([this](const QString& path) { openRootFolder(path); });
            const qreal halfW = item->boxWidth() / 2.0;
            const qreal centerX = connectTo.x() - gap - halfW;
            item->setPos(QPointF(centerX, centerY));
            scene_.addItem(item);
            combined = combined.united(item->mapRectToScene(item->boundingRect()));

            QColor color = neutralStroke();
            color.setAlpha(connectorLineAlpha());
            auto* edge = new QGraphicsPathItem(edgePathBetweenPoints(QPointF(centerX + halfW, centerY), connectTo));
            edge->setPen(QPen(color, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            edge->setZValue(ConnectionLayerZ);
            scene_.addItem(edge);

            connectTo = QPointF(centerX - halfW, centerY);
        }
        parentRootItemsBounds_ = combined;
        return combined;
    }

void MainWindow::rebuild(bool fitAfterRebuild)
{
        saveSideEditorNow();
        finishInlineRename(false);
        root_ = scanTree(rootPath_, 0, -1, collapsedPaths_, previewPaths_, previewSizes_, fileOrders_, rootPath_,
                         mycelStorageEnabled_);
        injectExternalRootNodes();
        if (boardMode_) {
            renderBoardScene(fitAfterRebuild);
            return;
        }
        renderCurrentTree(fitAfterRebuild);
    }

void MainWindow::injectExternalRootNodes()
{
        if (!mycelStorageEnabled_ || !root_ || externalRootLinks_.empty()) {
            return;
        }
        std::vector<Node*> touchedParents;
        for (const ExternalRootLink& link : externalRootLinks_) {
            const QString dirPath = link.dirKey.isEmpty()
                                        ? QDir::cleanPath(rootPath_)
                                        : QDir::cleanPath(QDir(rootPath_).absoluteFilePath(link.dirKey));
            Node* parent = findVisibleNodeByPath(root_.get(), dirPath);
            if (!parent || !parent->isDir || parent->isSubRoot) {
                continue;  // the containing folder is hidden, gone, or itself a boundary
            }
            const QString target = resolvedExternalRootTarget(link);
            if (target.isEmpty() || !QFileInfo(target).isDir()) {
                // Broken link: keep the record but show nothing until the target reappears.
                recordDebugEvent(QStringLiteral("external root link target missing: %1").arg(link.target));
                continue;
            }
            if (parent->collapsed) {
                ++parent->hiddenChildren;  // count the door in the collapsed badge
                continue;
            }
            parent->children.push_back(makeExternalRootDoorNode(target, *parent));
            touchedParents.push_back(parent);
        }
        // Re-apply the per-directory manual order so door nodes sort like normal siblings.
        for (Node* parent : touchedParents) {
            const auto order = fileOrders_.find(orderKeyForDirectory(parent->path));
            if (order != fileOrders_.end()) {
                reorderChildrenInPlace(*parent, order->second);
            }
        }
    }

void MainWindow::refreshCachedNodeMetadata(Node& node)
{
        node.previewOpen = !node.isDir && previewPaths_.contains(node.path);
        if (node.previewOpen) {
            const auto found = previewSizes_.find(node.path);
            node.previewSize = found == previewSizes_.end() ? automaticPreviewSize(QFileInfo(node.path))
                                                            : found->second;
        }
        for (auto& child : node.children) {
            refreshCachedNodeMetadata(*child);
        }
    }

void MainWindow::relayout()
{
        if (boardMode_) {
            refreshBoardCards();  // positions are user-owned; only card metadata changes
            return;
        }
        if (!root_) {
            rebuild(false);
            return;
        }
        saveSideEditorNow();
        finishInlineRename(false);
        refreshCachedNodeMetadata(*root_);

        assignTopLevelBranches(*root_);
        qreal yCursor = 0.0;
        const QSet<QString> linkedTargets = TreeLayoutEngine::linkedTargetPaths(fileLinks_);
        TreeLayoutEngine::layoutTree(*root_, 0.0, yCursor, linkedTargets, *root_, fileLinks_);
        TreeLayoutEngine::layoutFileLinks(*root_, fileLinks_);
        TreeLayoutEngine::translateTree(*root_, QPointF(140.0, 120.0 - root_->center.y()));
        TreeLayoutEngine::updateSubtreeBounds(*root_, linkedTargets);

        bool allItemsFound = true;
        visitNodes(*root_, [&](Node& node) {
            if (NodeItem* item = nodeItemsByPath_.value(node.path, nullptr)) {
                item->syncFromNode();
            } else {
                allItemsFound = false;
            }
        });
        if (!allItemsFound || !connectionLayer_) {
            renderCurrentTree(false);
            return;
        }
        restackNodeItemsByVisualOrder();
        connectionLayer_->refreshGeometry();

        const QRectF bounds = root_->subtreeBounds.united(parentRootItemsBounds_);
        scene_.setSceneRect(bounds.adjusted(-FreeCanvasMargin, -FreeCanvasMargin,
                                            FreeCanvasMargin, FreeCanvasMargin));
        scene_.update();
    }

