#include "main_window.h"

void MainWindow::applyMovedMetadata(const FileOperationService::MovedEntry& entry, const QString& targetDirPath,
                            bool updateOrder)
{
        rekeyPathMetadataAfterRename(entry.oldPath, entry.newPath, entry.isDir);
        if (updateOrder) {
            QStringList& sourceOrder = fileOrders_[orderKeyForDirectory(entry.sourceParentPath)];
            sourceOrder.removeAll(entry.name);
            QStringList& targetOrder = fileOrders_[orderKeyForDirectory(targetDirPath)];
            if (!targetOrder.contains(entry.destinationName)) {
                targetOrder.append(entry.destinationName);
            }
        }
    }

void MainWindow::pauseFileSystemWatcher()
{
        if (!fileSystemWatcher_) {
            return;
        }
        const QStringList files = fileSystemWatcher_->files();
        if (!files.isEmpty()) {
            fileSystemWatcher_->removePaths(files);
        }
        const QStringList directories = fileSystemWatcher_->directories();
        if (!directories.isEmpty()) {
            fileSystemWatcher_->removePaths(directories);
        }
    }

void MainWindow::refreshNode(Node*)
{
        cancelQueuedInlinePreviewToggle();
        rebuild(false);
    }

void MainWindow::refreshSelectedItems()
{
        cancelQueuedInlinePreviewToggle();
        rebuild(false);
    }

void MainWindow::refreshAll()
{
        cancelQueuedInlinePreviewToggle();
        loadMetadataAndReconcileHashCache();
        if (!loadCollapsedFile()) {
            applyLargeTreeStartupCollapse();
        }
        rebuild(false);
    }

bool MainWindow::isMetadataPath(const QString& path) const
{
        const QString metadataRoot = QDir::cleanPath(QDir(rootPath_).filePath(QStringLiteral(".mycel")));
        const QString cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        return cleanPath == metadataRoot || cleanPath.startsWith(metadataRoot + QLatin1Char('/'));
    }

void MainWindow::updateNativeWatcherModeForRoot()
{
        // MYCEL_NO_WATCHER forces sweep-only mode: an escape hatch for filesystems whose
        // change notifications misbehave but are not detected as network mounts.
        nativeWatcherDisabled_ = qEnvironmentVariableIsSet("MYCEL_NO_WATCHER") ||
                                 isNetworkFileSystemPath(rootPath_);
        if (nativeWatcherDisabled_ && fileSystemWatcher_) {
            const QStringList files = fileSystemWatcher_->files();
            if (!files.isEmpty()) {
                fileSystemWatcher_->removePaths(files);
            }
            const QStringList directories = fileSystemWatcher_->directories();
            if (!directories.isEmpty()) {
                fileSystemWatcher_->removePaths(directories);
            }
            recordDebugEvent(QStringLiteral(
                "network filesystem root: native watching disabled, periodic sweep takes over"));
        }
    }

void MainWindow::resetFileSystemWatcher()
{
        if (!fileSystemWatcher_ || inlineRenameActivitySuspended_ || nativeWatcherDisabled_) {
            return;
        }
        if (startupScanPending_ || fileWatcherResetPending_) {
            // The startup scan thread, or an already in-flight reset, is already producing (or
            // will produce) an up-to-date directory/file list; no need to walk again. Called from
            // every rebuild/board-mode toggle/rename resume, this used to walk the whole tree
            // synchronously on the GUI thread every time -- the walk (no content hashing, but
            // still a full recursive readdir) now runs on a worker thread, same pattern as
            // runBackgroundReconcile().
            return;
        }

        fileWatcherResetPending_ = true;
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        fileWatcherResetCancelled_ = cancelled;
        const QString scannedRootPath = rootPath_;
        fileWatcherResetThread_ = QThread::create(
            [this, scannedRootPath, storageEnabled = mycelStorageEnabled_, cancelled] {
                TreeWalkResult walk = walkRealTree(scannedRootPath, cancelled.get(), storageEnabled);
                if (cancelled->load()) {
                    return;  // window is closing; the destructor waits for this thread
                }
                QMetaObject::invokeMethod(
                    this, [this, scannedRootPath, walk = std::move(walk)] {
                        finishFileSystemWatcherReset(scannedRootPath, walk);
                    },
                    Qt::QueuedConnection);
            });
        fileWatcherResetThread_->start();
    }

void MainWindow::finishFileSystemWatcherReset(const QString& scannedRootPath, const TreeWalkResult& walk)
{
        fileWatcherResetPending_ = false;
        if (QDir::cleanPath(scannedRootPath) != QDir::cleanPath(rootPath_)) {
            return;  // root changed mid-walk; the new root's own rebuild already re-requested this
        }
        if (!fileSystemWatcher_ || inlineRenameActivitySuspended_ || nativeWatcherDisabled_) {
            return;
        }
        applyFileSystemWatcherPaths(walk.directories, walk.files);
    }

void MainWindow::applyFileSystemWatcherPaths(const QStringList& directories, const QStringList& files)
{
        if (nativeWatcherDisabled_) {
            return;
        }
        QSet<QString> toAdd(directories.begin(), directories.end());
        for (const QString& path : files) {
            toAdd.insert(path);
        }
        QStringList toRemove;
        const QStringList watched = fileSystemWatcher_->directories() + fileSystemWatcher_->files();
        for (const QString& path : watched) {
            if (!toAdd.remove(path)) {  // still wanted: nothing to do, keep the live watch
                toRemove.append(path);
            }
        }
        if (!toRemove.isEmpty()) {
            fileSystemWatcher_->removePaths(toRemove);
        }
        if (!toAdd.isEmpty()) {
            fileSystemWatcher_->addPaths(QStringList(toAdd.begin(), toAdd.end()));
        }
    }

void MainWindow::reviveWatchedPaths(const QSet<QString>& paths)
{
        if (!fileSystemWatcher_ || inlineRenameActivitySuspended_ || nativeWatcherDisabled_) {
            return;
        }
        QStringList revive;
        for (const QString& path : paths) {
            if (!isMetadataPath(path) && QFileInfo::exists(path)) {
                revive.append(path);
            }
        }
        if (revive.isEmpty()) {
            return;
        }
        fileSystemWatcher_->removePaths(revive);
        fileSystemWatcher_->addPaths(revive);
    }

void MainWindow::scheduleFileSystemRefresh(const QString& path)
{
        if (inlineRenameActivitySuspended_ || renameEdit_) {
            return;
        }
        if (sideEditorEditing_) {
            return;
        }
        if (isMetadataPath(path)) {
            return;
        }
        pendingFileSystemPaths_.insert(QFileInfo(path).absoluteFilePath());
        fileSystemRefreshTimer_.start();
    }

QString MainWindow::refreshDirectoryForChangedPath(const QString& path) const
{
        QFileInfo info(path);
        if (info.exists() && info.isDir()) {
            return info.absoluteFilePath();
        }
        return info.absolutePath();
    }

QString MainWindow::nearestVisibleDirectoryPath(QString path) const
{
        path = QDir::cleanPath(path);
        const QString cleanRoot = QDir::cleanPath(rootPath_);
        while (!path.isEmpty()) {
            if (findVisibleNodeByPath(root_.get(), path)) {
                return path;
            }
            if (path == cleanRoot) {
                return cleanRoot;
            }
            const QString parent = QFileInfo(path).absolutePath();
            if (parent == path) {
                break;
            }
            path = parent;
        }
        return cleanRoot;
    }

QStringList MainWindow::pendingRefreshDirectories() const
{
        QStringList directories;
        for (const QString& path : pendingFileSystemPaths_) {
            QString directory = refreshDirectoryForChangedPath(path);
            if (directory.isEmpty() || isMetadataPath(directory)) {
                continue;
            }
            if (root_ && !findVisibleNodeByPath(root_.get(), directory)) {
                directory = nearestVisibleDirectoryPath(directory);
            }
            directories.append(QDir::cleanPath(directory));
        }
        directories.removeDuplicates();

        QStringList topLevel;
        for (const QString& directory : directories) {
            bool coveredByAncestor = false;
            for (const QString& other : directories) {
                if (directory == other) {
                    continue;
                }
                if (isDescendantPath(other, directory)) {
                    coveredByAncestor = true;
                    break;
                }
            }
            if (!coveredByAncestor) {
                topLevel.append(directory);
            }
        }
        return topLevel;
    }

QStringList MainWindow::pendingChangedDirectoriesForHashSync() const
{
        QStringList directories;
        for (const QString& path : pendingFileSystemPaths_) {
            const QString directory = refreshDirectoryForChangedPath(path);
            if (directory.isEmpty() || isMetadataPath(directory)) {
                continue;
            }
            directories.append(QDir::cleanPath(directory));
        }
        directories.removeDuplicates();
        return directories;
    }

bool MainWindow::reconcileRenamedFiles(const QStringList& directories, bool pruneUnmatchedMetadata)
{
        if (!mycelStorageEnabled_ || directories.isEmpty()) {
            return false;
        }

        QSet<QString> currentPaths;
        for (const QString& directoryPath : directories) {
            const QDir dir(directoryPath);
            const QFileInfoList entries =
                dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
            for (const QFileInfo& info : entries) {
                currentPaths.insert(info.absoluteFilePath());
            }
        }
        return reconcileRenamedFiles(directories, currentPaths, pruneUnmatchedMetadata, nullptr);
    }

bool MainWindow::reconcileRenamedFiles(const QStringList& directories, const QSet<QString>& currentPaths,
                               bool pruneUnmatchedMetadata,
                               const std::map<QString, FileHashCacheEntry>* precomputedHashes)
{
        if (!mycelStorageEnabled_ || directories.isEmpty()) {
            return false;
        }

        QSet<QString> dirKeys;
        for (const QString& directoryPath : directories) {
            dirKeys.insert(orderKeyForDirectory(directoryPath));
        }

        // Files these directories were previously known to contain, per the hash cache
        // (independent of whatever the display tree currently shows for them).
        QStringList previousPaths;
        for (const auto& [key, entry] : fileHashCache_) {
            Q_UNUSED(entry);
            const QString parentKey = QFileInfo(key).path();
            if (dirKeys.contains(parentKey == QStringLiteral(".") ? QString() : parentKey)) {
                previousPaths.append(absolutePathForKey(key));
            }
        }

        QStringList vanished;
        for (const QString& path : previousPaths) {
            if (!currentPaths.contains(path)) {
                vanished.append(path);
            }
        }
        if (vanished.isEmpty()) {
            return false;
        }

        const QSet<QString> previousPathSet(previousPaths.begin(), previousPaths.end());
        QStringList appeared;
        for (const QString& path : currentPaths) {
            if (!previousPathSet.contains(path)) {
                appeared.append(path);
            }
        }

        recordDebugEvent(QStringLiteral("reconcile candidates across %1 directories: vanished=%2 appeared=%3 prune=%4")
                             .arg(directories.size())
                             .arg(vanished.size())
                             .arg(appeared.size())
                             .arg(pruneUnmatchedMetadata ? QStringLiteral("true") : QStringLiteral("false")));

        std::map<QByteArray, QStringList> vanishedByHash;
        for (const QString& path : vanished) {
            const auto cached = fileHashCache_.find(relativeKeyForPath(path));
            if (cached != fileHashCache_.end()) {
                vanishedByHash[cached->second.hash].append(path);
            }
        }
        std::map<QByteArray, QStringList> appearedByHash;
        for (const QString& path : appeared) {
            std::optional<QByteArray> hash;
            if (precomputedHashes) {
                const auto found = precomputedHashes->find(relativeKeyForPath(path));
                if (found != precomputedHashes->end()) {
                    hash = found->second.hash;
                }
            } else {
                hash = computeFileContentHash(QFileInfo(path));
            }
            if (hash) {
                appearedByHash[*hash].append(path);
            } else {
                const QFileInfo info(path);
                recordDebugEvent(QStringLiteral("reconcile: could not hash appeared %1 (%2)")
                                     .arg(relativeKeyForPath(path), describeHashSkipReason(info)));
            }
        }

        bool changed = false;
        for (const auto& [hash, oldCandidates] : vanishedByHash) {
            if (oldCandidates.size() != 1) {
                recordDebugEvent(QStringLiteral("reconcile: ambiguous vanished group (%1 files share a hash)")
                                     .arg(oldCandidates.size()));
                continue;  // ambiguous: duplicate content among vanished files -- leave alone
            }
            const QString& oldPath = oldCandidates.front();
            const auto match = appearedByHash.find(hash);
            if (match == appearedByHash.end()) {
                if (pruneUnmatchedMetadata) {
                    removeDeletedPathMetadata(oldPath, false);
                    recordDebugEvent(QStringLiteral("reconcile: no match anywhere for vanished %1, removing its metadata")
                                         .arg(relativeKeyForPath(oldPath)));
                    changed = true;
                }
                continue;
            }
            if (match->second.size() != 1) {
                recordDebugEvent(QStringLiteral("reconcile: ambiguous appeared group (%1 files share a hash)")
                                     .arg(match->second.size()));
                continue;  // ambiguous among appeared files -- leave alone
            }
            const QString& newPath = match->second.front();
            rekeyPathMetadataAfterRename(oldPath, newPath, false);
            recordDebugEvent(QStringLiteral("reconcile: matched rename/move %1 -> %2")
                                 .arg(relativeKeyForPath(oldPath), relativeKeyForPath(newPath)));
            changed = true;
        }
        return changed;
    }

void MainWindow::persistRenameReconciliationResults()
{
        saveColorFile();
        saveOrderFile();
        savePreviewFile();
        saveLinkFile();
        saveCollapsedFile();
        // The reconcile rekeyed fileHashCache_ in memory too. Persist it now: the hash refresh
        // passes that follow (updateHashCacheEntryForFile over the walked files, or
        // updateHashCacheForDirectory) see every current file as already
        // hashed (the rekeyed entries match size/mtime), report "no change", and skip their
        // save — leaving hashcache.json holding the pre-rename names. A stale cache makes the
        // NEXT session reconcile against one-generation-old paths, which misses the metadata
        // keyed under the current names and orphans it.
        saveHashCacheFile();
    }

void MainWindow::runBackgroundReconcile()
{
        if (rootPath_.isEmpty() || !QFileInfo(rootPath_).isDir()) {
            return;
        }
        if (!mycelStorageEnabled_ && !nativeWatcherDisabled_) {
            return;  // nothing to reconcile, and the live watcher covers structural changes
        }
        if (startupScanPending_ || reconcileScanPending_) {
            return;  // a whole-tree walk (startup, or a still-running reconcile pass) already covers this
        }
        if ((view_ && view_->isDraggingNode()) || sideEditorEditing_ || inlineRenameActivitySuspended_ ||
            renameEdit_) {
            return;
        }

        reconcileScanPending_ = true;
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        reconcileScanCancelled_ = cancelled;
        reconcileScanThread_ = QThread::create(
            [this, rootPath = rootPath_, storageEnabled = mycelStorageEnabled_,
             knownHashes = fileHashCache_, cancelled] {
                StartupScanResult result = runStartupScan(rootPath, storageEnabled, knownHashes, *cancelled);
                if (cancelled->load()) {
                    return;  // window is closing; the destructor waits for this thread
                }
                QMetaObject::invokeMethod(
                    this, [this, rootPath, result = std::move(result)] { finishBackgroundReconcile(rootPath, result); },
                    Qt::QueuedConnection);
            });
        reconcileScanThread_->start();
    }

void MainWindow::finishBackgroundReconcile(const QString& scannedRootPath, const StartupScanResult& result)
{
        reconcileScanPending_ = false;
        if (QDir::cleanPath(scannedRootPath) != QDir::cleanPath(rootPath_)) {
            // The root changed while this sweep was in flight; see finishStartupScan() for why
            // applying it to the new root would be wrong. No extra recovery needed here: the new
            // root's own periodic sweep (or resetFileSystemWatcher() from its own rebuild) covers it.
            recordDebugEvent(QStringLiteral("background reconcile discarded: root changed mid-scan (%1 -> %2)")
                                 .arg(scannedRootPath, rootPath_));
            return;
        }

        bool renameReconciled = false;
        if (mycelStorageEnabled_ && !result.directories.isEmpty()) {
            QSet<QString> currentPaths;
            for (const QString& path : result.files) {
                currentPaths.insert(path);
            }
            // Whole-tree scope: safe to prune metadata for files with no match anywhere in the root.
            renameReconciled = reconcileRenamedFiles(result.directories, currentPaths,
                                                     /*pruneUnmatchedMetadata=*/true,
                                                     &result.refreshedHashes);
            if (renameReconciled) {
                persistRenameReconciliationResults();
            }
        }

        bool hashCacheChanged = false;
        for (const auto& [key, entry] : result.refreshedHashes) {
            const auto existing = fileHashCache_.find(key);
            if (existing != fileHashCache_.end() &&
                (existing->second.mtimeMs > entry.mtimeMs ||
                 (existing->second.mtimeMs == entry.mtimeMs && existing->second.size == entry.size &&
                  existing->second.hash == entry.hash))) {
                continue;
            }
            fileHashCache_[key] = entry;
            hashCacheChanged = true;
        }
        if (hashCacheChanged) {
            saveHashCacheFile();
        }
        if (renameReconciled) {
            // The display tree still holds the old name/path until rescanned.
            rebuild(false);
        }

        // Network roots get no native change notifications, so this sweep is also their
        // structural refresh: external adds/removes surface here instead of via the watcher.
        if (nativeWatcherDisabled_) {
            if (boardMode_) {
                // Same rule as the watcher-driven path: only a vanished card forces a render.
                for (const auto& node : boardNodes_) {
                    if (!QFileInfo::exists(node->path)) {
                        renderBoardScene(false);
                        break;
                    }
                }
            } else if (!renameReconciled && root_) {  // a reconcile rebuild already rescanned
                const QStringList selectedPaths = selectedNodePaths();
                if (replaceScannedSubtree(rootPath_) == SubtreeRefresh::Changed) {
                    renderCurrentTree(false);
                    if (!selectedPaths.isEmpty()) {
                        restoreSelection(selectedPaths);
                    }
                }
            }
        }
    }

MainWindow::SubtreeRefresh MainWindow::replaceScannedSubtree(const QString& directoryPath)
{
        if (!root_) {
            return SubtreeRefresh::NotFound;
        }

        const QString cleanPath = QDir::cleanPath(directoryPath);
        const QString cleanRoot = QDir::cleanPath(root_->path);
        if (cleanPath == cleanRoot) {
            auto rescanned = scanTree(rootPath_, 0, -1, collapsedPaths_, previewPaths_, previewSizes_,
                                      fileOrders_, rootPath_, mycelStorageEnabled_);
            if (sameVisibleStructure(*root_, *rescanned)) {
                return SubtreeRefresh::Unchanged;
            }
            root_ = std::move(rescanned);
            return SubtreeRefresh::Changed;
        }

        Node* existing = findVisibleNodeByPath(root_.get(), cleanPath);
        if (!existing || !existing->isDir) {
            return SubtreeRefresh::NotFound;
        }

        Node* parent = findVisibleNodeByPath(root_.get(), existing->parentPath);
        if (!parent) {
            return SubtreeRefresh::NotFound;
        }

        for (auto& child : parent->children) {
            if (QDir::cleanPath(child->path) == cleanPath) {
                auto rescanned = scanTree(cleanPath, existing->depth, existing->branch,
                                          collapsedPaths_, previewPaths_, previewSizes_, fileOrders_, rootPath_,
                                          mycelStorageEnabled_);
                if (sameVisibleStructure(*child, *rescanned)) {
                    return SubtreeRefresh::Unchanged;
                }
                child = std::move(rescanned);
                return SubtreeRefresh::Changed;
            }
        }
        return SubtreeRefresh::NotFound;
    }

void MainWindow::refreshFromFileSystemChange()
{
        if (view_ && view_->isDraggingNode()) {
            // Defer rebuilds while a node drag is in progress. Rebuilding mid-drag
            // destroys the dragged item and cancels the gesture — frequent with
            // cloud-synced roots (Dropbox etc.) that touch files continuously.
            fileSystemRefreshTimer_.start();
            return;
        }
        if (sideEditorEditing_) {
            pendingFileSystemPaths_.clear();
            return;
        }
        if (!QFileInfo(rootPath_).isDir()) {
            resetFileSystemWatcher();
            return;
        }
        // Kept for the no-visible-change exits below: a fired watch may be dead (deleted or
        // renamed path) and needs re-arming even when nothing is re-rendered or re-registered.
        const QSet<QString> firedPaths = pendingFileSystemPaths_;
        if (boardMode_) {
            // Board mode: re-render only when a displayed card's file disappeared (spec: a card
            // whose real file is gone must leave the screen). New files never appear on their own.
            pendingFileSystemPaths_.clear();
            cancelQueuedInlinePreviewToggle();
            bool missing = false;
            for (const auto& node : boardNodes_) {
                if (!QFileInfo::exists(node->path)) {
                    missing = true;
                    break;
                }
            }
            if (missing) {
                renderBoardScene(false);
            } else {
                reviveWatchedPaths(firedPaths);
            }
            return;
        }

        const QStringList refreshDirectories = pendingRefreshDirectories();
        const QStringList hashSyncDirectories = pendingChangedDirectoriesForHashSync();
        pendingFileSystemPaths_.clear();
        cancelQueuedInlinePreviewToggle();

        // Reconcile external renames/moves and refresh the hash cache against the real
        // directories that changed, regardless of whether they are visible/expanded in the
        // display tree (collapsed folders still need this to keep their metadata from going
        // stale). Scope is limited to the directories that actually changed, so this can match a
        // move only when both its source and destination are in that set; pruning is left to the
        // whole-tree background sweep, since this narrower scope could otherwise mistake a file
        // moved to a folder outside this batch for a deletion.
        QStringList existingHashSyncDirectories;
        for (const QString& directory : hashSyncDirectories) {
            if (QFileInfo(directory).isDir()) {
                existingHashSyncDirectories.append(directory);
            }
        }
        const bool renameReconciled =
            reconcileRenamedFiles(existingHashSyncDirectories, /*pruneUnmatchedMetadata=*/false);
        bool hashCacheChanged = false;
        for (const QString& directory : existingHashSyncDirectories) {
            if (updateHashCacheForDirectory(directory)) {
                hashCacheChanged = true;
            }
        }
        if (renameReconciled) {
            persistRenameReconciliationResults();
        }
        if (hashCacheChanged) {
            saveHashCacheFile();
        }

        if (refreshDirectories.isEmpty()) {
            reviveWatchedPaths(firedPaths);
            return;
        }

        // Capture selection before any subtree is replaced: selectedNodePaths() reads the
        // Node objects behind the live items, which a replace would free.
        const QStringList selectedPaths = selectedNodePaths();

        bool changed = false;
        bool needFullRescan = false;
        for (const QString& directory : refreshDirectories) {
            switch (replaceScannedSubtree(directory)) {
            case SubtreeRefresh::Changed:
                changed = true;
                break;
            case SubtreeRefresh::NotFound:
                needFullRescan = true;
                break;
            case SubtreeRefresh::Unchanged:
                break;
            }
        }

        if (needFullRescan) {
            auto rescanned = scanTree(rootPath_, 0, -1, collapsedPaths_, previewPaths_, previewSizes_,
                                      fileOrders_, rootPath_, mycelStorageEnabled_);
            if (!root_ || !sameVisibleStructure(*root_, *rescanned)) {
                root_ = std::move(rescanned);
                changed = true;
            }
        }

        if (!changed) {
            // The notification did not change the displayed tree (typically a cloud-sync
            // mtime touch). Skip the re-render so selection and previews do not churn, and
            // only re-arm the fired watches instead of re-walking and re-registering the
            // whole tree -- under a cloud-sync client that touches files continuously
            // (Dropbox etc.) this branch runs every debounce tick.
            reviveWatchedPaths(firedPaths);
            return;
        }

        renderCurrentTree(false);
        if (!selectedPaths.isEmpty()) {
            restoreSelection(selectedPaths);
        }
    }

void MainWindow::removeDeletedPathMetadata(const QString& deletedPath, bool wasDir)
{
        if (wasDir) {
            for (const QString& path : collapsedPaths_.values()) {
                if (path == deletedPath || isDescendantPath(deletedPath, path)) {
                    collapsedPaths_.remove(path);
                }
            }
        } else {
            collapsedPaths_.remove(deletedPath);
        }

        if (mycelStorageEnabled_) {
            for (auto it = userColors_.begin(); it != userColors_.end();) {
                if (it->first == deletedPath || (wasDir && isDescendantPath(deletedPath, it->first))) {
                    it = userColors_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        for (auto it = previewSizes_.begin(); it != previewSizes_.end();) {
            if (it->first == deletedPath || (wasDir && isDescendantPath(deletedPath, it->first))) {
                it = previewSizes_.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = previewImageScales_.begin(); it != previewImageScales_.end();) {
            if (it->first == deletedPath || (wasDir && isDescendantPath(deletedPath, it->first))) {
                it = previewImageScales_.erase(it);
            } else {
                ++it;
            }
        }

        for (const QString& path : previewPaths_.values()) {
            if (path == deletedPath || (wasDir && isDescendantPath(deletedPath, path))) {
                previewPaths_.remove(path);
            }
        }

        for (auto it = fileLinks_.begin(); it != fileLinks_.end();) {
            const bool deleteFrom = it->from == deletedPath || (wasDir && isDescendantPath(deletedPath, it->from));
            const bool deleteTo = it->to == deletedPath || (wasDir && isDescendantPath(deletedPath, it->to));
            if (deleteFrom || deleteTo) {
                it = fileLinks_.erase(it);
            } else {
                ++it;
            }
        }

        // Drop the deleted file's cached hash so a later unrelated file that happens to share its
        // content is never mistaken for "the deleted file, renamed" during reconciliation.
        for (auto it = fileHashCache_.begin(); it != fileHashCache_.end();) {
            const QString path = absolutePathForKey(it->first);
            if (path == deletedPath || (wasDir && isDescendantPath(deletedPath, path))) {
                it = fileHashCache_.erase(it);
            } else {
                ++it;
            }
        }

        if (!mycelStorageEnabled_) {
            return;
        }

        QStringList& parentOrder = fileOrders_[orderKeyForDirectory(QFileInfo(deletedPath).absolutePath())];
        parentOrder.removeAll(QFileInfo(deletedPath).fileName());
        if (!wasDir) {
            return;
        }

        const QString deletedKey = orderKeyForDirectory(deletedPath);
        for (auto it = fileOrders_.begin(); it != fileOrders_.end();) {
            if (it->first == deletedKey || (!deletedKey.isEmpty() && it->first.startsWith(deletedKey + QLatin1Char('/')))) {
                it = fileOrders_.erase(it);
            } else {
                ++it;
            }
        }
    }

void MainWindow::rekeyPathMetadataAfterRename(const QString& oldPath, const QString& newPath, bool wasDir)
{
        QSet<QString> updatedCollapsedPaths;
        for (const QString& path : collapsedPaths_.values()) {
            updatedCollapsedPaths.insert(rekeyPathAfterRename(path, oldPath, newPath, wasDir));
        }
        collapsedPaths_ = updatedCollapsedPaths;

        std::map<QString, QColor> updatedColors;
        for (const auto& [path, color] : userColors_) {
            updatedColors[rekeyPathAfterRename(path, oldPath, newPath, wasDir)] = color;
        }
        userColors_ = std::move(updatedColors);

        std::map<QString, QSizeF> updatedPreviewSizes;
        for (const auto& [path, size] : previewSizes_) {
            updatedPreviewSizes[rekeyPathAfterRename(path, oldPath, newPath, wasDir)] = size;
        }
        previewSizes_ = std::move(updatedPreviewSizes);

        std::map<QString, qreal> updatedPreviewImageScales;
        for (const auto& [path, scale] : previewImageScales_) {
            updatedPreviewImageScales[rekeyPathAfterRename(path, oldPath, newPath, wasDir)] = scale;
        }
        previewImageScales_ = std::move(updatedPreviewImageScales);

        QSet<QString> updatedPreviewPaths;
        for (const QString& path : previewPaths_.values()) {
            updatedPreviewPaths.insert(rekeyPathAfterRename(path, oldPath, newPath, wasDir));
        }
        previewPaths_ = updatedPreviewPaths;

        for (FileLink& link : fileLinks_) {
            link.from = rekeyPathAfterRename(link.from, oldPath, newPath, wasDir);
            link.to = rekeyPathAfterRename(link.to, oldPath, newPath, wasDir);
        }
        // Rekeying can make two links' targets collide (e.g. the renamed file's new path matches
        // an existing link's target). A target sits beside exactly one source (see addFileLink()).
        dedupeFileLinksKeepingLastPerTarget(fileLinks_);

        if (wasDir) {
            const QString oldKey = orderKeyForDirectory(oldPath);
            const QString newKey = orderKeyForDirectory(newPath);
            std::map<QString, QStringList> updatedOrders;
            for (const auto& [key, order] : fileOrders_) {
                QString updatedKey = key;
                if (key == oldKey) {
                    updatedKey = newKey;
                } else if (!oldKey.isEmpty() && key.startsWith(oldKey + QLatin1Char('/'))) {
                    updatedKey = newKey + key.mid(oldKey.length());
                }
                updatedOrders[updatedKey] = order;
            }
            fileOrders_ = std::move(updatedOrders);
        }

        std::map<QString, FileHashCacheEntry> updatedHashCache;
        for (const auto& [key, entry] : fileHashCache_) {
            const QString updatedAbsolutePath =
                rekeyPathAfterRename(absolutePathForKey(key), oldPath, newPath, wasDir);
            updatedHashCache[relativeKeyForPath(updatedAbsolutePath)] = entry;
        }
        fileHashCache_ = std::move(updatedHashCache);
    }

QString MainWindow::rekeyPathAfterRename(const QString& path, const QString& oldPath, const QString& newPath, bool wasDir) const
{
        if (path == oldPath) {
            return newPath;
        }
        if (wasDir && isDescendantPath(oldPath, path)) {
            return newPath + path.mid(oldPath.length());
        }
        return path;
    }

bool MainWindow::updateHashCacheEntryForFile(const QFileInfo& info)
{
        if (info.size() > RenameHashMaxFileSize || isCloudPlaceholderFile(info)) {
            return false;
        }

        const QString key = relativeKeyForPath(info.absoluteFilePath());
        const qint64 mtimeMs = info.lastModified().toMSecsSinceEpoch();
        const auto existing = fileHashCache_.find(key);
        if (existing != fileHashCache_.end() && existing->second.size == info.size() &&
            existing->second.mtimeMs == mtimeMs) {
            return false;
        }

        const std::optional<QByteArray> hash = computeFileContentHash(info);
        if (!hash) {
            return false;
        }
        fileHashCache_[key] = FileHashCacheEntry{info.size(), mtimeMs, *hash};
        return true;
    }

void MainWindow::ensureHashCachedForRenameTracking(const QString& path)
{
        if (!mycelStorageEnabled_) {
            return;
        }
        if (updateHashCacheEntryForFile(QFileInfo(path))) {
            saveHashCacheFile();
        }
    }

bool MainWindow::updateHashCacheForDirectory(const QString& directoryPath)
{
        if (!mycelStorageEnabled_) {
            return false;
        }
        const QDir dir(directoryPath);
        const QFileInfoList entries =
            dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
        bool changed = false;
        for (const QFileInfo& info : entries) {
            if (isMetadataPath(info.absoluteFilePath())) {
                continue;
            }
            changed = updateHashCacheEntryForFile(info) || changed;
        }
        return changed;
    }

