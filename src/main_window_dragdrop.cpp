#include "main_window.h"

bool MainWindow::copySelectedItems()
{
        QStringList paths;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node() && nodeItem->node() != root_.get()) {
                paths.append(nodeItem->node()->path);
            }
        }
        paths.removeDuplicates();

        QStringList topLevelPaths;
        for (const QString& path : paths) {
            bool containedBySelectedFolder = false;
            for (const QString& otherPath : paths) {
                if (otherPath == path || !QFileInfo(otherPath).isDir()) {
                    continue;
                }
                if (isDescendantPath(otherPath, path)) {
                    containedBySelectedFolder = true;
                    break;
                }
            }
            if (!containedBySelectedFolder) {
                topLevelPaths.append(path);
            }
        }

        if (topLevelPaths.isEmpty()) {
            return false;
        }
        copiedPaths_ = topLevelPaths;
        return true;
    }

bool MainWindow::copyNode(Node* node)
{
        if (!node || node == root_.get()) {
            return false;
        }
        copiedPaths_ = {node->path};
        return true;
    }

bool MainWindow::copyPath(const QString& path)
{
        Node* node = nodeForPath(path);
        if (!node || node == root_.get()) {
            return false;
        }
        copiedPaths_ = {node->path};
        return true;
    }

bool MainWindow::pasteCopiedItems()
{
        if (copiedPaths_.isEmpty()) {
            return false;
        }

        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        std::vector<std::pair<QString, QString>> redoMoves;
        std::vector<std::pair<QString, QString>> undoMoves;

        QStringList createdDirectories;
        QStringList failed;
        for (const QString& sourcePath : copiedPaths_) {
            QFileInfo sourceInfo(sourcePath);
            if (!sourceInfo.exists()) {
                failed.append(sourcePath);
                continue;
            }

            const QString parentPath = sourceInfo.absolutePath();
            const QString destinationPath = availableImportPath(parentPath, sourceInfo.fileName(), sourceInfo.isDir());
            const bool copied = sourceInfo.isDir()
                                    ? copyDirectoryRecursively(sourceInfo.absoluteFilePath(), destinationPath)
                                    : QFile::copy(sourceInfo.absoluteFilePath(), destinationPath);
            if (!copied) {
                failed.append(sourceInfo.absoluteFilePath());
                continue;
            }

            QFileInfo destinationInfo(destinationPath);
            appendCreatedItemToOrder(parentPath, destinationInfo.fileName(), destinationInfo.isDir());
            const QString trashPath = allocateTrashPath(destinationInfo.fileName());
            redoMoves.push_back({trashPath, destinationPath});
            undoMoves.push_back({destinationPath, trashPath});
            if (destinationInfo.isDir()) {
                createdDirectories.append(destinationInfo.absoluteFilePath());
            }
        }

        if (!createdDirectories.isEmpty()) {
            for (const QString& path : createdDirectories) {
                collapsedPaths_.insert(path);
            }
        }
        saveOrderFile();
        rebuild(false);
        if (!redoMoves.empty()) {
            recordHistory(QStringLiteral("貼り付け"), std::move(redoMoves), std::move(undoMoves),
                          historyBefore, historySelection);
        }

        if (!failed.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 QStringLiteral("コピーできなかった項目があります。\n%1").arg(failed.join(QLatin1Char('\n'))));
        }
        return true;
    }

void MainWindow::moveNode(Node* source, Node* targetDir)
{
        const QString sourcePath = source ? source->path : QString();
        const bool sourceIsDir = source && source->isDir;
        const bool sourceIsRoot = source == root_.get();
        const QString targetDirPath = targetDir ? targetDir->path : QString();
        const bool targetIsDir = targetDir && targetDir->isDir;
        recordDebugEvent(QStringLiteral("move node begin: source=%1 isDir=%2 isRoot=%3 target=%4 targetIsDir=%5")
                             .arg(relativeKeyForPath(sourcePath),
                                  sourceIsDir ? QStringLiteral("true") : QStringLiteral("false"),
                                  sourceIsRoot ? QStringLiteral("true") : QStringLiteral("false"),
                                  relativeKeyForPath(targetDirPath),
                                  targetIsDir ? QStringLiteral("true") : QStringLiteral("false")));

        if (sourcePath.isEmpty() || targetDirPath.isEmpty() || !targetIsDir || sourceIsRoot) {
            recordDebugEvent(QStringLiteral("move node aborted: invalid source or target"));
            rebuild(false);
            return;
        }

        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();

        // A link source moving to a different folder takes its whole chain of link targets
        // along, so the linked group stays physically together. Skipped when the source already
        // lives in the target directory: that drop is the unlink gesture, not a move.
        std::vector<FileOperationService::MoveRequest> requests;
        requests.push_back({sourcePath, sourceIsDir, sourceIsRoot});
        if (QDir::cleanPath(QFileInfo(sourcePath).absolutePath()) != QDir::cleanPath(targetDirPath)) {
            for (const QString& linkedPath : linkedDescendantPaths(sourcePath)) {
                const QFileInfo linkedInfo(linkedPath);
                if (linkedInfo.exists()) {
                    requests.push_back({linkedPath, linkedInfo.isDir(), false});
                    recordDebugEvent(QStringLiteral("move node linked target follows: %1")
                                         .arg(relativeKeyForPath(linkedPath)));
                }
            }
        }

        pauseFileSystemWatcher();
        const FileOperationService::MoveResult result = FileOperationService::moveInto(requests, targetDirPath);
        recordDebugEvent(QStringLiteral("move node fs result: moved=%1 failed=%2 blocked=%3 sameDir=%4")
                             .arg(static_cast<int>(result.moved.size()))
                             .arg(static_cast<int>(result.failed.size()))
                             .arg(result.blocked ? QStringLiteral("true") : QStringLiteral("false"),
                                  result.skippedSameDir ? QStringLiteral("true") : QStringLiteral("false")));

        if (result.blocked) {
            recordDebugEvent(QStringLiteral("move node blocked"));
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("自分自身または配下へは移動できません。"));
            rebuild(false);
            return;
        }
        if (!result.failed.empty()) {
            recordDebugEvent(QStringLiteral("move node failed: %1 -> %2")
                                 .arg(relativeKeyForPath(result.failed.front().first), result.failed.front().second));
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 QStringLiteral("移動できませんでした。\n%1").arg(result.failed.front().second));
            rebuild(false);
            return;
        }
        if (result.moved.empty()) {
            // source already lives in the target directory. Dropping a linked node onto its
            // real parent folder is how the user removes the link, so do that instead of nothing.
            clearDragPreview();
            if (hasIncomingFileLinkPath(sourcePath)) {
                removeIncomingFileLinks(source);
                return;
            }
            rebuild(false);
            return;
        }

        const bool multiMove = result.moved.size() > 1;
        for (const FileOperationService::MovedEntry& entry : result.moved) {
            recordDebugEvent(QStringLiteral("move node metadata: %1 -> %2")
                                 .arg(relativeKeyForPath(entry.oldPath), relativeKeyForPath(entry.newPath)));
            applyMovedMetadata(entry, targetDirPath, mycelStorageEnabled_ && (multiMove || !entry.isDir));
        }
        saveColorFile();
        savePreviewFile();
        saveLinkFile();
        saveCollapsedFile();
        saveHashCacheFile();
        if (mycelStorageEnabled_) {
            saveOrderFile();
        }
        rebuild(false);
        std::vector<std::pair<QString, QString>> redoMoves;
        std::vector<std::pair<QString, QString>> undoMoves;
        for (const FileOperationService::MovedEntry& entry : result.moved) {
            redoMoves.push_back({entry.oldPath, entry.newPath});
            undoMoves.push_back({entry.newPath, entry.oldPath});
        }
        recordHistory(QStringLiteral("移動"), std::move(redoMoves), std::move(undoMoves),
                      historyBefore, historySelection);
    }

void MainWindow::moveDragItemsToFolder(NodeItem* sourceItem, Node* targetDir)
{
        const QString targetDirPath = targetDir ? targetDir->path : QString();
        const bool targetIsDir = targetDir && targetDir->isDir;
        std::vector<NodeItem*> dragItems = selectedTopLevelDragItems(sourceItem);
        recordDebugEvent(QStringLiteral("move drag begin: source=%1 target=%2 targetIsDir=%3 dragItems=%4")
                             .arg(sourceItem ? relativeKeyForPath(sourceItem->path()) : QStringLiteral("(null)"),
                                  relativeKeyForPath(targetDirPath),
                                  targetIsDir ? QStringLiteral("true") : QStringLiteral("false"))
                             .arg(static_cast<int>(dragItems.size())));
        if (dragItems.size() <= 1) {
            moveNode(sourceItem ? sourceItem->node() : nullptr, targetDir);
            return;
        }
        if (targetDirPath.isEmpty() || !targetIsDir) {
            recordDebugEvent(QStringLiteral("move drag aborted: invalid target"));
            clearDragPreview();
            return;
        }

        std::vector<FileOperationService::MoveRequest> requests;
        QSet<QString> requestedPaths;
        for (NodeItem* item : dragItems) {
            Node* node = item ? item->node() : nullptr;
            if (!node || node == root_.get() || requestedPaths.contains(node->path)) {
                continue;
            }
            requestedPaths.insert(node->path);
            requests.push_back({node->path, node->isDir, false});
            recordDebugEvent(QStringLiteral("move drag request: %1 isDir=%2")
                                 .arg(relativeKeyForPath(node->path),
                                      node->isDir ? QStringLiteral("true") : QStringLiteral("false")));
            // A link source moving to a different folder takes its chain of link targets along
            // (same rule as the single-item move). Same-dir drops keep their unlink semantics.
            if (QDir::cleanPath(QFileInfo(node->path).absolutePath()) == QDir::cleanPath(targetDirPath)) {
                continue;
            }
            for (const QString& linkedPath : linkedDescendantPaths(node->path)) {
                const QFileInfo linkedInfo(linkedPath);
                if (!linkedInfo.exists() || requestedPaths.contains(linkedPath)) {
                    continue;
                }
                requestedPaths.insert(linkedPath);
                requests.push_back({linkedPath, linkedInfo.isDir(), false});
                recordDebugEvent(QStringLiteral("move drag linked target follows: %1")
                                     .arg(relativeKeyForPath(linkedPath)));
            }
        }

        if (requests.empty()) {
            recordDebugEvent(QStringLiteral("move drag aborted: no requests"));
            clearDragPreview();
            return;
        }

        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();

        pauseFileSystemWatcher();
        const FileOperationService::MoveResult result = FileOperationService::moveInto(requests, targetDirPath);
        recordDebugEvent(QStringLiteral("move drag fs result: moved=%1 failed=%2 blocked=%3 sameDir=%4")
                             .arg(static_cast<int>(result.moved.size()))
                             .arg(static_cast<int>(result.failed.size()))
                             .arg(result.blocked ? QStringLiteral("true") : QStringLiteral("false"),
                                  result.skippedSameDir ? QStringLiteral("true") : QStringLiteral("false")));

        if (result.blocked) {
            recordDebugEvent(QStringLiteral("move drag blocked"));
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 QStringLiteral("自分自身または配下へは移動できません。"));
            rebuild(false);
            return;
        }

        for (const FileOperationService::MovedEntry& entry : result.moved) {
            recordDebugEvent(QStringLiteral("move drag metadata: %1 -> %2")
                                 .arg(relativeKeyForPath(entry.oldPath), relativeKeyForPath(entry.newPath)));
            applyMovedMetadata(entry, targetDirPath, mycelStorageEnabled_);
        }

        // Items that already lived in the target directory are skipped by moveInto and never
        // appear in result.moved. Dropping a linked item onto its own real parent folder is how
        // the user removes the link, so treat that case the same as the single-item path.
        QSet<QString> movedOldPaths;
        for (const FileOperationService::MovedEntry& entry : result.moved) {
            movedOldPaths.insert(entry.oldPath);
        }
        std::vector<Node*> unlinkTargets;
        for (NodeItem* item : dragItems) {
            Node* node = item ? item->node() : nullptr;
            if (!node || node == root_.get() || movedOldPaths.contains(node->path)) {
                continue;
            }
            if (QDir::cleanPath(QFileInfo(node->path).absolutePath()) == QDir::cleanPath(targetDirPath) &&
                hasIncomingFileLinkPath(node->path)) {
                unlinkTargets.push_back(node);
            }
        }
        for (Node* node : unlinkTargets) {
            fileLinks_.erase(std::remove_if(fileLinks_.begin(), fileLinks_.end(),
                                            [node](const FileLink& link) { return link.to == node->path; }),
                             fileLinks_.end());
        }

        recordDebugEvent(QStringLiteral("move drag saving metadata"));
        saveColorFile();
        savePreviewFile();
        saveOrderFile();
        saveLinkFile();
        saveCollapsedFile();
        saveHashCacheFile();
        recordDebugEvent(QStringLiteral("move drag rebuild"));
        rebuild(false);

        std::vector<std::pair<QString, QString>> redoMoves;
        std::vector<std::pair<QString, QString>> undoMoves;
        for (const FileOperationService::MovedEntry& entry : result.moved) {
            redoMoves.push_back({entry.oldPath, entry.newPath});
            undoMoves.push_back({entry.newPath, entry.oldPath});
        }
        if (!redoMoves.empty() || !unlinkTargets.empty()) {
            const QString description = redoMoves.empty() ? QStringLiteral("関連付け解除") : QStringLiteral("移動");
            recordHistory(description, std::move(redoMoves), std::move(undoMoves),
                          historyBefore, historySelection);
        }

        if (!result.failed.empty()) {
            QStringList failed;
            for (const auto& [path, message] : result.failed) {
                failed.append(QStringLiteral("%1\n%2").arg(path, message));
            }
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 QStringLiteral("移動できなかった項目があります。\n%1").arg(failed.join(QLatin1Char('\n'))));
        }
    }

bool MainWindow::canImportExternalUrls(const QMimeData* mimeData, Node* targetDir) const
{
        if (!mimeData || !targetDir || !targetDir->isDir || !mimeData->hasUrls()) {
            return false;
        }

        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile() && QFileInfo::exists(url.toLocalFile())) {
                return true;
            }
        }
        return false;
    }

void MainWindow::importExternalUrlsToFolder(const QList<QUrl>& urls, Node* targetDir)
{
        if (!targetDir || !targetDir->isDir) {
            return;
        }

        QDir target(targetDir->path);
        QStringList importedNames;
        QStringList failedPaths;
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        std::vector<std::pair<QString, QString>> redoMoves;
        std::vector<std::pair<QString, QString>> undoMoves;

        for (const QUrl& url : urls) {
            if (!url.isLocalFile()) {
                continue;
            }

            const QString sourcePath = url.toLocalFile();
            QFileInfo sourceInfo(sourcePath);
            if (!sourceInfo.exists()) {
                failedPaths.append(sourcePath);
                continue;
            }
            if (sourceInfo.isDir() && (sourceInfo.absoluteFilePath() == targetDir->path ||
                                       isDescendantPath(sourceInfo.absoluteFilePath(), targetDir->path))) {
                failedPaths.append(sourceInfo.absoluteFilePath());
                continue;
            }

            const QString destinationPath = availableImportPath(targetDir->path, sourceInfo.fileName(), sourceInfo.isDir());
            const bool copied = sourceInfo.isDir()
                                    ? copyDirectoryRecursively(sourceInfo.absoluteFilePath(), destinationPath)
                                    : QFile::copy(sourceInfo.absoluteFilePath(), destinationPath);
            if (!copied) {
                failedPaths.append(sourceInfo.absoluteFilePath());
                continue;
            }

            QFileInfo destinationInfo(destinationPath);
            importedNames.append(destinationInfo.fileName());
            appendCreatedItemToOrder(targetDir->path, destinationInfo.fileName(), destinationInfo.isDir());
            const QString trashPath = allocateTrashPath(destinationInfo.fileName());
            redoMoves.push_back({trashPath, destinationPath});
            undoMoves.push_back({destinationPath, trashPath});
        }

        if (!importedNames.isEmpty()) {
            saveOrderFile();
            rebuild(false);
            recordHistory(QStringLiteral("取り込み"), std::move(redoMoves), std::move(undoMoves),
                          historyBefore, historySelection);
        }

        if (!failedPaths.isEmpty()) {
            QMessageBox::warning(
                this, QStringLiteral("Mycel"),
                QStringLiteral("取り込めなかった項目があります。\n%1").arg(failedPaths.join(QLatin1Char('\n'))));
        }
    }

bool MainWindow::hasPasteableWebUrl(const QMimeData* mimeData) const
{
        if (!mimeData || !mimeData->hasUrls()) {
            return false;
        }
        for (const QUrl& url : mimeData->urls()) {
            if (!url.isLocalFile() &&
                (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https"))) {
                return true;
            }
        }
        return false;
    }

bool MainWindow::canPasteClipboardToFolder(Node* targetDir) const
{
        if (!targetDir || !targetDir->isDir) {
            return false;
        }

        const QClipboard* clipboard = QApplication::clipboard();
        const QMimeData* mimeData = clipboard ? clipboard->mimeData() : nullptr;
        if (!mimeData) {
            return false;
        }

        return canImportExternalUrls(mimeData, targetDir) || hasPasteableWebUrl(mimeData) ||
               mimeData->hasImage() || mimeData->hasHtml() || mimeData->hasText() ||
               pasteableBinaryFormat(mimeData).has_value();
    }

bool MainWindow::canPasteClipboardToFolderPath(const QString& folderPath) const
{
        const QFileInfo folderInfo(folderPath);
        if (!folderInfo.exists() || !folderInfo.isDir()) {
            return false;
        }

        const QClipboard* clipboard = QApplication::clipboard();
        const QMimeData* mimeData = clipboard ? clipboard->mimeData() : nullptr;
        if (!mimeData) {
            return false;
        }

        return mimeData->hasUrls() || mimeData->hasImage() || mimeData->hasHtml() ||
               mimeData->hasText() || pasteableBinaryFormat(mimeData).has_value();
    }

void MainWindow::pasteClipboardToFolder(Node* targetDir)
{
        if (!targetDir || !targetDir->isDir) {
            return;
        }

        const QClipboard* clipboard = QApplication::clipboard();
        const QMimeData* mimeData = clipboard ? clipboard->mimeData() : nullptr;
        if (!mimeData) {
            return;
        }

        if (canImportExternalUrls(mimeData, targetDir)) {
            importExternalUrlsToFolder(mimeData->urls(), targetDir);
            return;
        }

        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        QString destinationPath;
        bool saved = false;
        if (mimeData->hasUrls()) {
            for (const QUrl& url : mimeData->urls()) {
                if (!url.isLocalFile() &&
                    (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https"))) {
                    destinationPath = availableImportPath(targetDir->path, urlShortcutFileName(url), false);
                    saved = writeBytes(destinationPath, urlShortcutBytes(url));
                    break;
                }
            }
        }
        if (mimeData->hasText()) {
            if (!saved) {
                if (const auto embedUrl = youtubeEmbedUrlFromText(mimeData->text())) {
                    const QString videoId = youtubeVideoIdFromUrl(QUrl(*embedUrl));
                    const QString preferredName = videoId.isEmpty()
                                                      ? QStringLiteral("YouTube.url")
                                                      : QStringLiteral("YouTube %1.url").arg(videoId);
                    destinationPath = availableImportPath(targetDir->path, preferredName, false);
                    saved = writeBytes(destinationPath, urlShortcutBytes(QUrl(*embedUrl)));
                }
            }
            if (!saved) {
                const auto url = firstUrlFromText(mimeData->text());
                if (url && url->isValid()) {
                    destinationPath = availableImportPath(targetDir->path, urlShortcutFileName(*url), false);
                    saved = writeBytes(destinationPath, urlShortcutBytes(*url));
                }
            }
        }
        if (!saved && mimeData->hasImage()) {
            const QImage image = qvariant_cast<QImage>(mimeData->imageData());
            if (!image.isNull()) {
                destinationPath = availableImportPath(targetDir->path, QStringLiteral("貼り付け画像.png"), false);
                saved = image.save(destinationPath, "PNG");
            }
        } else if (!saved && mimeData->hasHtml()) {
            destinationPath = availableImportPath(targetDir->path, QStringLiteral("貼り付け.html"), false);
            saved = writeBytes(destinationPath, mimeData->html().toUtf8());
        } else if (!saved && mimeData->hasText()) {
            destinationPath = availableImportPath(targetDir->path, QStringLiteral("貼り付け.txt"), false);
            saved = writeBytes(destinationPath, mimeData->text().toUtf8());
        } else if (!saved) {
            const auto binaryFormat = pasteableBinaryFormat(mimeData);
            if (!binaryFormat) {
                saved = false;
            } else {
            const QString extension = extensionForMimeFormat(*binaryFormat);
            destinationPath = availableImportPath(targetDir->path, QStringLiteral("貼り付け.%1").arg(extension), false);
            saved = writeBytes(destinationPath, mimeData->data(*binaryFormat));
            }
        }

        if (!saved || destinationPath.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("クリップボードの内容を貼り付けできませんでした。"));
            return;
        }

        const QString fileName = QFileInfo(destinationPath).fileName();
        appendCreatedItemToOrder(targetDir->path, fileName, false);
        saveOrderFile();
        rebuild(false);
        const QString trashPath = allocateTrashPath(fileName);
        recordHistory(QStringLiteral("貼り付け"), {{trashPath, destinationPath}},
                      {{destinationPath, trashPath}}, historyBefore, historySelection);
    }

void MainWindow::pasteClipboardToFolderPathAction(const QString& folderPath)
{
        pasteClipboardToFolder(nodeForPath(folderPath));
    }

NodeItem* MainWindow::folderItemAt(const QPointF& scenePos, const NodeItem* exclude) const
{
        for (QGraphicsItem* item : scene_.items(scenePos)) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem != exclude && nodeItem->node()->isDir) {
                return nodeItem;
            }
        }
        return nullptr;
    }

NodeItem* MainWindow::fileItemAt(const QPointF& scenePos, const NodeItem* exclude) const
{
        for (QGraphicsItem* item : scene_.items(scenePos)) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem != exclude && !nodeItem->node()->isDir) {
                return nodeItem;
            }
        }
        return nullptr;
    }

std::vector<NodeItem*> MainWindow::selectedTopLevelDragItems(const NodeItem* source) const
{
        if (!source || !source->isSelected()) {
            return {};
        }

        std::vector<NodeItem*> selectedItems;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node()) {
                selectedItems.push_back(nodeItem);
            }
        }

        std::vector<NodeItem*> topLevelItems;
        for (NodeItem* item : selectedItems) {
            bool containedBySelectedFolder = false;
            for (NodeItem* other : selectedItems) {
                if (other == item || !other->node()->isDir) {
                    continue;
                }
                if (isDescendantPath(other->node()->path, item->node()->path)) {
                    containedBySelectedFolder = true;
                    break;
                }
            }
            if (!containedBySelectedFolder) {
                topLevelItems.push_back(item);
            }
        }
        return topLevelItems;
    }

bool MainWindow::isMultiDragSelection(const NodeItem* source) const
{
        return selectedTopLevelDragItems(source).size() > 1;
    }

void MainWindow::setDragVisuals(NodeItem* source, qreal opacity, qreal zValue)
{
        std::vector<NodeItem*> dragItems = selectedTopLevelDragItems(source);
        if (dragItems.empty() && source) {
            dragItems.push_back(source);
        }
        for (NodeItem* item : dragItems) {
            item->setOpacity(opacity);
            item->setZValue(zValue);
        }
    }

void MainWindow::previewDragSelection(NodeItem* source)
{
        std::vector<NodeItem*> dragItems = selectedTopLevelDragItems(source);
        if (dragItems.size() <= 1 || !source) {
            return;
        }

        const QPointF delta = source->pos() - source->layoutCenter();
        for (NodeItem* item : dragItems) {
            setVisualPosition(item, item->layoutCenter() + delta);
        }
        scene_.update();
    }

NodeItem* MainWindow::folderItemForDrop(const NodeItem* source, const QPointF& scenePos) const
{
        return DropTargetResolver::folderTarget(scene_, source, selectedTopLevelDragItems(source), scenePos);
    }

NodeItem* MainWindow::linkTargetItemForDrop(const NodeItem* source, const QPointF& scenePos) const
{
        return DropTargetResolver::linkTarget(scene_, source, isMultiDragSelection(source),
                                              mycelStorageEnabled_, scenePos);
    }

NodeItem* MainWindow::nodeItemForPath(const QString& path) const
{
        // O(1) lookup via the index maintained by renderCurrentTree (rebuilt with the scene).
        return path.isEmpty() ? nullptr : nodeItemsByPath_.value(path, nullptr);
    }

NodeItem* MainWindow::updateInternalDropHover(NodeItem* source, const QPointF& scenePos)
{
        NodeItem* target = folderItemForDrop(source, scenePos);
        const QString targetPath = target ? target->path() : QString();
        if (targetPath == dragHoverFolderPath_) {
            return target;
        }

        clearInternalDropHover();
        dragHoverFolderPath_ = targetPath;
        if (target) {
            target->setInternalDropHover(true);
        }
        return target;
    }

NodeItem* MainWindow::updateLinkDropHover(NodeItem* source, const QPointF& scenePos)
{
        NodeItem* target = linkTargetItemForDrop(source, scenePos);
        const QString targetPath = target ? target->path() : QString();
        if (targetPath == dragHoverLinkTargetPath_) {
            return target;
        }

        clearLinkDropHover();
        dragHoverLinkTargetPath_ = targetPath;
        if (target) {
            target->setLinkDropHover(true);
        }
        return target;
    }

void MainWindow::clearInternalDropHover()
{
        const QString hoverPath = dragHoverFolderPath_;
        dragHoverFolderPath_.clear();
        if (hoverPath.isEmpty()) {
            return;
        }
        if (NodeItem* hoverItem = nodeItemForPath(hoverPath)) {
            hoverItem->setInternalDropHover(false);
        }
    }

void MainWindow::clearLinkDropHover()
{
        const QString hoverPath = dragHoverLinkTargetPath_;
        dragHoverLinkTargetPath_.clear();
        if (hoverPath.isEmpty()) {
            return;
        }
        if (NodeItem* hoverItem = nodeItemForPath(hoverPath)) {
            hoverItem->setLinkDropHover(false);
        }
    }

void MainWindow::resetDropHoverState()
{
        dragHoverFolderPath_.clear();
        dragHoverLinkTargetPath_.clear();
    }

bool MainWindow::reorderNodeByY(Node* source, const NodeItem* sourceItem, qreal dropCenterY)
{
        if (!mycelStorageEnabled_) {
            return false;
        }
        if (!source || !sourceItem) {
            return false;
        }

        struct Sibling {
            QString name;
            qreal y;
        };
        std::vector<Sibling> siblings;
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem || nodeItem->node() == source) {
                continue;
            }
            if (nodeItem->node()->parentPath == source->parentPath) {
                siblings.push_back({nodeItem->node()->name, nodeItem->layoutCenter().y()});
            }
        }

        if (siblings.empty()) {
            return false;
        }

        std::sort(siblings.begin(), siblings.end(), [](const Sibling& a, const Sibling& b) {
            return a.y < b.y;
        });

        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        const QString key = orderKeyForDirectory(source->parentPath);
        QStringList order = fileOrders_[key];
        QDir parent(source->parentPath);
        const QFileInfoList entries = parent.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                           QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& entry : entries) {
            if (entry.fileName() == QStringLiteral(".mycel")) {
                continue;
            }
            if (!order.contains(entry.fileName())) {
                order.append(entry.fileName());
            }
        }

        order.removeAll(source->name);
        int insertIndex = 0;
        while (insertIndex < static_cast<int>(siblings.size()) && dropCenterY > siblings[insertIndex].y) {
            ++insertIndex;
        }
        order.insert(std::clamp(insertIndex, 0, static_cast<int>(order.size())), source->name);
        fileOrders_[key] = order;
        saveOrderFile();
        // No file was added/removed/renamed -- only the display order changed -- so resort the
        // cached children in place and relayout instead of rebuild()'s full disk re-stat.
        if (Node* parentNode = findVisibleNodeByPath(root_.get(), source->parentPath)) {
            reorderChildrenInPlace(*parentNode, order);
            relayout();
        } else {
            rebuild(false);
        }
        recordHistory(QStringLiteral("並び替え"), {}, {}, historyBefore, historySelection);
        return true;
    }

void MainWindow::previewReorder(Node* source, const NodeItem* sourceItem, qreal dragCenterY)
{
        if (!mycelStorageEnabled_) {
            return;
        }
        if (!source || !sourceItem) {
            return;
        }

        struct Sibling {
            NodeItem* item;
            qreal y;
        };

        std::vector<Sibling> siblings;
        std::vector<qreal> slotYs;
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem) {
                continue;
            }
            if (nodeItem->node()->parentPath != source->parentPath) {
                continue;
            }

            slotYs.push_back(nodeItem->layoutCenter().y());
            if (nodeItem->node() != source) {
                siblings.push_back({nodeItem, nodeItem->layoutCenter().y()});
            }
        }

        if (siblings.empty()) {
            return;
        }

        std::sort(slotYs.begin(), slotYs.end());
        std::sort(siblings.begin(), siblings.end(), [](const Sibling& a, const Sibling& b) {
            return a.y < b.y;
        });

        int insertIndex = 0;
        while (insertIndex < static_cast<int>(siblings.size()) && dragCenterY > siblings[insertIndex].y) {
            ++insertIndex;
        }

        std::vector<NodeItem*> visualOrder;
        visualOrder.reserve(slotYs.size());
        for (int i = 0; i <= static_cast<int>(siblings.size()); ++i) {
            if (i == insertIndex) {
                visualOrder.push_back(nullptr);
            }
            if (i < static_cast<int>(siblings.size())) {
                visualOrder.push_back(siblings[i].item);
            }
        }

        for (int i = 0; i < static_cast<int>(visualOrder.size()) && i < static_cast<int>(slotYs.size()); ++i) {
            NodeItem* item = visualOrder[i];
            if (!item) {
                continue;
            }
            setVisualPosition(item, QPointF(item->layoutCenter().x(), slotYs[i]));
        }
        scene_.update();
    }

void MainWindow::previewMoveDescendants(Node* source, const QPointF& delta)
{
        if (!source || !source->isDir) {
            return;
        }

        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem || nodeItem->node() == source) {
                continue;
            }
            if (isDescendantPath(source->path, nodeItem->node()->path)) {
                nodeItem->setPos(nodeItem->layoutCenter() + delta);
            }
        }
        scene_.update();
    }

bool MainWindow::shouldKeepDragPreviewItem(const NodeItem* item, const std::vector<NodeItem*>& keepItems) const
{
        if (!item) {
            return false;
        }
        for (const NodeItem* keepItem : keepItems) {
            if (!keepItem || !keepItem->node()) {
                continue;
            }
            if (item == keepItem || isDescendantPath(keepItem->node()->path, item->node()->path)) {
                return true;
            }
        }
        return false;
    }

void MainWindow::clearDragPreviewForSource(const NodeItem* source)
{
        std::vector<NodeItem*> keepItems = selectedTopLevelDragItems(source);
        if (keepItems.empty() && source) {
            keepItems.push_back(const_cast<NodeItem*>(source));
        }
        clearDragPreview(keepItems);
    }

void MainWindow::clearDragPreview(const std::vector<NodeItem*>& keepItems)
{
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem) {
                continue;
            }
            if (shouldKeepDragPreviewItem(nodeItem, keepItems)) {
                continue;
            }
            if ((nodeItem->pos() - nodeItem->layoutCenter()).manhattanLength() > 0.5) {
                nodeItem->setPos(nodeItem->layoutCenter());
            }
        }
        scene_.update();
    }

QString MainWindow::availableImportPath(const QString& targetDirectoryPath, const QString& preferredName, bool isDirectory,
                                QSet<QString>* reservedNames) const
{
        return FileOperationService::availableDestination(targetDirectoryPath, preferredName, isDirectory,
                                                          reservedNames);
    }

bool MainWindow::writeBytes(const QString& path, const QByteArray& bytes) const
{
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        return file.write(bytes) == bytes.size();
    }

std::optional<QString> MainWindow::pasteableBinaryFormat(const QMimeData* mimeData) const
{
        if (!mimeData) {
            return std::nullopt;
        }

        const QStringList preferredFormats = {
            QStringLiteral("image/png"),
            QStringLiteral("image/jpeg"),
            QStringLiteral("image/bmp"),
            QStringLiteral("image/gif"),
            QStringLiteral("application/pdf"),
            QStringLiteral("application/json"),
            QStringLiteral("text/csv"),
            QStringLiteral("application/octet-stream")
        };

        for (const QString& format : preferredFormats) {
            if (mimeData->hasFormat(format) && !mimeData->data(format).isEmpty()) {
                return format;
            }
        }

        for (const QString& format : mimeData->formats()) {
            if (format.startsWith(QStringLiteral("application/x-qt-"), Qt::CaseInsensitive)) {
                continue;
            }
            if (!mimeData->data(format).isEmpty()) {
                return format;
            }
        }

        return std::nullopt;
    }

QString MainWindow::extensionForMimeFormat(const QString& format) const
{
        const QString lower = format.toLower();
        if (lower == QStringLiteral("image/png")) {
            return QStringLiteral("png");
        }
        if (lower == QStringLiteral("image/jpeg") || lower == QStringLiteral("image/jpg")) {
            return QStringLiteral("jpg");
        }
        if (lower == QStringLiteral("image/bmp")) {
            return QStringLiteral("bmp");
        }
        if (lower == QStringLiteral("image/gif")) {
            return QStringLiteral("gif");
        }
        if (lower == QStringLiteral("image/webp")) {
            return QStringLiteral("webp");
        }
        if (lower == QStringLiteral("application/pdf")) {
            return QStringLiteral("pdf");
        }
        if (lower == QStringLiteral("application/json")) {
            return QStringLiteral("json");
        }
        if (lower == QStringLiteral("text/csv")) {
            return QStringLiteral("csv");
        }
        if (lower == QStringLiteral("text/markdown")) {
            return QStringLiteral("md");
        }
        if (lower == QStringLiteral("text/xml") || lower == QStringLiteral("application/xml")) {
            return QStringLiteral("xml");
        }
        return QStringLiteral("bin");
    }

bool MainWindow::copyDirectoryRecursively(const QString& sourcePath, const QString& destinationPath) const
{
        return FileOperationService::copyDirectoryRecursively(sourcePath, destinationPath);
    }

