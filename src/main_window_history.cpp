#include "main_window.h"

QString MainWindow::historyTrashDir() const
{
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/trash"));
    }

void MainWindow::cleanHistoryTrash()
{
        QDir trash(historyTrashDir());
        if (trash.exists()) {
            trash.removeRecursively();
        }
        historyTrashCounter_ = 0;
    }

QString MainWindow::allocateTrashPath(const QString& name)
{
        QString slotDir = QDir(historyTrashDir()).filePath(QString::number(++historyTrashCounter_));
        while (QFileInfo::exists(slotDir)) {
            slotDir = QDir(historyTrashDir()).filePath(QString::number(++historyTrashCounter_));
        }
        QDir().mkpath(slotDir);
        return QDir(slotDir).filePath(name);
    }

QString MainWindow::moveToTrash(const QString& path)
{
        pauseFileSystemWatcher();
        const QString trashPath = allocateTrashPath(QFileInfo(path).fileName());
        std::error_code ec;
        std::filesystem::rename(std::filesystem::u8path(path.toStdString()),
                                std::filesystem::u8path(trashPath.toStdString()), ec);
        if (ec) {
            recordDebugEvent(QStringLiteral("trash move failed: %1 : %2")
                                 .arg(relativeKeyForPath(path), QString::fromStdString(ec.message())));
            return QString();
        }
        return trashPath;
    }

bool MainWindow::applyHistoryMoves(const std::vector<std::pair<QString, QString>>& moves)
{
        if (!moves.empty()) {
            pauseFileSystemWatcher();
        }
        bool ok = true;
        for (const auto& move : moves) {
            if (move.first.isEmpty() || move.second.isEmpty()) {
                continue;
            }
            QDir().mkpath(QFileInfo(move.second).absolutePath());
            std::error_code ec;
            std::filesystem::rename(std::filesystem::u8path(move.first.toStdString()),
                                    std::filesystem::u8path(move.second.toStdString()), ec);
            if (ec) {
                ok = false;
                recordDebugEvent(QStringLiteral("history move failed: %1 -> %2 : %3")
                                     .arg(relativeKeyForPath(move.first), relativeKeyForPath(move.second),
                                          QString::fromStdString(ec.message())));
            }
        }
        return ok;
    }

void MainWindow::pushHistoryEntry(HistoryEntry entry)
{
        redoStack_.clear();
        undoStack_.push_back(std::move(entry));
        while (static_cast<int>(undoStack_.size()) > kMaxHistoryEntries) {
            undoStack_.erase(undoStack_.begin());
        }
        updateUndoRedoActions();
    }

void MainWindow::recordHistory(const QString& description,
                       std::vector<std::pair<QString, QString>> redoMoves,
                       std::vector<std::pair<QString, QString>> undoMoves,
                       const MetadataSnapshot& before, const QStringList& selectionBefore)
{
        if (applyingHistory_) {
            return;
        }
        if (historyGroup_) {
            for (auto& move : redoMoves) {
                historyGroup_->redoMoves.push_back(std::move(move));
            }
            for (auto& move : undoMoves) {
                historyGroup_->undoMoves.push_back(std::move(move));
            }
            return;
        }
        HistoryEntry entry;
        entry.description = description;
        entry.redoMoves = std::move(redoMoves);
        entry.undoMoves = std::move(undoMoves);
        entry.before = before;
        entry.selectionBefore = selectionBefore;
        entry.after = captureMetadataSnapshot();
        entry.selectionAfter = selectedNodePaths();
        pushHistoryEntry(std::move(entry));
    }

void MainWindow::beginHistoryGroup(const QString& description)
{
        if (applyingHistory_ || historyGroup_) {
            return;
        }
        historyGroup_ = std::make_unique<HistoryEntry>();
        historyGroup_->description = description;
        historyGroup_->before = captureMetadataSnapshot();
        historyGroup_->selectionBefore = selectedNodePaths();
    }

void MainWindow::endHistoryGroup()
{
        if (!historyGroup_) {
            return;
        }
        std::unique_ptr<HistoryEntry> group = std::move(historyGroup_);
        historyGroup_.reset();
        group->after = captureMetadataSnapshot();
        group->selectionAfter = selectedNodePaths();
        pushHistoryEntry(std::move(*group));
    }

void MainWindow::performUndo()
{
        if (renameEdit_ || sideEditorEditing_ || undoStack_.empty()) {
            return;
        }
        HistoryEntry entry = std::move(undoStack_.back());
        undoStack_.pop_back();
        applyingHistory_ = true;
        applyHistoryMoves(entry.undoMoves);
        restoreMetadataSnapshot(entry.before);
        saveAllMetadata();
        rebuild(false);
        restoreSelection(entry.selectionBefore, true);
        applyingHistory_ = false;
        recordDebugEvent(QStringLiteral("undo: %1").arg(entry.description));
        redoStack_.push_back(std::move(entry));
        updateUndoRedoActions();
    }

void MainWindow::performRedo()
{
        if (renameEdit_ || sideEditorEditing_ || redoStack_.empty()) {
            return;
        }
        HistoryEntry entry = std::move(redoStack_.back());
        redoStack_.pop_back();
        applyingHistory_ = true;
        applyHistoryMoves(entry.redoMoves);
        restoreMetadataSnapshot(entry.after);
        saveAllMetadata();
        rebuild(false);
        restoreSelection(entry.selectionAfter, true);
        applyingHistory_ = false;
        recordDebugEvent(QStringLiteral("redo: %1").arg(entry.description));
        undoStack_.push_back(std::move(entry));
        updateUndoRedoActions();
    }

void MainWindow::updateUndoRedoActions()
{
        if (undoAction_) {
            const bool can = !undoStack_.empty();
            undoAction_->setEnabled(can);
            undoAction_->setToolTip(can ? QStringLiteral("元に戻す: %1").arg(undoStack_.back().description)
                                        : QStringLiteral("元に戻す"));
        }
        if (redoAction_) {
            const bool can = !redoStack_.empty();
            redoAction_->setEnabled(can);
            redoAction_->setToolTip(can ? QStringLiteral("やり直す: %1").arg(redoStack_.back().description)
                                        : QStringLiteral("やり直す"));
        }
    }

