#include "main_window.h"

bool MainWindow::boardModeActive() const
{ return boardMode_; }

QString MainWindow::boardsDirPath() const
{ return QDir(rootPath_).filePath(QStringLiteral(".mycel/boards")); }

QString MainWindow::boardPatternFilePath(const QString& name) const
{
        return QDir(boardsDirPath()).filePath(name + QStringLiteral(".json"));
    }

QStringList MainWindow::availableBoardPatternNames() const
{
        QStringList names;
        const QStringList files = QDir(boardsDirPath()).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        for (const QString& file : files) {
            names << QFileInfo(file).completeBaseName();
        }
        return names;
    }

void MainWindow::applyCustomEntryOrder(QFileInfoList& entries, const QString& dirPath) const
{
        const QString orderKey = QDir(rootPath_).relativeFilePath(dirPath);
        const QString normalizedOrderKey = orderKey == QStringLiteral(".") ? QString() : orderKey;
        const auto order = fileOrders_.find(normalizedOrderKey);
        if (order == fileOrders_.end()) {
            return;
        }
        const QStringList names = order->second;
        std::stable_sort(entries.begin(), entries.end(), [&names](const QFileInfo& a, const QFileInfo& b) {
            const int ai = names.indexOf(a.fileName());
            const int bi = names.indexOf(b.fileName());
            if (ai != -1 || bi != -1) {
                if (ai == -1) {
                    return false;
                }
                if (bi == -1) {
                    return true;
                }
                return ai < bi;
            }
            if (a.isDir() != b.isDir()) {
                return a.isDir();
            }
            return QString::compare(a.fileName(), b.fileName(), Qt::CaseInsensitive) < 0;
        });
    }

std::vector<std::pair<QString, int>> MainWindow::collectBoardFiles() const
{
        std::vector<std::pair<QString, int>> files;
        QStringList currentDirs{rootPath_};
        int folderRow = 0;
        for (int depth = 0; depth < 16 && !currentDirs.isEmpty(); ++depth) {
            QStringList nextDirs;
            for (const QString& dirPath : currentDirs) {
                QDir dir(dirPath);
                QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                          QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
                entries.erase(std::remove_if(entries.begin(), entries.end(), [](const QFileInfo& entry) {
                    return entry.fileName() == QStringLiteral(".mycel");
                }), entries.end());
                applyCustomEntryOrder(entries, dirPath);
                for (const QFileInfo& entry : entries) {
                    if (entry.isDir()) {
                        if (!directoryHasMycel(entry.absoluteFilePath())) {
                            nextDirs << entry.absoluteFilePath();
                        }
                    } else {
                        files.push_back({entry.absoluteFilePath(), folderRow});
                    }
                }
                ++folderRow;  // each folder owns its own row
            }
            currentDirs = nextDirs;
        }
        return files;
    }

std::unique_ptr<Node> MainWindow::makeBoardNode(const QString& path, const QPointF& center) const
{
        const QFileInfo info(path);
        auto node = std::make_unique<Node>();
        node->path = info.absoluteFilePath();
        node->parentPath = info.absoluteDir().absolutePath();
        node->name = info.fileName();
        node->isDir = false;
        node->depth = 1;
        node->previewOpen = previewPaths_.contains(node->path);
        QFont font;
        font.setPointSize(11);
        const QFontMetricsF metrics(font);
        node->size = QSizeF(std::max(104.0, metrics.horizontalAdvance(shortLabel(node->name)) + 30.0 + 34.0), 30.0);
        if (node->previewOpen) {
            const auto found = previewSizes_.find(node->path);
            node->previewSize = found == previewSizes_.end() ? automaticPreviewSize(info) : found->second;
        }
        node->center = center;
        return node;
    }

void MainWindow::createBoardPattern(const QString& name)
{
        boardCards_.clear();
        constexpr qreal HGap = 48.0;  // horizontal gap between cards in a row
        constexpr qreal VGap = 70.0;  // vertical gap between rows
        int currentRow = 0;
        qreal x = 0.0;
        qreal rowTop = 0.0;
        qreal rowMaxHeight = 0.0;
        for (const auto& [path, row] : collectBoardFiles()) {
            const auto node = makeBoardNode(path, QPointF());
            const qreal visualWidth = node->previewOpen ? std::max(node->size.width(), node->previewSize.width())
                                                        : node->size.width();
            const qreal visualHeight = node->previewOpen ? 34.0 + node->previewSize.height() : node->size.height();
            if (row != currentRow) {
                rowTop += rowMaxHeight + VGap;
                x = 0.0;
                rowMaxHeight = 0.0;
                currentRow = row;
            }
            BoardCardState state;
            // The item position is the card-header centre; the visual (header + preview) hangs
            // down-right from the top-left corner, so anchoring by top-left aligns the row tops.
            state.pos = QPointF(x + node->size.width() / 2.0, rowTop + node->size.height() / 2.0);
            state.visible = true;
            boardCards_[relativeKeyForPath(path)] = state;
            x += visualWidth + HGap;
            rowMaxHeight = std::max(rowMaxHeight, visualHeight);
        }
        boardPatternName_ = name;
        boardPatternView_ = QJsonObject();
        saveBoardPattern();
        recordDebugEvent(QStringLiteral("board pattern created: %1 (%2 cards)").arg(name).arg(boardCards_.size()));
    }

void MainWindow::saveBoardPattern()
{
        if (!mycelStorageEnabled_ || boardPatternName_.isEmpty()) {
            return;
        }
        QJsonObject cards;
        for (const auto& [key, state] : boardCards_) {
            QJsonObject card;
            card.insert(QStringLiteral("x"), state.pos.x());
            card.insert(QStringLiteral("y"), state.pos.y());
            card.insert(QStringLiteral("visible"), state.visible);
            cards.insert(key, card);
        }
        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("cards"), cards);
        if (boardMode_ && view_) {
            QJsonObject viewObject;
            viewObject.insert(QStringLiteral("scale"), view_->transform().m11());
            const QPointF center = view_->mapToScene(view_->viewport()->rect().center());
            viewObject.insert(QStringLiteral("centerX"), center.x());
            viewObject.insert(QStringLiteral("centerY"), center.y());
            boardPatternView_ = viewObject;
        }
        if (!boardPatternView_.isEmpty()) {
            rootObject.insert(QStringLiteral("view"), boardPatternView_);
        }
        writeJsonFileAtomic(boardPatternFilePath(boardPatternName_), rootObject);
    }

bool MainWindow::loadBoardPattern(const QString& name)
{
        QFile file(boardPatternFilePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        const QJsonObject rootObject = QJsonDocument::fromJson(file.readAll()).object();
        boardCards_.clear();
        const QJsonObject cards = rootObject.value(QStringLiteral("cards")).toObject();
        for (auto it = cards.begin(); it != cards.end(); ++it) {
            const QJsonObject card = it.value().toObject();
            BoardCardState state;
            state.pos = QPointF(card.value(QStringLiteral("x")).toDouble(0.0),
                                card.value(QStringLiteral("y")).toDouble(0.0));
            state.visible = card.value(QStringLiteral("visible")).toBool(true);
            boardCards_[it.key()] = state;
        }
        boardPatternName_ = name;
        boardPatternView_ = rootObject.value(QStringLiteral("view")).toObject();
        return true;
    }

void MainWindow::ensureBoardPattern()
{
        if (!boardPatternName_.isEmpty() && loadBoardPattern(boardPatternName_)) {
            return;
        }
        const QStringList names = availableBoardPatternNames();
        if (!names.isEmpty() && loadBoardPattern(names.first())) {
            return;
        }
        createBoardPattern(QStringLiteral("パターン1"));
    }

void MainWindow::renderBoardScene(bool restoreView)
{
        saveSideEditorNow();
        finishInlineRename(false);
        const QTransform previousTransform = view_->transform();
        const int previousHScroll = view_->horizontalScrollBar()->value();
        const int previousVScroll = view_->verticalScrollBar()->value();
        const bool previousSuppress = suppressSideEditorSelectionUpdate_;
        suppressSideEditorSelectionUpdate_ = true;
        resetDropHoverState();
        if (view_) {
            view_->cancelTransientNodeInteraction();
        }
        nodeItemsByPath_.clear();
        connectionLayer_ = nullptr;
        scene_.clear();
        boardNodes_.clear();

        QRectF bounds;
        for (const auto& [key, state] : boardCards_) {
            if (!state.visible) {
                continue;  // hidden: only shown again when called in
            }
            const QString path = absolutePathForKey(key);
            if (!QFileInfo::exists(path)) {
                continue;  // real file is gone: drop from display (entry is kept)
            }
            auto node = makeBoardNode(path, state.pos);
            auto* item = new NodeItem(node.get(), this);
            scene_.addItem(item);
            nodeItemsByPath_.insert(node->path, item);
            bounds = bounds.united(item->sceneBoundingRect());
            boardNodes_.push_back(std::move(node));
        }
        suppressSideEditorSelectionUpdate_ = previousSuppress;

        if (bounds.isNull()) {
            bounds = QRectF(-400.0, -300.0, 800.0, 600.0);
        }
        // Generous margins: the working area is meant to feel unbounded.
        scene_.setSceneRect(bounds.adjusted(-4000.0, -4000.0, 4000.0, 4000.0));
        if (restoreView) {
            restoreBoardViewOrFit();
        } else {
            view_->setTransform(previousTransform);
            view_->horizontalScrollBar()->setValue(previousHScroll);
            view_->verticalScrollBar()->setValue(previousVScroll);
        }
        resetFileSystemWatcher();
    }

void MainWindow::restoreBoardViewOrFit()
{
        const double scale = boardPatternView_.value(QStringLiteral("scale")).toDouble(0.0);
        if (scale > 0.001) {
            restoringViewState_ = true;
            QTransform transform;
            transform.scale(scale, scale);
            view_->setTransform(transform);
            view_->centerOn(QPointF(boardPatternView_.value(QStringLiteral("centerX")).toDouble(0.0),
                                    boardPatternView_.value(QStringLiteral("centerY")).toDouble(0.0)));
            restoringViewState_ = false;
        } else {
            fitToMap();
        }
    }

void MainWindow::refreshBoardCards()
{
        saveSideEditorNow();
        finishInlineRename(false);
        QRectF bounds;
        for (auto& node : boardNodes_) {
            node->previewOpen = previewPaths_.contains(node->path);
            if (node->previewOpen) {
                const auto found = previewSizes_.find(node->path);
                node->previewSize = found == previewSizes_.end() ? automaticPreviewSize(QFileInfo(node->path))
                                                                 : found->second;
            }
            if (NodeItem* item = nodeItemsByPath_.value(node->path, nullptr)) {
                item->syncFromNode();
                bounds = bounds.united(item->sceneBoundingRect());
            }
        }
        if (!bounds.isNull()) {
            scene_.setSceneRect(scene_.sceneRect().united(bounds.adjusted(-200.0, -200.0, 200.0, 200.0)));
        }
        scene_.update();
    }

void MainWindow::boardCardMoved(NodeItem* item)
{
        if (!boardMode_ || !item || !item->node()) {
            return;
        }
        const QString key = relativeKeyForPath(item->node()->path);
        const auto it = boardCards_.find(key);
        if (it == boardCards_.end()) {
            return;
        }
        if ((it->second.pos - item->pos()).manhattanLength() < 0.5) {
            return;  // no real movement
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();  // still holds the old pos
        const QStringList historySelection = selectedNodePaths();
        it->second.pos = item->pos();
        item->syncFromNode();  // adopt the new position as the layout center
        scene_.setSceneRect(scene_.sceneRect().united(item->sceneBoundingRect().adjusted(-4000.0, -4000.0, 4000.0, 4000.0)));
        saveBoardPattern();
        recordHistory(QStringLiteral("カード移動"), {}, {}, historyBefore, historySelection);
        recordDebugEvent(QStringLiteral("board card moved: %1").arg(key));
    }

void MainWindow::hideBoardCard(const QString& path)
{
        if (!boardMode_) {
            return;
        }
        const auto it = boardCards_.find(relativeKeyForPath(path));
        if (it == boardCards_.end()) {
            return;
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        it->second.visible = false;
        saveBoardPattern();
        renderBoardScene(false);
        recordHistory(QStringLiteral("カード非表示"), {}, {}, historyBefore, historySelection);
        recordDebugEvent(QStringLiteral("board card hidden: %1").arg(relativeKeyForPath(path)));
    }

void MainWindow::setBoardMode(bool on, bool persistCurrent)
{
        if (boardMode_ == on) {
            return;
        }
        if (on && !mycelStorageEnabled_) {
            QMessageBox::information(this, QStringLiteral("Mycel"),
                                     QStringLiteral(".mycel なしの制限モードではボード表示を使用できません。"));
            if (boardModeAction_) {
                boardModeAction_->setChecked(false);
            }
            return;
        }
        if (persistCurrent) {
            saveViewState();  // persists the outgoing mode (board pattern included)
        }
        boardMode_ = on;
        if (boardModeAction_) {
            boardModeAction_->setChecked(on);
        }
        if (on) {
            ensureBoardPattern();
            renderBoardScene(true);
        } else {
            rebuild(true);  // rescan and restore the tree view
        }
        updateBoardPatternMenu();
        saveViewState();  // persist the new mode flag
        recordDebugEvent(QStringLiteral("board mode: %1 pattern=%2")
                             .arg(on ? QStringLiteral("on") : QStringLiteral("off"), boardPatternName_));
    }

void MainWindow::switchBoardPattern(const QString& name)
{
        if (!boardMode_) {
            boardPatternName_ = name;
            setBoardMode(true);
            return;
        }
        if (name == boardPatternName_) {
            return;
        }
        saveBoardPattern();
        if (loadBoardPattern(name)) {
            renderBoardScene(true);
            saveViewState();
        }
        updateBoardPatternMenu();
    }

void MainWindow::createBoardPatternInteractive()
{
        bool ok = false;
        QString name = QInputDialog::getText(this, QStringLiteral("Mycel"), QStringLiteral("新しいパターン名:"),
                                             QLineEdit::Normal,
                                             QStringLiteral("パターン%1").arg(availableBoardPatternNames().size() + 1),
                                             &ok)
                           .trimmed();
        if (!ok || name.isEmpty()) {
            return;
        }
        name.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));  // keep it file-name safe
        if (name.isEmpty()) {
            return;
        }
        if (availableBoardPatternNames().contains(name)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("同名のパターンが既にあります。"));
            return;
        }
        if (boardMode_) {
            saveBoardPattern();
        }
        createBoardPattern(name);
        if (boardMode_) {
            renderBoardScene(true);
            saveViewState();
            updateBoardPatternMenu();
        } else {
            setBoardMode(true);
        }
    }

void MainWindow::renameBoardPatternInteractive()
{
        if (!boardMode_ || boardPatternName_.isEmpty()) {
            return;
        }
        bool ok = false;
        QString name = QInputDialog::getText(this, QStringLiteral("Mycel"), QStringLiteral("新しいパターン名:"),
                                             QLineEdit::Normal, boardPatternName_, &ok)
                           .trimmed();
        if (!ok || name.isEmpty() || name == boardPatternName_) {
            return;
        }
        name.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));
        if (name.isEmpty()) {
            return;
        }
        if (availableBoardPatternNames().contains(name)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("同名のパターンが既にあります。"));
            return;
        }
        saveBoardPattern();
        if (!QFile::rename(boardPatternFilePath(boardPatternName_), boardPatternFilePath(name))) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("パターン名を変更できませんでした。"));
            return;
        }
        boardPatternName_ = name;
        saveViewState();
        updateBoardPatternMenu();
    }

void MainWindow::deleteBoardPatternInteractive()
{
        if (!boardMode_ || boardPatternName_.isEmpty()) {
            return;
        }
        const auto reply = QMessageBox::question(
            this, QStringLiteral("Mycel"),
            QStringLiteral("ボードパターン「%1」を削除しますか？\n（ファイルの実体は削除されません）")
                .arg(boardPatternName_),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
        QFile::remove(boardPatternFilePath(boardPatternName_));
        recordDebugEvent(QStringLiteral("board pattern deleted: %1").arg(boardPatternName_));
        boardPatternName_.clear();
        boardCards_.clear();
        boardPatternView_ = QJsonObject();
        const QStringList names = availableBoardPatternNames();
        if (!names.isEmpty() && loadBoardPattern(names.first())) {
            renderBoardScene(true);
        } else {
            // No pattern left: fall back to the tree.
            boardMode_ = false;
            if (boardModeAction_) {
                boardModeAction_->setChecked(false);
            }
            rebuild(true);
        }
        saveViewState();
        updateBoardPatternMenu();
    }

void MainWindow::showBoardHiddenCardsDialog()
{
        if (!boardMode_) {
            QMessageBox::information(this, QStringLiteral("Mycel"),
                                     QStringLiteral("ボード表示中のみ使用できます。"));
            return;
        }
        struct Entry {
            QString key;
            bool hidden = false;  // false = not yet placed (new file)
        };
        std::vector<Entry> entries;
        for (const auto& [key, state] : boardCards_) {
            if (!state.visible && QFileInfo::exists(absolutePathForKey(key))) {
                entries.push_back({key, true});
            }
        }
        for (const auto& [path, row] : collectBoardFiles()) {
            Q_UNUSED(row);
            const QString key = relativeKeyForPath(path);
            if (boardCards_.find(key) == boardCards_.end()) {
                entries.push_back({key, false});
            }
        }
        if (entries.empty()) {
            QMessageBox::information(this, QStringLiteral("Mycel"),
                                     QStringLiteral("非表示・未配置のカードはありません。"));
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("非表示カード一覧"));
        dialog.resize(560, 440);
        auto* layout = new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel(QStringLiteral("表示するカードを選択してください（複数選択可）"), &dialog));
        auto* list = new QListWidget(&dialog);
        list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        for (const Entry& entry : entries) {
            auto* item = new QListWidgetItem(
                QStringLiteral("%1  %2").arg(QDir::toNativeSeparators(entry.key),
                                             entry.hidden ? QStringLiteral("（非表示）") : QStringLiteral("（未配置）")),
                list);
            item->setData(Qt::UserRole, entry.key);
            item->setData(Qt::UserRole + 1, entry.hidden);
        }
        layout->addWidget(list, 1);
        auto* buttonRow = new QHBoxLayout();
        auto* showButton = new QPushButton(QStringLiteral("表示する"), &dialog);
        auto* closeButton = new QPushButton(QStringLiteral("閉じる"), &dialog);
        buttonRow->addStretch(1);
        buttonRow->addWidget(showButton);
        buttonRow->addWidget(closeButton);
        layout->addLayout(buttonRow);
        connect(showButton, &QPushButton::clicked, &dialog, &QDialog::accept);
        connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(list, &QListWidget::itemDoubleClicked, &dialog, [&dialog](QListWidgetItem*) { dialog.accept(); });
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        const QList<QListWidgetItem*> selectedItems = list->selectedItems();
        if (selectedItems.isEmpty()) {
            return;
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        const QPointF spawn = view_->mapToScene(view_->viewport()->rect().center());
        int placed = 0;
        for (QListWidgetItem* item : selectedItems) {
            const QString key = item->data(Qt::UserRole).toString();
            const bool hidden = item->data(Qt::UserRole + 1).toBool();
            if (hidden) {
                const auto it = boardCards_.find(key);
                if (it != boardCards_.end()) {
                    it->second.visible = true;  // back to its remembered position
                }
            } else {
                BoardCardState state;
                state.pos = spawn + QPointF(44.0 * placed, 36.0 * placed);  // stagger new cards
                state.visible = true;
                boardCards_[key] = state;
                ++placed;
            }
        }
        saveBoardPattern();
        renderBoardScene(false);
        recordHistory(QStringLiteral("カード表示"), {}, {}, historyBefore, historySelection);
    }

void MainWindow::updateBoardPatternMenu()
{
        if (!boardPatternMenu_) {
            return;
        }
        boardPatternMenu_->clear();
        QAction* newAction = boardPatternMenu_->addAction(QStringLiteral("新規パターン作成…"));
        connect(newAction, &QAction::triggered, this, [this] { createBoardPatternInteractive(); });
        QAction* renameAction = boardPatternMenu_->addAction(QStringLiteral("パターン名変更…"));
        renameAction->setEnabled(boardMode_ && !boardPatternName_.isEmpty());
        connect(renameAction, &QAction::triggered, this, [this] { renameBoardPatternInteractive(); });
        QAction* deleteAction = boardPatternMenu_->addAction(QStringLiteral("パターン削除…"));
        deleteAction->setEnabled(boardMode_ && !boardPatternName_.isEmpty());
        connect(deleteAction, &QAction::triggered, this, [this] { deleteBoardPatternInteractive(); });
        const QStringList names = availableBoardPatternNames();
        if (!names.isEmpty()) {
            boardPatternMenu_->addSeparator();
        }
        for (const QString& name : names) {
            QAction* action = boardPatternMenu_->addAction(name);
            action->setCheckable(true);
            action->setChecked(boardMode_ && name == boardPatternName_);
            connect(action, &QAction::triggered, this, [this, name] { switchBoardPattern(name); });
        }
    }

