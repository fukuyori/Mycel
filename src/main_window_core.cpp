#include "main_window.h"

MainWindow::MainWindow(QString rootPath, bool mycelStorageEnabled, QWidget* parent)
        : QMainWindow(parent), rootPath_(std::move(rootPath)), mycelStorageEnabled_(mycelStorageEnabled)
{
        resize(1280, 820);
        uiTheme_ = appThemeFromString(QSettings().value(QStringLiteral("ui/theme"), QStringLiteral("light")).toString());
        if (qApp) {
            qApp->setProperty("mycelTheme", appThemeToString(uiTheme_));
        }

        editorSplitter_ = new QSplitter(Qt::Horizontal, this);

        view_ = new BoardView(editorSplitter_);
        view_->setScene(&scene_);
        view_->setCheatSheetHandler([this] { showCheatSheet(); });
        view_->setKeyHandler([this](QKeyEvent* event) { return handleBoardShortcut(event); });
        view_->setViewChangedHandler([this] { scheduleViewStateSave(); });
        view_->setDebugHandler([this](const QString& message) { recordDebugEvent(message); });
        recordDebugEvent(QStringLiteral("main window constructed: root=%1 mycelStorage=%2")
                             .arg(rootPath_,
                                  mycelStorageEnabled_ ? QStringLiteral("true") : QStringLiteral("false")));

        sideEditorPanel_ = new QWidget(editorSplitter_);
        sideEditorPanel_->setObjectName(QStringLiteral("SideEditorPanel"));
        auto* editorLayout = new QVBoxLayout(sideEditorPanel_);
        editorLayout->setContentsMargins(10, 8, 10, 8);
        editorLayout->setSpacing(6);
        sideModeLabel_ = new QLabel(sideEditorPanel_);
        sideModeLabel_->setAlignment(Qt::AlignCenter);
        sideModeLabel_->setFixedHeight(24);
        sideEditorPathLabel_ = new QLabel(sideEditorPanel_);
        sideEditorPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        sideEditorPathLabel_->setWordWrap(true);
        sideEditorStatusLabel_ = new QLabel(sideEditorPanel_);
        sidePreviewStack_ = new QStackedWidget(sideEditorPanel_);
        sidePreviewText_ = new PreviewText(sidePreviewStack_);
        sidePreviewText_->setReadOnly(true);
        sidePreviewText_->setUndoRedoEnabled(false);
        sidePreviewText_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        sidePreviewText_->setLineWrapMode(QTextEdit::WidgetWidth);
        sidePreviewImage_ = new AspectImagePreview(sidePreviewStack_);
        sidePreviewImage_->setAlignment(Qt::AlignCenter);
        // sideHtmlWeb_ (QtWebEngine) is created lazily on first HTML preview — see
        // ensureHtmlPreviewView(). Initializing Chromium at startup is the heaviest part of launch
        // and is only needed for HTML files, so it is deferred.
        sidePreviewVideo_ = new QVideoWidget(sidePreviewStack_);
        sideEditor_ = new TextEditor(sideEditorPanel_);
        sideEditor_->setReadOnly(true);
        sideEditor_->setPlaceholderText({});
        sidePreviewText_->setStyleSheet(QStringLiteral(
            "QTextEdit { background: #f7fbff; color: #172321; border: 1px solid #b7d4f2; "
            "border-radius: 4px; padding: 8px; }"));
        sideEditor_->setStyleSheet(QStringLiteral(
            "QPlainTextEdit { background: #f4fbf7; color: #172321; border: 2px solid #2f8f68; "
            "border-radius: 4px; padding: 8px; selection-background-color: #2f7de1; "
            "selection-color: #ffffff; }"));
        sidePreviewStack_->addWidget(sidePreviewText_);
        sidePreviewStack_->addWidget(sidePreviewImage_);
        if (sidePreviewVideo_) {
            sidePreviewStack_->addWidget(sidePreviewVideo_);
        }
        sidePreviewStack_->addWidget(sideEditor_);
        sidePreviewText_->installEventFilter(this);
        sidePreviewText_->viewport()->installEventFilter(this);
        sidePreviewImage_->installEventFilter(this);
        if (sidePreviewVideo_) {
            sidePreviewVideo_->installEventFilter(this);
            sidePreviewAudioOutput_ = new QAudioOutput(this);
            sidePreviewAudioOutput_->setMuted(true);
            sidePreviewPlayer_ = new QMediaPlayer(this);
            sidePreviewPlayer_->setAudioOutput(sidePreviewAudioOutput_);
            sidePreviewPlayer_->setVideoOutput(sidePreviewVideo_);
        }
        editorLayout->addWidget(sideModeLabel_);
        editorLayout->addWidget(sideEditorPathLabel_);
        editorLayout->addWidget(sideEditorStatusLabel_);
        editorLayout->addWidget(sidePreviewStack_, 1);

        editorSplitter_->addWidget(view_);
        editorSplitter_->addWidget(sideEditorPanel_);
        setCentralWidget(editorSplitter_);

        debugDock_ = new QDockWidget(QStringLiteral("Debug"), this);
        debugDock_->setObjectName(QStringLiteral("DebugDock"));
        auto* debugPanel = new QWidget(debugDock_);
        auto* debugLayout = new QVBoxLayout(debugPanel);
        debugLayout->setContentsMargins(6, 6, 6, 6);
        debugLayout->setSpacing(4);
        auto* copyDebugButton = new QPushButton(QStringLiteral("Copy"), debugPanel);
        copyDebugButton->setFixedWidth(88);
        debugText_ = new QPlainTextEdit(debugPanel);
        debugText_->setReadOnly(true);
        debugText_->setFocusPolicy(Qt::StrongFocus);
        debugText_->setMaximumBlockCount(600);
        debugText_->setStyleSheet(QStringLiteral(
            "QPlainTextEdit { background: #111827; color: #e5e7eb; border: none; "
            "font-family: Menlo, Consolas, monospace; font-size: 11px; }"));
        debugLayout->addWidget(copyDebugButton, 0, Qt::AlignLeft);
        debugLayout->addWidget(debugText_, 1);
        debugDock_->setWidget(debugPanel);
        addDockWidget(Qt::BottomDockWidgetArea, debugDock_);
        debugDock_->hide();

        // ---- Actions (shared by the menu bar and the toolbar) ----
        undoAction_ = new QAction(QStringLiteral("元に戻す"), this);
        undoAction_->setShortcut(QKeySequence::Undo);
        undoAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        undoAction_->setEnabled(false);
        view_->addAction(undoAction_);
        connect(undoAction_, &QAction::triggered, this, [this] { performUndo(); });
        redoAction_ = new QAction(QStringLiteral("やり直す"), this);
        redoAction_->setShortcut(QKeySequence::Redo);
        redoAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        redoAction_->setEnabled(false);
        view_->addAction(redoAction_);
        connect(redoAction_, &QAction::triggered, this, [this] { performRedo(); });

        QAction* openAction = new QAction(QStringLiteral("フォルダを開く"), this);
        QAction* exportAction = new QAction(QStringLiteral("エクスポート"), this);
        QAction* importAction = new QAction(QStringLiteral("インポート"), this);
        QAction* quitAction = new QAction(QStringLiteral("終了"), this);
        quitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
        quitAction->setShortcutContext(Qt::ApplicationShortcut);
        addAction(quitAction);

        QAction* refreshAction = new QAction(QStringLiteral("更新"), this);
        refreshAction->setShortcut(QKeySequence(Qt::Key_F5));
        QAction* fitAction = new QAction(QStringLiteral("全体表示"), this);
        fitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
        QAction* maximizeAction = new QAction(QStringLiteral("最大化"), this);
        maximizeAction->setShortcut(QKeySequence(Qt::Key_F11));
        maximizeAction->setShortcutContext(Qt::ApplicationShortcut);
        addAction(maximizeAction);
        QAction* openSelectedPreviewsAction = new QAction(QStringLiteral("プレビューを開く"), this);
        QAction* closeSelectedPreviewsAction = new QAction(QStringLiteral("プレビューを閉じる"), this);
        editorPaneAction_ = new QAction(QStringLiteral("プレビューペイン"), this);
        editorPaneAction_->setCheckable(true);
        editorPaneAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
        editorPaneAction_->setShortcutContext(Qt::ApplicationShortcut);
        editorPaneAction_->setChecked(QSettings().value(QStringLiteral("editor/paneVisible"), true).toBool());
        sideEditorPanel_->setVisible(editorPaneAction_->isChecked());
        debugPaneAction_ = new QAction(QStringLiteral("デバッグ"), this);
        debugPaneAction_->setCheckable(true);
        debugPaneAction_->setShortcut(QKeySequence(QStringLiteral("F12")));
        debugPaneAction_->setShortcutContext(Qt::ApplicationShortcut);

        QAction* renameSelectedAction = new QAction(this);
        renameSelectedAction->setShortcut(QKeySequence(Qt::Key_F2));
        renameSelectedAction->setShortcutContext(Qt::ApplicationShortcut);
        addAction(renameSelectedAction);

        auto* editorPositionGroup = new QActionGroup(this);
        editorPositionGroup->setExclusive(true);
        QAction* editorLeftAction = new QAction(QStringLiteral("左"), this);
        QAction* editorRightAction = new QAction(QStringLiteral("右"), this);
        QAction* editorBottomAction = new QAction(QStringLiteral("下"), this);
        for (QAction* action : {editorLeftAction, editorRightAction, editorBottomAction}) {
            action->setCheckable(true);
            editorPositionGroup->addAction(action);
        }
        editorLeftAction->setData(QStringLiteral("left"));
        editorRightAction->setData(QStringLiteral("right"));
        editorBottomAction->setData(QStringLiteral("bottom"));

        auto* themeGroup = new QActionGroup(this);
        themeGroup->setExclusive(true);
        lightThemeAction_ = new QAction(QStringLiteral("ライト"), this);
        darkThemeAction_ = new QAction(QStringLiteral("ダーク"), this);
        for (QAction* action : {lightThemeAction_, darkThemeAction_}) {
            action->setCheckable(true);
            themeGroup->addAction(action);
        }
        lightThemeAction_->setData(QStringLiteral("light"));
        darkThemeAction_->setData(QStringLiteral("dark"));

        // ---- Menu bar: ファイル / 編集 / 表示 / 設定 ----
        QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("ファイル"));
        fileMenu->addAction(openAction);
        QMenu* recentMenu = fileMenu->addMenu(QStringLiteral("履歴から開く"));
        connect(recentMenu, &QMenu::aboutToShow, this, [this, recentMenu] {
            recentMenu->clear();
            bool any = false;
            for (const QString& path : recentRootPaths()) {
                if (QDir::cleanPath(path) == QDir::cleanPath(rootPath_)) {
                    continue;  // skip the folder already open
                }
                QAction* item = recentMenu->addAction(QDir::toNativeSeparators(path));
                connect(item, &QAction::triggered, this, [this, path] { openRootFolder(path); });
                any = true;
            }
            if (!any) {
                QAction* none = recentMenu->addAction(QStringLiteral("(履歴なし)"));
                none->setEnabled(false);
            }
        });
        fileMenu->addSeparator();
        fileMenu->addAction(exportAction);
        fileMenu->addAction(importAction);
        fileMenu->addSeparator();
        fileMenu->addAction(quitAction);

        QMenu* editMenu = menuBar()->addMenu(QStringLiteral("編集"));
        editMenu->addAction(undoAction_);
        editMenu->addAction(redoAction_);

        QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("表示"));
        viewMenu->addAction(refreshAction);
        viewMenu->addAction(fitAction);
        viewMenu->addAction(maximizeAction);
        viewMenu->addSeparator();
        boardModeAction_ = viewMenu->addAction(QStringLiteral("ボード表示"));
        boardModeAction_->setCheckable(true);
        boardModeAction_->setEnabled(mycelStorageEnabled_);
        connect(boardModeAction_, &QAction::triggered, this, [this](bool on) { setBoardMode(on); });
        boardPatternMenu_ = viewMenu->addMenu(QStringLiteral("ボードパターン"));
        boardPatternMenu_->setEnabled(mycelStorageEnabled_);
        updateBoardPatternMenu();
        QAction* boardHiddenListAction = viewMenu->addAction(QStringLiteral("非表示カード一覧…"));
        boardHiddenListAction->setEnabled(mycelStorageEnabled_);
        connect(boardHiddenListAction, &QAction::triggered, this, [this] { showBoardHiddenCardsDialog(); });
        viewMenu->addSeparator();
        viewMenu->addAction(openSelectedPreviewsAction);
        viewMenu->addAction(closeSelectedPreviewsAction);
        viewMenu->addSeparator();
        viewMenu->addAction(editorPaneAction_);
        viewMenu->addAction(debugPaneAction_);

        QMenu* settingsMenu = menuBar()->addMenu(QStringLiteral("設定"));
        QMenu* themeMenu = settingsMenu->addMenu(QStringLiteral("テーマ"));
        themeMenu->addAction(lightThemeAction_);
        themeMenu->addAction(darkThemeAction_);
        QMenu* positionMenu = settingsMenu->addMenu(QStringLiteral("プレビュー位置"));
        positionMenu->addAction(editorLeftAction);
        positionMenu->addAction(editorRightAction);
        positionMenu->addAction(editorBottomAction);

        // ---- Toolbar: ファイル → 履歴 → 表示 → 設定 ----
        auto* toolbar = addToolBar(QStringLiteral("Mycel"));
        toolbar->setMovable(false);
        toolbar->addAction(openAction);
        toolbar->addAction(exportAction);
        toolbar->addAction(importAction);
        toolbar->addSeparator();
        toolbar->addAction(undoAction_);
        toolbar->addAction(redoAction_);
        toolbar->addSeparator();
        toolbar->addAction(refreshAction);
        toolbar->addAction(fitAction);
        toolbar->addAction(boardModeAction_);
        toolbar->addAction(openSelectedPreviewsAction);
        toolbar->addAction(closeSelectedPreviewsAction);
        toolbar->addAction(editorPaneAction_);
        toolbar->addAction(debugPaneAction_);
        toolbar->addSeparator();
        auto* editorPositionButton = new QToolButton(this);
        editorPositionButton->setText(QStringLiteral("配置"));
        editorPositionButton->setPopupMode(QToolButton::InstantPopup);
        auto* editorPositionToolMenu = new QMenu(editorPositionButton);
        editorPositionToolMenu->addAction(editorLeftAction);
        editorPositionToolMenu->addAction(editorRightAction);
        editorPositionToolMenu->addAction(editorBottomAction);
        editorPositionButton->setMenu(editorPositionToolMenu);
        toolbar->addWidget(editorPositionButton);
        auto* themeButton = new QToolButton(this);
        themeButton->setText(QStringLiteral("テーマ"));
        themeButton->setPopupMode(QToolButton::InstantPopup);
        auto* themeToolMenu = new QMenu(themeButton);
        themeToolMenu->addAction(lightThemeAction_);
        themeToolMenu->addAction(darkThemeAction_);
        themeButton->setMenu(themeToolMenu);
        toolbar->addWidget(themeButton);

        applyEditorPanePosition(QSettings().value(QStringLiteral("editor/panePosition"), QStringLiteral("right")).toString(),
                                false);
        for (QAction* action : editorPositionGroup->actions()) {
            if (action->data().toString() == editorPanePosition_) {
                action->setChecked(true);
                break;
            }
        }

        connect(openAction, &QAction::triggered, this, [this] {
            const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("フォルダを開く"), rootPath_);
            openRootFolder(dir);
        });
        connect(refreshAction, &QAction::triggered, this, [this] { refreshAll(); });
        connect(fitAction, &QAction::triggered, this, [this] { fitToMap(); });
        connect(exportAction, &QAction::triggered, this, [this] { exportArchive(); });
        connect(importAction, &QAction::triggered, this, [this] { importArchive(); });
        connect(openSelectedPreviewsAction, &QAction::triggered, this, [this] { setSelectedFilePreviews(true); });
        connect(closeSelectedPreviewsAction, &QAction::triggered, this, [this] { setSelectedFilePreviews(false); });
        connect(debugPaneAction_, &QAction::toggled, debugDock_, &QDockWidget::setVisible);
        connect(debugDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            if (debugPaneAction_ && debugPaneAction_->isChecked() != visible) {
                debugPaneAction_->setChecked(visible);
            }
            if (visible) {
                refreshDebugPane(QStringLiteral("debug pane opened"));
            }
        });
        connect(copyDebugButton, &QPushButton::clicked, this, [this] { copyDebugPaneToClipboard(); });
        connect(editorPositionGroup, &QActionGroup::triggered, this, [this](QAction* action) {
            applyEditorPanePosition(action->data().toString(), true);
        });
        connect(themeGroup, &QActionGroup::triggered, this, [this](QAction* action) {
            applyTheme(appThemeFromString(action->data().toString()), true, true);
        });
        connect(editorPaneAction_, &QAction::toggled, this, [this](bool visible) {
            if (!visible && !saveSideEditorNow()) {
                editorPaneAction_->setChecked(true);
                return;
            }
            sideEditorPanel_->setVisible(visible);
            QSettings settings;
            settings.setValue(QStringLiteral("editor/paneVisible"), visible);
            settings.sync();
            if (visible) {
                applyEditorPanePosition(editorPanePosition_, false);
                updateSideEditorForSelection();
            }
        });
        connect(renameSelectedAction, &QAction::triggered, this, [this] { renameSelectedItem(); });
        connect(maximizeAction, &QAction::triggered, this, [this] {
            if (isMaximized()) {
                showNormal();
            } else {
                showMaximized();
            }
        });
        connect(quitAction, &QAction::triggered, this, &QWidget::close);
        connect(&scene_, &QGraphicsScene::selectionChanged, this, [this] {
            if (!suppressSideEditorSelectionUpdate_) {
                updateSideEditorForSelection();
            }
            refreshDebugPane(QStringLiteral("selection changed"));
        });
        connect(view_->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] { scheduleViewStateSave(); });
        connect(view_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { scheduleViewStateSave(); });

        sideEditorSaveTimer_.setSingleShot(true);
        sideEditorSaveTimer_.setInterval(300);
        connect(&sideEditorSaveTimer_, &QTimer::timeout, this, [this] { saveSideEditorNow(); });
        auto* sideEditorSaveShortcut = new QShortcut(QKeySequence::Save, sideEditor_);
        sideEditorSaveShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sideEditorSaveShortcut, &QShortcut::activated, this, [this] { saveSideEditorFromShortcut(false); });
        auto* sideEditorReturnShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), sideEditor_);
        sideEditorReturnShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sideEditorReturnShortcut, &QShortcut::activated, this, [this] { saveSideEditorFromShortcut(true); });
        connect(sideEditor_, &QPlainTextEdit::textChanged, this, [this] {
            if (sideEditorLoading_ || sideEditorPath_.isEmpty() || sideEditor_->isReadOnly()) {
                return;
            }
            sideEditorDirty_ = true;
            sideEditorStatusLabel_->setText(QStringLiteral("保存待ち"));
            sideEditorSaveTimer_.start();
        });
        viewStateSaveTimer_.setSingleShot(true);
        viewStateSaveTimer_.setInterval(500);
        connect(&viewStateSaveTimer_, &QTimer::timeout, this, [this] { saveViewState(); });

        fileSystemWatcher_ = new QFileSystemWatcher(this);
        updateNativeWatcherModeForRoot();
        fileSystemRefreshTimer_.setSingleShot(true);
        fileSystemRefreshTimer_.setInterval(500);
        connect(fileSystemWatcher_, &QFileSystemWatcher::directoryChanged,
                this, [this](const QString& path) { scheduleFileSystemRefresh(path); });
        connect(fileSystemWatcher_, &QFileSystemWatcher::fileChanged,
                this, [this](const QString& path) { scheduleFileSystemRefresh(path); });
        connect(&fileSystemRefreshTimer_, &QTimer::timeout, this, [this] { refreshFromFileSystemChange(); });

        backgroundReconcileTimer_.setInterval(BackgroundReconcileIntervalMs);
        connect(&backgroundReconcileTimer_, &QTimer::timeout, this, [this] { runBackgroundReconcile(); });
        backgroundReconcileTimer_.start();

        previewClickTimer_.setSingleShot(true);
        connect(&previewClickTimer_, &QTimer::timeout, this, [this] {
            if (!queuedPreviewPath_.isEmpty()) {
                toggleInlinePreviewPath(queuedPreviewPath_);
                queuedPreviewPath_.clear();
                return;
            }
            if (!queuedCollapsePath_.isEmpty()) {
                toggleCollapsedPath(queuedCollapsePath_);
                queuedCollapsePath_.clear();
            }
        });

        applyTheme(uiTheme_, false, false);
        restoreWindowStateFromSettingsFile();
        // The initial scan is deferred so the window appears immediately: on network drives or
        // roots with many files, the metadata load + tree scan used to keep the constructor --
        // and therefore the first paint -- blocked for seconds to minutes.
        showStartupLoadingIndicator();
        QTimer::singleShot(DeferredStartupDelayMs, this, [this] { completeDeferredStartup(); });
        qApp->installEventFilter(this);
    }

MainWindow::~MainWindow()
{
        // Stop the startup scan thread before any member teardown: it may still be walking the
        // tree or posting its finished result back to this object. The cancel flag makes it
        // return promptly, so the wait does not stall window close on a slow network walk.
        if (startupScanCancelled_) {
            startupScanCancelled_->store(true);
        }
        if (startupScanThread_) {
            startupScanThread_->wait();
            delete startupScanThread_;
            startupScanThread_ = nullptr;
        }
        if (reconcileScanCancelled_) {
            reconcileScanCancelled_->store(true);
        }
        if (reconcileScanThread_) {
            reconcileScanThread_->wait();
            delete reconcileScanThread_;
            reconcileScanThread_ = nullptr;
        }
        if (fileWatcherResetCancelled_) {
            fileWatcherResetCancelled_->store(true);
        }
        if (fileWatcherResetThread_) {
            fileWatcherResetThread_->wait();
            delete fileWatcherResetThread_;
            fileWatcherResetThread_ = nullptr;
        }
        if (qApp) {
            qApp->removeEventFilter(this);
        }
        previewClickTimer_.stop();
        fileSystemRefreshTimer_.stop();
        sideEditorSaveTimer_.stop();
        viewStateSaveTimer_.stop();
        if (fileSystemWatcher_) {
            fileSystemWatcher_->removePaths(fileSystemWatcher_->files());
            fileSystemWatcher_->removePaths(fileSystemWatcher_->directories());
        }
        if (view_) {
            view_->setScene(nullptr);
        }
        nodeItemsByPath_.clear();
        connectionLayer_ = nullptr;
        scene_.clear();
        if (mycelStorageEnabled_) {
            cleanHistoryTrash();  // session over: undo history is gone, so its trash can go too
        }
    }

bool MainWindow::mycelStorageEnabled() const
{
        return mycelStorageEnabled_;
    }

Node* MainWindow::rootNode() const
{
        return root_.get();
    }

Node* MainWindow::nodeForPath(const QString& path) const
{
        return root_ ? findNodeByPath(*root_, path) : nullptr;
    }

void MainWindow::activateMainWindow()
{
        show();
        raise();
        activateWindow();
        focusBoard();
    }

void MainWindow::openRootFolder(const QString& dir)
{
        if (dir.isEmpty()) {
            return;
        }
        const QString normalized = normalizedDirectoryPath(dir);
        if (!QFileInfo(normalized).isDir()) {
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 QStringLiteral("フォルダが見つかりません:\n%1").arg(QDir::toNativeSeparators(dir)));
            return;
        }
        if (QDir::cleanPath(normalized) == QDir::cleanPath(rootPath_)) {
            return;  // already open
        }
        // Persist the outgoing root's canvas state now (parent⇄child navigation included), and
        // cancel any re-centering still pending from this root's own restore so it can neither
        // suppress this save nor recenter the next root's view.
        ++viewRestoreGeneration_;
        pendingViewRestoreReapplies_ = 0;
        saveViewState();  // board-aware: also persists the active pattern when in board mode
        saveCollapsedFile();
        // Board state belongs to a root: drop the outgoing root's board and adopt the new root's
        // saved mode/pattern after the tree data is loaded below.
        boardMode_ = false;
        if (boardModeAction_) {
            boardModeAction_->setChecked(false);
        }
        boardPatternName_.clear();
        boardCards_.clear();
        boardNodes_.clear();
        boardPatternView_ = QJsonObject();
        rootPath_ = normalized;
        rememberRootPath(rootPath_);
        updateNativeWatcherModeForRoot();
        collapsedPaths_.clear();
        previewPaths_.clear();
        previewSizes_.clear();
        previewImageScales_.clear();
        loadMetadataAndReconcileHashCache();
        if (!loadCollapsedFile()) {
            applyLargeTreeStartupCollapse();
        }
        // Keep the current window position/size when switching roots — do NOT apply the target
        // root's saved window geometry (that is only restored on initial startup).
        rebuild(true);
        // Adopt the new root's saved display mode (board/tree) and pattern.
        if (mycelStorageEnabled_) {
            if (const std::optional<QJsonObject> viewObject = loadViewStateObject()) {
                const QJsonObject board = viewObject->value(QStringLiteral("board")).toObject();
                boardPatternName_ = board.value(QStringLiteral("pattern")).toString();
                if (board.value(QStringLiteral("active")).toBool(false)) {
                    setBoardMode(true, false);
                }
            }
        }
        updateBoardPatternMenu();
        QTimer::singleShot(0, this, [this] { syncEditorPaneVisibility(); });
    }

void MainWindow::makeFolderChildRoot(const QString& folderPath)
{
        if (!mycelStorageEnabled_ || folderPath.isEmpty()) {
            return;
        }
        const QFileInfo info(folderPath);
        if (!info.isDir()) {
            return;
        }
        if (QDir::cleanPath(folderPath) == QDir::cleanPath(rootPath_)) {
            return;  // the opened root is already a root
        }
        if (directoryHasMycel(folderPath)) {
            return;  // already a root
        }
        if (!QDir(folderPath).mkpath(QStringLiteral(".mycel"))) {
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 QStringLiteral(".mycel フォルダを作成できませんでした。"));
            return;
        }
        writeParentRootRecord(folderPath, rootPath_);  // record this root as the new child's parent
        recordDebugEvent(QStringLiteral("make child root: %1").arg(relativeKeyForPath(folderPath)));
        rebuild(false);  // the folder is now a sub-root; keep the current view
    }

void MainWindow::integrateChildRootIntoParent(const QString& childDir)
{
        if (!mycelStorageEnabled_ || childDir.isEmpty() || !directoryHasMycel(childDir)) {
            return;
        }
        if (QDir::cleanPath(childDir) == QDir::cleanPath(rootPath_)) {
            return;  // cannot integrate the opened root itself
        }
        const auto reply = QMessageBox::question(
            this, QStringLiteral("Mycel"),
            QStringLiteral("「%1」を親に統合します。\n子の .mycel を解除し、設定を親に取り込みます。よろしいですか？")
                .arg(QFileInfo(childDir).fileName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }

        // Flush the parent's current in-memory state to disk before merging JSON files.
        saveOrderFile();
        saveColorFile();
        savePreviewFile();
        saveCollapsedFile();
        saveLinkFile();

        const QString cd = childDir;
        const std::function<QString(const QString&)> reKey = [this, cd](const QString& childKey) {
            const QString absolute = childKey.isEmpty() ? cd : QDir(cd).absoluteFilePath(childKey);
            return QDir::cleanPath(QDir(rootPath_).relativeFilePath(absolute));
        };
        QDir child(childDir);
        mergeChildObjectSection(colorFilePath(), child.filePath(QStringLiteral(".mycel/colors.json")),
                                QStringLiteral("colors"), reKey);
        mergeChildObjectSection(orderFilePath(), child.filePath(QStringLiteral(".mycel/order.json")),
                                QStringLiteral("directories"), reKey);
        mergeChildObjectSection(previewFilePath(), child.filePath(QStringLiteral(".mycel/previews.json")),
                                QStringLiteral("previews"), reKey);
        mergeChildStringArray(collapsedFilePath(), child.filePath(QStringLiteral(".mycel/collapsed.json")),
                              QStringLiteral("folders"), reKey);
        mergeChildLinks(linkFilePath(), child.filePath(QStringLiteral(".mycel/links.json")), reKey);

        // Remove the child's .mycel so the parent treats it as a plain folder again.
        QDir(child.filePath(QStringLiteral(".mycel"))).removeRecursively();

        // Reload the merged parent metadata and re-render (keep the current view).
        loadOrderFile();
        loadColorFile();
        loadPreviewFile();
        loadLinkFile();
        loadCollapsedFile();
        recordDebugEvent(QStringLiteral("integrate child root: %1").arg(relativeKeyForPath(childDir)));
        rebuild(false);
    }

void MainWindow::switchIntoSubRoot(const QString& childPath)
{
        if (childPath.isEmpty()) {
            return;
        }
        if (mycelStorageEnabled_) {
            writeParentRootRecord(childPath, rootPath_);
        }
        openRootFolder(childPath);
    }

void MainWindow::writeParentRootRecord(const QString& childDir, const QString& parentDir)
{
        QDir child(childDir);
        if (!child.mkpath(QStringLiteral(".mycel"))) {
            return;
        }
        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("parentRoot"), child.relativeFilePath(parentDir));
        writeJsonFileAtomic(child.filePath(QStringLiteral(".mycel/parent.json")), rootObject);
    }

QString MainWindow::recordedParentRoot(const QString& rootDir) const
{
        QFile file(QDir(rootDir).filePath(QStringLiteral(".mycel/parent.json")));
        if (!file.open(QIODevice::ReadOnly)) {
            return QString();
        }
        const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
        const QString relative = object.value(QStringLiteral("parentRoot")).toString();
        if (relative.isEmpty()) {
            return QString();
        }
        const QString parent = QDir::cleanPath(QDir(rootDir).absoluteFilePath(relative));
        // The recorded parent is the root the user actually navigated down from, so trust it as long
        // as the directory still exists — do not require it to carry its own .mycel.
        return QFileInfo(parent).isDir() ? parent : QString();
    }

QStringList MainWindow::parentRootChain() const
{
        QStringList chain;
        if (!mycelStorageEnabled_) {
            return chain;
        }
        QSet<QString> seen;
        QString current = QDir::cleanPath(rootPath_);
        for (int i = 0; i < 64; ++i) {
            const QString parent = recordedParentRoot(current);
            if (parent.isEmpty() || seen.contains(parent)) {
                break;
            }
            seen.insert(parent);
            chain.prepend(parent);
            current = parent;
        }
        if (chain.isEmpty()) {
            QDir dir(rootPath_);
            while (dir.cdUp()) {
                if (directoryHasMycel(dir.absolutePath())) {
                    chain.prepend(dir.absolutePath());
                }
            }
        }
        return chain;
    }

void MainWindow::showCheatSheet()
{
        QMessageBox::information(
            this, QStringLiteral("Mycel"),
            QStringLiteral(
                "キー操作\n"
                "? : このチートシートを表示\n"
                "F2 : 単一選択したファイル/フォルダの名前を変更\n"
                "F5 : 全体をリロード\n"
                "F11 : ウィンドウを最大化/通常表示\n"
                "Ctrl + 0 : 全体表示\n"
                "+ / - : ズームイン/ズームアウト\n"
                "Ctrl + E : プレビューペインの表示/非表示\n"
                "E : 選択ファイルをプレーンテキストで編集\n"
                "O : 選択項目を OS の既定アプリで開く\n"
                "Shift + O : 選択ファイルを別のアプリで開く…\n"
                "テキスト編集画面内 Ctrl + S : 保存\n"
                "テキスト編集画面内 Ctrl + Enter : 保存して閉じる\n"
                "テキスト編集画面内 Ctrl + W : 閉じる\n"
                "テキスト編集画面内 Esc : 未保存時に確認して閉じる\n"
                "Tab : 同じ階層の次の表示項目へ移動\n"
                "Shift + Tab : 同じ階層の前の表示項目へ移動\n"
                "↑ / ↓ : 同じフォルダ内の前/次の項目へ移動\n"
                "← : 上位フォルダへ移動\n"
                "→ : フォルダ内の最初の項目へ移動\n"
                "Shift + ↑ / ↓ : 表示順で選択範囲を拡大/縮小\n"
                "Ctrl + ↑ / ↓ : 兄弟内で並び順を変更（横リンクはリンク内で移動）\n"
                "Ctrl + ← : 上位フォルダへ移動（横リンクは上位レベルへ接続変更）\n"
                "Enter : 選択したファイル/フォルダの名前を変更\n"
                "Space : 選択ファイルのプレビュー表示/非表示、選択フォルダの折りたたみ/展開\n"
                "N : 選択フォルダ内/選択ファイルと同階層に NewFile を作成\n"
                "Shift + N : 選択フォルダ内/選択ファイルと同階層に NewFolder を作成\n"
                "D : 選択したファイル/フォルダの削除確認を表示\n"
                "Shift + D : 選択項目の関連（横リンク）を解除\n"
                "1 〜 6 : 選択項目に色を割り当て\n"
                "0 : 選択項目の色をクリア\n"
                "Ctrl + C / Ctrl + V : 選択項目を同じ親フォルダへコピー\n"
                "Ctrl + Q : アプリを終了\n"
                "範囲選択後 Enter : 選択範囲へズーム\n\n"
                "クリック操作\n"
                "ノードを左クリック : 選択\n"
                "フォルダをダブルクリック : 折りたたみ/展開\n"
                "ファイルをダブルクリック : プレビューを開く/閉じる\n"
                "テキストファイルを右クリックして編集 : 内蔵エディタで開く\n"
                "右クリック : コンテキストメニュー\n\n"
                "移動/ズーム\n"
                "Alt + 左ドラッグ、中ボタンドラッグ、または空白で右ドラッグ : キャンバス移動\n"
                "マウスホイール : ズーム\n"
                "タッチパッドのピンチ : ズーム"));
    }

void MainWindow::loadMetadataAndReconcileHashCache()
{
        loadOrderFile();
        loadColorFile();
        loadPreviewFile();
        loadLinkFile();
        loadHashCacheFile();
        if (!mycelStorageEnabled_) {
            return;  // no metadata to reconcile or hash; skip the whole-tree walk entirely
        }

        // One whole-tree walk feeds both passes below (they used to walk separately).
        const TreeWalkResult walk = walkRealTree(rootPath_);

        // Whole-tree scope: safe to prune metadata for files with no match anywhere in the root.
        QSet<QString> currentPaths;
        for (const QString& path : walk.files) {
            currentPaths.insert(path);
        }
        if (reconcileRenamedFiles(walk.directories, currentPaths, /*pruneUnmatchedMetadata=*/true,
                                  nullptr)) {
            persistRenameReconciliationResults();
        }

        // Seed/refresh hashes for every real file under the root (not just what the collapsed,
        // depth- and child-limited display tree happens to show), so files inside collapsed or
        // deeply nested folders are still recognized if renamed externally later.
        bool hashCacheChanged = false;
        for (const QFileInfo& info : walk.fileInfos) {
            hashCacheChanged = updateHashCacheEntryForFile(info) || hashCacheChanged;
        }
        if (hashCacheChanged) {
            saveHashCacheFile();
        }
    }

void MainWindow::showStartupLoadingIndicator()
{
        QGraphicsSimpleTextItem* text = scene_.addSimpleText(QStringLiteral("読み込み中…"));
        QFont font = text->font();
        font.setPointSize(15);
        text->setFont(font);
        text->setBrush(palette().color(QPalette::WindowText));
        text->setPos(140.0, 120.0);
    }

void MainWindow::completeDeferredStartup()
{
        loadOrderFile();
        loadColorFile();
        loadPreviewFile();
        loadLinkFile();
        loadHashCacheFile();
        if (!loadCollapsedFile()) {
            applyLargeTreeStartupCollapse();
        }
        if (mycelStorageEnabled_) {
            cleanHistoryTrash();  // discard any trash left behind by a previous session
        }
        rebuild(true);
        // Restore the last display mode (board/tree) and active pattern for this root.
        if (mycelStorageEnabled_) {
            if (const std::optional<QJsonObject> viewObject = loadViewStateObject()) {
                const QJsonObject board = viewObject->value(QStringLiteral("board")).toObject();
                boardPatternName_ = board.value(QStringLiteral("pattern")).toString();
                if (board.value(QStringLiteral("active")).toBool(false)) {
                    setBoardMode(true, false);
                }
            }
        }
        QTimer::singleShot(0, this, [this] { syncEditorPaneVisibility(); });
        startStartupScanThread();
    }

void MainWindow::startStartupScanThread()
{
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        startupScanCancelled_ = cancelled;
        startupScanThread_ = QThread::create(
            [this, rootPath = rootPath_, storageEnabled = mycelStorageEnabled_,
             knownHashes = fileHashCache_, cancelled] {
                StartupScanResult result = runStartupScan(rootPath, storageEnabled, knownHashes, *cancelled);
                if (cancelled->load()) {
                    return;  // window is closing; the destructor waits for this thread
                }
                QMetaObject::invokeMethod(
                    this, [this, rootPath, result = std::move(result)] { finishStartupScan(rootPath, result); },
                    Qt::QueuedConnection);
            });
        startupScanThread_->start();
    }

void MainWindow::finishStartupScan(const QString& scannedRootPath, const StartupScanResult& result)
{
        startupScanPending_ = false;
        if (QDir::cleanPath(scannedRootPath) != QDir::cleanPath(rootPath_)) {
            // openRootFolder() does not cancel an in-flight scan before switching roots. Applying
            // this result -- rename reconcile, hash-cache entries, watcher paths -- to the now-
            // different root would mix one root's file identities into another's metadata, so
            // discard it. rebuild(true) inside openRootFolder() already ran resetFileSystemWatcher()
            // for the new root, but that call saw startupScanPending_ still true and skipped
            // registering watches -- do it now that this (discarded) scan is no longer blocking it.
            recordDebugEvent(QStringLiteral("startup scan discarded: root changed mid-scan (%1 -> %2)")
                                 .arg(scannedRootPath, rootPath_));
            resetFileSystemWatcher();
            return;
        }

        bool renameReconciled = false;
        if (mycelStorageEnabled_ && !result.directories.isEmpty()) {
            // Must run before merging refreshedHashes: an "appeared" file is one on disk but
            // absent from the hash cache, so merging first would hide every rename candidate.
            QSet<QString> currentPaths;
            for (const QString& path : result.files) {
                currentPaths.insert(path);
            }
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
            // Skip entries something else (refreshAll, a watcher refresh) already brought to
            // the same or a newer state than the walk saw.
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
            // The display tree still holds the old name/path until rescanned. The watcher
            // resets inside this rebuild are still suppressed by startupScanPending_.
            rebuild(false);
        }

        recordDebugEvent(QStringLiteral("startup scan finished: dirs=%1 files=%2 hashed=%3 reconciled=%4")
                             .arg(result.directories.size())
                             .arg(result.files.size())
                             .arg(result.refreshedHashes.size())
                             .arg(renameReconciled ? QStringLiteral("true") : QStringLiteral("false")));
        if (fileSystemWatcher_ && !inlineRenameActivitySuspended_) {
            applyFileSystemWatcherPaths(result.directories, result.files);
        }
        // If a rename was in progress, the resume path calls resetFileSystemWatcher() itself.
    }

void MainWindow::applyLargeTreeStartupCollapse()
{
        if (!isLargeRootTree()) {
            return;
        }

        QDir root(rootPath_);
        const QFileInfoList entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                                                         QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& entry : entries) {
            if (entry.fileName() == QStringLiteral(".mycel")) {
                continue;
            }
            collapsedPaths_.insert(entry.absoluteFilePath());
        }
        if (!collapsedPaths_.isEmpty()) {
            saveCollapsedFile();
        }
    }

bool MainWindow::isLargeRootTree() const
{
        int fileCount = 0;
        QDirIterator iterator(rootPath_,
                              QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString path = QDir::cleanPath(iterator.next());
            if (path.contains(QStringLiteral("/.mycel/"))) {
                continue;
            }
            ++fileCount;
            if (fileCount > LargeTreeAutoCollapseFileThreshold) {
                return true;
            }
        }
        return false;
    }

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
        if (renameEdit_ && event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                QTimer::singleShot(0, this, [this] { finishInlineRename(false); });
                return true;
            }
            if (watched != renameEdit_ &&
                (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)) {
                QTimer::singleShot(0, this, [this] { finishInlineRename(true); });
                return true;
            }
            if (watched != renameEdit_) {
                renameEdit_->setFocus(Qt::OtherFocusReason);
                QKeyEvent forwarded(keyEvent->type(),
                                    keyEvent->key(),
                                    keyEvent->modifiers(),
                                    keyEvent->text(),
                                    keyEvent->isAutoRepeat(),
                                    keyEvent->count());
                QApplication::sendEvent(renameEdit_, &forwarded);
                return true;
            }
        }
        if (event->type() == QEvent::KeyPress && isInlinePreviewObject(watched)) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            recordDebugEvent(QStringLiteral("inline preview key: %1").arg(debugKeyName(keyEvent)));
            if (keyEvent->modifiers() == Qt::NoModifier &&
                (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down ||
                 keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right) &&
                singleSelectedNode() && handleBoardShortcut(keyEvent)) {
                recordDebugEvent(QStringLiteral("inline preview key handled by board: %1").arg(debugKeyName(keyEvent)));
                return true;
            }
        }
        if (event->type() == QEvent::KeyPress && watched == view_) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            recordDebugEvent(QStringLiteral("board key: %1").arg(debugKeyName(keyEvent)));
            Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
            modifiers &= ~Qt::KeypadModifier;
            const bool boardShortcut =
                (modifiers == Qt::NoModifier &&
                 (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down ||
                  keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right ||
                  keyEvent->key() == Qt::Key_N || keyEvent->key() == Qt::Key_O ||
                  keyEvent->key() == Qt::Key_D || keyEvent->key() == Qt::Key_Return ||
                  keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Space ||
                  (keyEvent->key() >= Qt::Key_0 && keyEvent->key() <= Qt::Key_6) ||
                  isDeleteShortcut(keyEvent->key(), modifiers))) ||
                (modifiers == Qt::ShiftModifier &&
                 (keyEvent->key() == Qt::Key_N || keyEvent->key() == Qt::Key_Tab ||
                  keyEvent->key() == Qt::Key_D || keyEvent->key() == Qt::Key_O ||
                  keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down)) ||
                (modifiers == Qt::ControlModifier &&
                 (keyEvent->key() == Qt::Key_C || keyEvent->key() == Qt::Key_V ||
                  keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down ||
                  keyEvent->key() == Qt::Key_Left)) ||
                isDeleteShortcut(keyEvent->key(), modifiers) ||
                isSelectAllShortcut(keyEvent->key(), modifiers);
            if (!textInputWidgetHasFocus() && boardShortcut) {
                if (handleBoardShortcut(keyEvent)) {
                    recordDebugEvent(QStringLiteral("board key handled by main filter: %1").arg(debugKeyName(keyEvent)));
                    return true;
                }
                recordDebugEvent(QStringLiteral("board key not handled by main filter: %1").arg(debugKeyName(keyEvent)));
            }
        }
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                if (const QString path = inlinePreviewPathForObject(watched); !path.isEmpty()) {
                    selectNodePath(path);
                }
                if (isSidePreviewObject(watched) && !sideEditorPath_.isEmpty()) {
                    return focusEditorForPath(sideEditorPath_);
                }
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }

void MainWindow::closeEvent(QCloseEvent* event)
{
        if (!saveSideEditorNow()) {
            event->ignore();
            return;
        }
        saveViewState();
        saveCollapsedFile();
        QMainWindow::closeEvent(event);
    }

void MainWindow::scheduleRestoreViewStateOrFit()
{
        QTimer::singleShot(0, this, [this] { restoreViewStateOrFit(); });
    }

void MainWindow::restoreViewStateOrFit()
{
        if (boardMode_) {
            return;  // queued tree-view restore must not run after switching to the board
        }
        QTransform transform;
        int horizontal = 0;
        int vertical = 0;
        QPointF center;
        bool hasCenter = false;
        if (!loadViewState(transform, horizontal, vertical, center, hasCenter)) {
            fitToMap();
            return;
        }

        restoringViewState_ = true;
        view_->setTransform(transform);
        if (hasCenter) {
            view_->centerOn(center);
        } else {
            // Legacy view.json without a scene-space centre: raw scrollbar values.
            view_->horizontalScrollBar()->setValue(horizontal);
            view_->verticalScrollBar()->setValue(vertical);
        }
        restoringViewState_ = false;

        // The window may still be settling (show/maximize resizes the viewport after this runs,
        // shifting the visible area). Re-center once the geometry has settled, and keep the
        // view-state saves suppressed until then so a drifted position is never written back.
        // The generation guard voids these re-applies if the root switches before they fire.
        if (hasCenter) {
            pendingViewRestoreCenter_ = center;
            pendingViewRestoreReapplies_ = 2;
            const int generation = ++viewRestoreGeneration_;
            QTimer::singleShot(150, this, [this, generation] { reapplyRestoredViewCenter(generation); });
            QTimer::singleShot(450, this, [this, generation] { reapplyRestoredViewCenter(generation); });
        }
    }

void MainWindow::reapplyRestoredViewCenter(int generation)
{
        if (generation != viewRestoreGeneration_ || pendingViewRestoreReapplies_ <= 0 || !view_ || boardMode_) {
            return;  // a newer restore, a root switch, or a mode switch superseded this re-apply
        }
        --pendingViewRestoreReapplies_;
        restoringViewState_ = true;
        view_->centerOn(pendingViewRestoreCenter_);
        restoringViewState_ = false;
    }

void MainWindow::fitToMap()
{
        if (scene_.items().isEmpty()) {
            return;
        }
        QRectF nodeBounds;
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem) {
                continue;
            }
            nodeBounds = nodeBounds.isNull() ? nodeItem->sceneBoundingRect()
                                             : nodeBounds.united(nodeItem->sceneBoundingRect());
        }
        if (nodeBounds.isNull()) {
            return;
        }
        view_->resetTransform();
        view_->fitInView(nodeBounds.adjusted(-160.0, -160.0, 260.0, 160.0), Qt::KeepAspectRatio);
        view_->scale(0.96, 0.96);
        scheduleViewStateSave();
    }

void MainWindow::applyEditorPanePosition(QString position, bool persist)
{
        if (!editorSplitter_ || !view_ || !sideEditorPanel_) {
            return;
        }
        if (position != QStringLiteral("left") &&
            position != QStringLiteral("right") &&
            position != QStringLiteral("bottom")) {
            position = QStringLiteral("right");
        }

        const bool editorVisible = sideEditorPanel_->isVisible();
        editorPanePosition_ = position;

        if (position == QStringLiteral("bottom")) {
            editorSplitter_->setOrientation(Qt::Vertical);
            editorSplitter_->insertWidget(0, view_);
            editorSplitter_->insertWidget(1, sideEditorPanel_);
            editorSplitter_->setStretchFactor(0, 1);
            editorSplitter_->setStretchFactor(1, 0);
            editorSplitter_->setSizes({620, 220});
        } else if (position == QStringLiteral("left")) {
            editorSplitter_->setOrientation(Qt::Horizontal);
            editorSplitter_->insertWidget(0, sideEditorPanel_);
            editorSplitter_->insertWidget(1, view_);
            editorSplitter_->setStretchFactor(0, 0);
            editorSplitter_->setStretchFactor(1, 1);
            editorSplitter_->setSizes({360, 920});
        } else {
            editorSplitter_->setOrientation(Qt::Horizontal);
            editorSplitter_->insertWidget(0, view_);
            editorSplitter_->insertWidget(1, sideEditorPanel_);
            editorSplitter_->setStretchFactor(0, 1);
            editorSplitter_->setStretchFactor(1, 0);
            editorSplitter_->setSizes({920, 360});
        }

        sideEditorPanel_->setVisible(editorVisible);
        if (persist) {
            QSettings settings;
            settings.setValue(QStringLiteral("editor/panePosition"), editorPanePosition_);
            settings.sync();
        }
    }

