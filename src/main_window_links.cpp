#include "main_window.h"

void MainWindow::createLinkedFileBeside(const QString& filePath)
{
        const QFileInfo info(filePath);
        if (!info.exists() || info.isDir()) {
            return;
        }
        if (!mycelStorageEnabled_) {
            createFileInDirectory(info.absolutePath());
            return;
        }
        beginHistoryGroup(QStringLiteral("リンク付きファイル作成"));
        const QString newPath = createFileInDirectory(info.absolutePath(), filePath);  // beside this file
        if (!newPath.isEmpty()) {
            if (Node* from = nodeForPath(filePath)) {
                if (Node* to = nodeForPath(newPath)) {
                    addFileLink(from, to);
                }
            }
        }
        endHistoryGroup();
        if (!newPath.isEmpty()) {
            selectNodePath(newPath, true);
        }
    }

void MainWindow::addFileLink(Node* from, Node* to)
{
        if (!mycelStorageEnabled_) {
            QMessageBox::information(this, QStringLiteral("Mycel"),
                                     QStringLiteral(".mycel なしの制限モードでは関連付けできません。"));
            return;
        }
        if (!from || !to || from->isDir || from->path == to->path) {  // link source must be a file; target may be a folder
            return;
        }

        const QString fromPath = from->path;
        QString toPath = to->path;
        const QString sourceDir = QFileInfo(fromPath).absolutePath();
        // A link target is displayed beside its source, which reads as same-folder membership.
        // When the dragged item lives in another real folder, move it into the source's folder
        // first so the physical location matches the visual placement. The root itself can
        // never be moved.
        const bool needsMove = to != root_.get() &&
                               QDir::cleanPath(QFileInfo(toPath).absolutePath()) != QDir::cleanPath(sourceDir);

        if (!needsMove) {
            for (const FileLink& link : fileLinks_) {
                if (link.from == fromPath && link.to == toPath) {
                    relayout();  // already linked: just restore the layout after the drag
                    return;
                }
            }
        }

        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();

        FileOperationService::MovedEntry movedEntry;
        if (needsMove) {
            pauseFileSystemWatcher();
            const FileOperationService::MoveResult result = FileOperationService::moveInto(
                {{toPath, to->isDir, false}}, sourceDir);
            recordDebugEvent(QStringLiteral("link move fs result: moved=%1 failed=%2 blocked=%3")
                                 .arg(static_cast<int>(result.moved.size()))
                                 .arg(static_cast<int>(result.failed.size()))
                                 .arg(result.blocked ? QStringLiteral("true") : QStringLiteral("false")));
            if (result.blocked) {
                QMessageBox::warning(this, QStringLiteral("Mycel"),
                                     QStringLiteral("自分自身または配下へは移動できません。"));
                rebuild(false);
                return;
            }
            if (!result.failed.empty() || result.moved.empty()) {
                QMessageBox::warning(this, QStringLiteral("Mycel"),
                                     QStringLiteral("リンク先への移動ができませんでした。\n%1")
                                         .arg(result.failed.empty() ? toPath : result.failed.front().second));
                rebuild(false);
                return;
            }
            movedEntry = result.moved.front();
            recordDebugEvent(QStringLiteral("link move metadata: %1 -> %2")
                                 .arg(relativeKeyForPath(movedEntry.oldPath), relativeKeyForPath(movedEntry.newPath)));
            // Rekeys color/preview/collapse/order metadata AND any existing links that pointed
            // at the old path, so the replace-erase below sees them under the new path.
            applyMovedMetadata(movedEntry, sourceDir, mycelStorageEnabled_ && !movedEntry.isDir);
            toPath = movedEntry.newPath;
        }

        // A link target is placed beside exactly one source, so re-linking the same file
        // replaces its previous connection instead of leaving a stale line behind.
        fileLinks_.erase(std::remove_if(fileLinks_.begin(), fileLinks_.end(),
                                        [&toPath](const FileLink& link) { return link.to == toPath; }),
                         fileLinks_.end());

        fileLinks_.push_back({fromPath, toPath});
        saveLinkFile();
        if (needsMove) {
            saveColorFile();
            savePreviewFile();
            saveCollapsedFile();
            saveHashCacheFile();
            if (mycelStorageEnabled_ && !movedEntry.isDir) {
                saveOrderFile();
            }
            rebuild(false);  // the tree structure changed (a node moved between folders)
            recordHistory(QStringLiteral("関連付け"), {{movedEntry.oldPath, movedEntry.newPath}},
                          {{movedEntry.newPath, movedEntry.oldPath}}, historyBefore, historySelection);
            return;
        }
        relayout();
        recordHistory(QStringLiteral("関連付け"), {}, {}, historyBefore, historySelection);
    }

QStringList MainWindow::linkedDescendantPaths(const QString& sourcePath) const
{
        QStringList result;
        QSet<QString> visited{sourcePath};
        QStringList frontier{sourcePath};
        while (!frontier.isEmpty()) {
            const QString current = frontier.takeFirst();
            for (const FileLink& link : fileLinks_) {
                if (link.from == current && !visited.contains(link.to)) {
                    visited.insert(link.to);
                    result.append(link.to);
                    frontier.append(link.to);
                }
            }
        }
        return result;
    }

bool MainWindow::hasIncomingFileLink(Node* node) const
{
        if (!mycelStorageEnabled_ || !node || node->isDir) {
            return false;
        }
        for (const FileLink& link : fileLinks_) {
            if (link.to == node->path) {
                return true;
            }
        }
        return false;
    }

bool MainWindow::hasIncomingFileLinkPath(const QString& path) const
{
        if (!mycelStorageEnabled_ || path.isEmpty()) {
            return false;
        }
        for (const FileLink& link : fileLinks_) {
            if (link.to == path) {
                return true;
            }
        }
        return false;
    }

void MainWindow::removeIncomingFileLinks(Node* node)
{
        if (!mycelStorageEnabled_ || !node) {  // a linked folder can be unlinked too
            return;
        }

        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        const auto oldSize = fileLinks_.size();
        fileLinks_.erase(std::remove_if(fileLinks_.begin(), fileLinks_.end(), [node](const FileLink& link) {
                             return link.to == node->path;
                         }),
                         fileLinks_.end());
        if (fileLinks_.size() == oldSize) {
            return;
        }

        saveLinkFile();
        relayout();
        recordHistory(QStringLiteral("関連付け解除"), {}, {}, historyBefore, historySelection);
    }

void MainWindow::removeIncomingFileLinksPath(const QString& path)
{
        Node* node = nodeForPath(path);
        if (node) {
            removeIncomingFileLinks(node);
        }
    }

MainWindow::FileLinkSiblingGroup MainWindow::fileLinkSiblingGroup(const QString& path) const
{
        QString from;
        bool found = false;
        for (const FileLink& link : fileLinks_) {
            if (link.to == path) {
                from = link.from;
                found = true;
                break;
            }
        }
        if (!found) {
            return {};
        }
        FileLinkSiblingGroup group;
        for (std::size_t i = 0; i < fileLinks_.size(); ++i) {
            if (fileLinks_[i].from == from) {
                if (fileLinks_[i].to == path) {
                    group.pos = static_cast<int>(group.indices.size());
                }
                group.indices.push_back(i);
            }
        }
        return group;
    }

bool MainWindow::canMoveFileLinkTargetPath(const QString& path, int direction) const
{
        if (!mycelStorageEnabled_ || path.isEmpty()) {
            return false;
        }
        const FileLinkSiblingGroup group = fileLinkSiblingGroup(path);
        if (group.pos < 0) {
            return false;
        }
        return direction < 0 ? group.pos > 0 : group.pos + 1 < static_cast<int>(group.indices.size());
    }

void MainWindow::moveFileLinkTargetPath(const QString& path, int direction)
{
        if (!mycelStorageEnabled_ || path.isEmpty()) {
            return;
        }
        const FileLinkSiblingGroup group = fileLinkSiblingGroup(path);
        if (group.pos < 0) {
            return;
        }
        const bool moveUp = direction < 0;
        if (moveUp ? group.pos == 0 : group.pos + 1 >= static_cast<int>(group.indices.size())) {
            return;
        }

        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        const std::size_t swapWith = group.indices[static_cast<std::size_t>(moveUp ? group.pos - 1 : group.pos + 1)];
        std::swap(fileLinks_[group.indices[static_cast<std::size_t>(group.pos)]], fileLinks_[swapWith]);
        saveLinkFile();
        relayout();
        recordHistory(QStringLiteral("横リンク並び替え"), {}, {}, historyBefore, historySelection);
    }

