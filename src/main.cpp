#include <QtCore/QDir>
#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QEvent>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QDirIterator>
#include <QtCore/QStorageInfo>
#include <QtCore/QHash>
#include <QtCore/QIODevice>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QCryptographicHash>
#include <QtCore/QMimeData>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>
#include <QtCore/QXmlStreamReader>
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtCore/QRectF>
#include <QtCore/QRegularExpression>
#include <QtCore/QBuffer>
#include <QtCore/QMutex>
#include <QtCore/QSaveFile>
#include <QtCore/QSettings>
#include <QtCore/QThread>
#include <QtCore/QSet>
#include <QtCore/QSizeF>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QVariant>
#include <QtCore/QVector>
#include <QtCore/qtenvironmentvariables.h>
#include <QtGui/QTransform>
#include <QtGui/QBrush>
#include <QtGui/QActionGroup>
#include <QtGui/QColor>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QDesktopServices>
#include <QtGui/QFont>
#include <QtGui/QFontMetricsF>
#include <QtGui/QIcon>
#include <QtGui/QClipboard>
#include <QtGui/QImage>
#include <QtGui/QImageReader>
#include <QtGui/QKeyEvent>
#include <QtGui/QKeySequence>
#include <QtGui/QMouseEvent>
#include <QtGui/QNativeGestureEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPalette>
#include <QtGui/QPen>
#include <QtGui/QPixmap>
#include <QtGui/QPixmapCache>
#include <QtGui/QShortcut>
#include <QtGui/QTextCursor>
#include <QtGui/QTextDocument>
#include <QtGui/QTextOption>
#include <QtGui/QWheelEvent>
#include <QtGui/QAction>
#include <QtGui/QCloseEvent>
#include <QtGui/QScreen>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimediaWidgets/QVideoWidget>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#if MYCEL_HAS_WEBENGINE
#include <QtWebEngineCore/QWebEngineSettings>
#include <QtWebEngineWidgets/QWebEngineView>
#endif
#if MYCEL_HAS_PDF
#include <QtPdf/QPdfDocument>
#include <QtPdf/QPdfDocumentRenderOptions>
#endif
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsPathItem>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsSceneContextMenuEvent>
#include <QtWidgets/QGraphicsSceneDragDropEvent>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

#ifndef MYCEL_VERSION
#define MYCEL_VERSION "0.0.0"
#endif

#include "mycel_fileops.hpp"

namespace {

using mycel::FileOperationService;
using mycel::isDescendantPath;


#include "tree_model.h"

#include "node_item.h"
#include "connection_layer_item.h"
#include "mind_map_scene.h"
#include "drag_controller.h"
#include "board_view.h"
#include "text_font_settings.h"
#include "preview_widgets.h"
#include "text_editor.h"
#include "drop_target_resolver.h"
#include "selection_controller.h"
#include "parent_root_item.h"

// A curved connector between two points, matching the parent-child edge style.
QPainterPath edgePathBetweenPoints(const QPointF& start, const QPointF& end)
{
    const qreal distance = std::max<qreal>(96.0, end.x() - start.x());
    const qreal splitX = start.x() + std::clamp(distance * 0.46, 58.0, 128.0);
    QPainterPath path(start);
    path.cubicTo(QPointF(start.x() + distance * 0.16, start.y()),
                 QPointF(splitX - 34.0, end.y()), QPointF(splitX, end.y()));
    path.cubicTo(QPointF(splitX + 30.0, end.y()),
                 QPointF(end.x() - 38.0, end.y()), end);
    return path;
}

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QString rootPath, bool mycelStorageEnabled, QWidget* parent = nullptr)
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

    ~MainWindow() override
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

    bool mycelStorageEnabled() const
    {
        return mycelStorageEnabled_;
    }

    Node* rootNode() const
    {
        return root_.get();
    }

    Node* nodeForPath(const QString& path) const
    {
        return root_ ? findNodeByPath(*root_, path) : nullptr;
    }

    // Create an empty NewFile.txt (conflict-free name) in dirPath, register it, rebuild, record
    // history, and move focus to the new file. Returns the new path (empty on failure).
    QString createFileInDirectory(const QString& dirPath, const QString& afterSiblingPath = QString())
    {
        const QFileInfo dirInfo(dirPath);
        if (!dirInfo.exists() || !dirInfo.isDir()) {
            recordDebugEvent(QStringLiteral("create file: not a directory: %1").arg(QDir::toNativeSeparators(dirPath)));
            return QString();
        }
        QDir dir(dirInfo.absoluteFilePath());
        QString name = QStringLiteral("NewFile.txt");
        QString path = dir.filePath(name);
        for (int number = 2; QFileInfo::exists(path); ++number) {
            name = QStringLiteral("NewFile %1.txt").arg(number);
            path = dir.filePath(name);
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            recordDebugEvent(QStringLiteral("create file failed: %1").arg(QDir::toNativeSeparators(path)));
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("ファイルを作成できませんでした。"));
            return QString();
        }
        file.close();
        recordDebugEvent(QStringLiteral("created file: %1").arg(relativeKeyForPath(path)));
        appendCreatedItemToOrder(dir.absolutePath(), name, false,
                                 afterSiblingPath.isEmpty() ? QString() : QFileInfo(afterSiblingPath).fileName());
        saveOrderFile();
        rebuild(false);
        selectNodePath(path, true);  // focus the newly created file
        const QString trashPath = allocateTrashPath(name);
        recordHistory(QStringLiteral("ファイル作成"), {{trashPath, path}}, {{path, trashPath}},
                      historyBefore, historySelection);
        return path;
    }

    // Create a NewFolder (conflict-free) in dirPath, register it, rebuild, record history, and
    // move focus to the new folder. Returns the new path (empty on failure).
    QString createFolderInDirectory(const QString& dirPath, const QString& afterSiblingPath = QString())
    {
        const QFileInfo dirInfo(dirPath);
        if (!dirInfo.exists() || !dirInfo.isDir()) {
            recordDebugEvent(QStringLiteral("create folder: not a directory: %1").arg(QDir::toNativeSeparators(dirPath)));
            return QString();
        }
        QDir dir(dirInfo.absoluteFilePath());
        QString name = QStringLiteral("NewFolder");
        QString path = dir.filePath(name);
        for (int number = 2; QFileInfo::exists(path); ++number) {
            name = QStringLiteral("NewFolder %1").arg(number);
            path = dir.filePath(name);
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        if (!dir.mkdir(name)) {
            recordDebugEvent(QStringLiteral("create folder failed: %1").arg(QDir::toNativeSeparators(path)));
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("フォルダを作成できませんでした。"));
            return QString();
        }
        recordDebugEvent(QStringLiteral("created folder: %1").arg(relativeKeyForPath(path)));
        appendCreatedItemToOrder(dir.absolutePath(), name, true,
                                 afterSiblingPath.isEmpty() ? QString() : QFileInfo(afterSiblingPath).fileName());
        saveOrderFile();
        rebuild(false);
        selectNodePath(path, true);  // focus the newly created folder
        const QString trashPath = allocateTrashPath(name);
        recordHistory(QStringLiteral("フォルダ作成"), {{trashPath, path}}, {{path, trashPath}},
                      historyBefore, historySelection);
        return path;
    }

    // Create a new file next to `filePath` and link it horizontally to the right of it, recorded
    // as a single undoable action.
    void createLinkedFileBeside(const QString& filePath)
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

    // Thin wrappers kept for existing callers (keyboard shortcuts, selection-based creation).
    void createFolder(Node* parent)
    {
        if (parent && parent->isDir) {
            createFolderInDirectory(parent->path);
        }
    }

    void createFile(Node* parent, const QString& = {})
    {
        if (parent && parent->isDir) {
            createFileInDirectory(parent->path);
        }
    }

    void createFileInFolderPath(const QString& folderPath)
    {
        createFileInDirectory(folderPath);
    }

    void createFolderInFolderPath(const QString& folderPath)
    {
        createFolderInDirectory(folderPath);
    }

    void createFileInSelectedFolder()
    {
        Node* node = singleSelectedNode();
        if (!node) {
            recordDebugEvent(QStringLiteral("create file: no selected node"));
            return;
        }
        if (node->isDir) {
            recordDebugEvent(QStringLiteral("create file in selected folder: %1").arg(relativeKeyForPath(node->path)));
            createFile(node);
            return;
        }

        Node* parent = findVisibleNodeByPath(root_.get(), node->parentPath);
        if (!parent || !parent->isDir) {
            recordDebugEvent(QStringLiteral("create file: selected file has no visible parent"));
            return;
        }
        recordDebugEvent(QStringLiteral("create file beside selected file: %1").arg(relativeKeyForPath(parent->path)));
        createFileInDirectory(parent->path, node->path);  // place directly below the selected file
    }

    void createFolderInSelectedFolder()
    {
        Node* node = singleSelectedNode();
        if (!node || !node->isDir) {
            recordDebugEvent(QStringLiteral("create folder: selected node is not a folder"));
            return;
        }
        recordDebugEvent(QStringLiteral("create folder in selected folder: %1").arg(relativeKeyForPath(node->path)));
        createFolder(node);
    }

    bool copySelectedItems()
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

    bool copyNode(Node* node)
    {
        if (!node || node == root_.get()) {
            return false;
        }
        copiedPaths_ = {node->path};
        return true;
    }

    bool copyPath(const QString& path)
    {
        Node* node = nodeForPath(path);
        if (!node || node == root_.get()) {
            return false;
        }
        copiedPaths_ = {node->path};
        return true;
    }

    bool pasteCopiedItems()
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

    bool focusEditorForSelectedFile()
    {
        Node* node = singleSelectedNode();
        if (!node) {
            return false;
        }
        return focusEditorForNode(node);
    }

    bool focusEditorForNode(Node* node)
    {
        if (!canEditTextFile(node)) {
            return false;
        }
        return focusEditorForPath(node->path);
    }

    bool focusEditorForPath(const QString& path)
    {
        if (path.isEmpty()) {
            return false;
        }
        const QFileInfo info(path);
        if (!info.exists() || !info.isFile() || !isTextPreviewFile(info) || info.size() > 4 * 1024 * 1024) {
            return false;
        }

        return openCenteredTextEditor(path);
    }

    void focusBoard()
    {
        if (view_) {
            view_->setFocus(Qt::MouseFocusReason);
        }
    }

    void activateMainWindow()
    {
        show();
        raise();
        activateWindow();
        focusBoard();
    }

    void saveSideEditorFromShortcut(bool returnFocusToFile)
    {
        const QString path = sideEditorPath_;
        const QString subjectName = subjectFileNameFromEditor();
        const QString textAfterSubjectRemoval = subjectName.isEmpty()
                                                    ? QString()
                                                    : editorTextWithoutSubjectLine(sideEditor_->toPlainText());
        if (!saveSideEditorNow()) {
            return;
        }
        QString focusPath = path;
        if (!path.isEmpty() && !subjectName.isEmpty() && subjectName != QFileInfo(path).fileName()) {
            const QString destination = QFileInfo(path).dir().filePath(subjectName);
            if (!renamePathTo(path, subjectName)) {
                sideEditor_->setFocus(Qt::ShortcutFocusReason);
                return;
            }
            focusPath = destination;
            sideEditorPath_ = destination;
            sideEditorPathLabel_->setText(QDir::toNativeSeparators(relativeKeyForPath(destination)));
        }
        if (!focusPath.isEmpty() && !subjectName.isEmpty()) {
            sideEditorLoading_ = true;
            sideEditor_->setReadOnly(false);
            sideEditor_->setPlainText(textAfterSubjectRemoval);
            sideEditorLoading_ = false;
            sideEditorDirty_ = true;
            if (!saveSideEditorNow()) {
                sideEditor_->setFocus(Qt::ShortcutFocusReason);
                return;
            }
        }
        if (!returnFocusToFile) {
            sideEditor_->setFocus(Qt::ShortcutFocusReason);
            return;
        }
        if (!focusPath.isEmpty()) {
            sideEditorEditing_ = false;
            resetFileSystemWatcher();
            loadSidePreviewFile(focusPath);
            selectNodePath(focusPath);
            focusBoard();
            return;
        }
        view_->setFocus(Qt::ShortcutFocusReason);
    }

    QString subjectFileNameFromEditor() const
    {
        QString firstLine = sideEditor_->toPlainText().section(QLatin1Char('\n'), 0, 0).trimmed();
        if (firstLine.endsWith(QLatin1Char('\r'))) {
            firstLine.chop(1);
        }
        const QString prefix = QStringLiteral("Subject:");
        if (!firstLine.startsWith(prefix)) {
            return {};
        }
        return firstLine.mid(prefix.size()).trimmed();
    }

    QString editorTextWithoutSubjectLine(const QString& text) const
    {
        if (!text.startsWith(QStringLiteral("Subject:"))) {
            return text;
        }

        const int newline = text.indexOf(QLatin1Char('\n'));
        if (newline < 0) {
            return {};
        }
        return text.mid(newline + 1);
    }

    bool toggleSelectedFilePreviews()
    {
        const QStringList paths = selectedFilePaths();
        if (paths.isEmpty()) {
            return false;
        }
        toggleFilePreviewsForPaths(paths);
        view_->setFocus(Qt::ShortcutFocusReason);
        return true;
    }

    bool toggleSelectedFoldersCollapsed()
    {
        bool hasSelectedFolder = false;
        bool allFoldersCollapsed = true;
        QStringList selectedPaths;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem || !nodeItem->node()) {
                continue;
            }

            selectedPaths.append(nodeItem->node()->path);
            if (!nodeItem->node()->isDir) {
                continue;
            }

            hasSelectedFolder = true;
            if (!collapsedPaths_.contains(nodeItem->node()->path)) {
                allFoldersCollapsed = false;
            }
        }

        if (!hasSelectedFolder) {
            return false;
        }

        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        const bool collapse = !allFoldersCollapsed;
        bool changed = false;
        for (const QString& path : selectedPaths) {
            Node* node = findVisibleNodeByPath(root_.get(), path);
            if (!node || !node->isDir) {
                continue;
            }
            if (collapse) {
                if (!collapsedPaths_.contains(path)) {
                    collapsedPaths_.insert(path);
                    changed = true;
                }
            } else if (collapsedPaths_.contains(path)) {
                collapsedPaths_.remove(path);
                changed = true;
            }
        }

        if (changed) {
            saveCollapsedFile();
            rebuild(false);
            restoreSelection(selectedPaths, true);
            recordHistory(QStringLiteral("折り畳み切替"), {}, {}, historyBefore, historySelection);
        } else {
            view_->setFocus(Qt::ShortcutFocusReason);
        }
        return true;
    }

    bool toggleSelectedPreviewOrCollapse()
    {
        if (Node* node = singleSelectedNode(); node && node->isSubRoot) {
            switchIntoSubRoot(node->path);  // Enter on a sub-root switches into it
            return true;
        }
        if (toggleSelectedFoldersCollapsed()) {
            return true;
        }
        return toggleSelectedFilePreviews();
    }

    bool handleBoardShortcut(QKeyEvent* event)
    {
        if (!event || event->isAutoRepeat()) {
            return false;
        }
        if (renameEdit_ || sideEditorEditing_ || textInputHasFocus()) {
            return false;
        }

        Qt::KeyboardModifiers modifiers = event->modifiers();
        modifiers &= ~Qt::KeypadModifier;
        if (boardMode_) {
            // Board mode: no file-structure operations (create/delete/rename/paste/move). Keep
            // preview, open and edit; everything else falls through to the view's own keys.
            if (event->key() == Qt::Key_E && modifiers == Qt::NoModifier) {
                return focusEditorForSelectedFile();
            }
            if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && modifiers == Qt::NoModifier) {
                return toggleSelectedFilePreviews();
            }
            if (event->key() == Qt::Key_O && modifiers == Qt::NoModifier) {
                return openSelectedNode();
            }
            return false;
        }
        if (event->key() == Qt::Key_E && modifiers == Qt::NoModifier) {
            return focusEditorForSelectedFile();
        }
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && modifiers == Qt::NoModifier) {
            return toggleSelectedPreviewOrCollapse();
        }
        if (event->key() == Qt::Key_O && modifiers == Qt::NoModifier) {
            return openSelectedNode();
        }
        if (event->key() == Qt::Key_Tab && modifiers == Qt::NoModifier) {
            return moveSelectionWithTab(false);
        }
        if ((event->key() == Qt::Key_Backtab) ||
            (event->key() == Qt::Key_Tab && modifiers == Qt::ShiftModifier)) {
            return moveSelectionWithTab(true);
        }
        if (event->key() == Qt::Key_Up && modifiers == Qt::NoModifier) {
            const bool handled = moveSelectionVertically(true);
            recordDebugEvent(QStringLiteral("node navigation Up: %1").arg(handled ? QStringLiteral("handled") : QStringLiteral("not handled")));
            return handled;
        }
        if (event->key() == Qt::Key_Down && modifiers == Qt::NoModifier) {
            const bool handled = moveSelectionVertically(false);
            recordDebugEvent(QStringLiteral("node navigation Down: %1").arg(handled ? QStringLiteral("handled") : QStringLiteral("not handled")));
            return handled;
        }
        if (event->key() == Qt::Key_Left && modifiers == Qt::NoModifier) {
            const bool handled = moveSelectionToParent();
            recordDebugEvent(QStringLiteral("node navigation Left: %1").arg(handled ? QStringLiteral("handled") : QStringLiteral("not handled")));
            return handled;
        }
        if (event->key() == Qt::Key_Right && modifiers == Qt::NoModifier) {
            const bool handled = moveSelectionToFirstChild();
            recordDebugEvent(QStringLiteral("node navigation Right: %1").arg(handled ? QStringLiteral("handled") : QStringLiteral("not handled")));
            return handled;
        }
        if (event->key() == Qt::Key_N && modifiers == Qt::NoModifier) {
            createFileInSelectedFolder();
            return true;
        }
        if (event->key() == Qt::Key_N && modifiers == Qt::ShiftModifier) {
            createFolderInSelectedFolder();
            return true;
        }
        if ((event->key() == Qt::Key_D && modifiers == Qt::NoModifier) ||
            isDeleteShortcut(event->key(), modifiers)) {
            if (selectedDeletableItemCount() <= 0) {
                return false;
            }
            deleteSelectedItems();
            return true;
        }
        if (isSelectAllShortcut(event->key(), modifiers)) {
            return selectAllVisibleNodes();
        }
        if (event->key() == Qt::Key_C && modifiers == Qt::ControlModifier) {
            return copySelectedItems();
        }
        if (event->key() == Qt::Key_V && modifiers == Qt::ControlModifier) {
            return pasteCopiedItems();
        }
        return false;
    }

    bool isDeleteShortcut(int key, Qt::KeyboardModifiers modifiers) const
    {
#ifdef Q_OS_MACOS
        return (key == Qt::Key_Delete && modifiers == Qt::NoModifier) ||
               (key == Qt::Key_Backspace && modifiers == Qt::NoModifier) ||
               (key == Qt::Key_Backspace && modifiers == Qt::MetaModifier);
#else
        return key == Qt::Key_Delete && modifiers == Qt::NoModifier;
#endif
    }

    bool isSelectAllShortcut(int key, Qt::KeyboardModifiers modifiers) const
    {
#ifdef Q_OS_MACOS
        return key == Qt::Key_A && modifiers == Qt::MetaModifier;
#else
        return key == Qt::Key_A && modifiers == Qt::ControlModifier;
#endif
    }

    bool selectAllVisibleNodes()
    {
        const bool selectedAny = selection_.selectAll();
        if (selectedAny) {
            view_->setFocus(Qt::ShortcutFocusReason);
            recordDebugEvent(QStringLiteral("selected all visible nodes"));
        }
        return selectedAny;
    }

    bool textInputHasFocus() const
    {
        return textInputWidgetHasFocus();
    }

    void openNode(Node* node)
    {
        if (!node) {
            return;
        }
        openPath(node->path);
    }

    // Switch the whole board to a different root folder (used by ファイル > フォルダを開く and the
    // ファイル > 履歴から開く list). Persists view state of the current root before switching.
    void openRootFolder(const QString& dir)
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

    // Turn a plain folder into a new child root: create its own .mycel, record the current root as
    // its parent, and refresh so the parent now shows it as a sub-root boundary.
    void makeFolderChildRoot(const QString& folderPath)
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

    static QJsonObject readJsonObjectFile(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QJsonDocument::fromJson(file.readAll()).object();
    }

    static void writeJsonObjectFile(const QString& path, const QJsonObject& object)
    {
        writeJsonFileAtomic(path, object);
    }

    // Merge a keyed object section (e.g. "colors"/"directories"/"previews") from a child .mycel file
    // into the parent's, re-keying each entry from child-relative to parent-relative.
    void mergeChildObjectSection(const QString& parentPath, const QString& childPath, const QString& section,
                                 const std::function<QString(const QString&)>& reKey)
    {
        const QJsonObject childSection = readJsonObjectFile(childPath).value(section).toObject();
        if (childSection.isEmpty()) {
            return;
        }
        QJsonObject parentRoot = readJsonObjectFile(parentPath);
        QJsonObject merged = parentRoot.value(section).toObject();
        for (auto it = childSection.begin(); it != childSection.end(); ++it) {
            merged.insert(reKey(it.key()), it.value());
        }
        parentRoot.insert(QStringLiteral("version"), 1);
        parentRoot.insert(section, merged);
        writeJsonObjectFile(parentPath, parentRoot);
    }

    // Merge a string-array section (e.g. "folders") from a child .mycel file into the parent's.
    void mergeChildStringArray(const QString& parentPath, const QString& childPath, const QString& section,
                               const std::function<QString(const QString&)>& reKey)
    {
        const QJsonArray childArray = readJsonObjectFile(childPath).value(section).toArray();
        if (childArray.isEmpty()) {
            return;
        }
        QJsonObject parentRoot = readJsonObjectFile(parentPath);
        QJsonArray merged = parentRoot.value(section).toArray();
        QSet<QString> seen;
        for (const QJsonValue& value : merged) {
            seen.insert(value.toString());
        }
        for (const QJsonValue& value : childArray) {
            const QString key = reKey(value.toString());
            if (!seen.contains(key)) {
                merged.append(key);
                seen.insert(key);
            }
        }
        parentRoot.insert(QStringLiteral("version"), 1);
        parentRoot.insert(section, merged);
        writeJsonObjectFile(parentPath, parentRoot);
    }

    // Merge the "links" array (from/to pairs) from a child .mycel file into the parent's.
    void mergeChildLinks(const QString& parentPath, const QString& childPath,
                         const std::function<QString(const QString&)>& reKey)
    {
        const QJsonArray childArray = readJsonObjectFile(childPath).value(QStringLiteral("links")).toArray();
        if (childArray.isEmpty()) {
            return;
        }
        QJsonObject parentRoot = readJsonObjectFile(parentPath);
        QJsonArray merged = parentRoot.value(QStringLiteral("links")).toArray();
        for (const QJsonValue& value : childArray) {
            const QJsonObject link = value.toObject();
            QJsonObject relinked;
            relinked.insert(QStringLiteral("from"), reKey(link.value(QStringLiteral("from")).toString()));
            relinked.insert(QStringLiteral("to"), reKey(link.value(QStringLiteral("to")).toString()));
            merged.append(relinked);
        }
        parentRoot.insert(QStringLiteral("version"), 1);
        parentRoot.insert(QStringLiteral("links"), merged);
        writeJsonObjectFile(parentPath, parentRoot);
    }

    // Integrate a sub-root back into its parent: merge the child's metadata (colors, order, previews,
    // collapse, links) into the parent's .mycel with re-keyed paths, remove the child's own .mycel,
    // then reload so the parent shows it as a plain (expandable) folder again.
    void integrateChildRootIntoParent(const QString& childDir)
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

    // Switch the board into a sub-root, first recording the current root as that child's parent
    // inside the child's own .mycel so the parent bar can be restored whenever the child is opened.
    void switchIntoSubRoot(const QString& childPath)
    {
        if (childPath.isEmpty()) {
            return;
        }
        if (mycelStorageEnabled_) {
            writeParentRootRecord(childPath, rootPath_);
        }
        openRootFolder(childPath);
    }

    // Persist the parent root into <childDir>/.mycel/parent.json. The parent is stored as a path
    // relative to the child so the record survives moving the whole tree.
    void writeParentRootRecord(const QString& childDir, const QString& parentDir)
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

    // Read the recorded parent root of a root directory, resolved to an absolute path. Empty when no
    // record exists or the recorded parent no longer has its own .mycel.
    QString recordedParentRoot(const QString& rootDir) const
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

    // The chain of parent roots from the outermost down to the immediate parent of the opened root.
    // Built by following the parent recorded in each .mycel; if none is recorded, falls back to the
    // nearest ancestor directories on disk that carry a .mycel. Empty for a top-level root.
    QStringList parentRootChain() const
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

    void openPath(const QString& path)
    {
        if (path.isEmpty()) {
            return;
        }

        const QFileInfo info(path);
        if (const auto embedUrl = youtubeEmbedUrlForFile(info)) {
            QDesktopServices::openUrl(QUrl(youtubeWatchUrlFromEmbedUrl(*embedUrl)));
            return;
        }
        if (info.exists() && info.isFile() && isTextPreviewFile(info) && info.size() <= 64 * 1024) {
            QFile file(info.absoluteFilePath());
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const auto url = firstUrlFromText(QString::fromUtf8(file.read(64 * 1024)));
                if (url && url->isValid()) {
                    QDesktopServices::openUrl(*url);
                    return;
                }
            }
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }

    // Let the user choose which application opens this file. On Windows this shows the native
    // "Open with" dialog; on other platforms the user picks an application and Mycel launches it
    // with the file as an argument.
    void openWithApplication(const QString& path)
    {
        const QFileInfo info(path);
        if (!info.exists()) {
            return;
        }
        const QString target = info.absoluteFilePath();
        recordDebugEvent(QStringLiteral("open with: %1").arg(relativeKeyForPath(path)));
#if defined(Q_OS_WIN)
        QProcess::startDetached(QStringLiteral("rundll32.exe"),
                                {QStringLiteral("shell32.dll,OpenAs_RunDLL"), QDir::toNativeSeparators(target)});
#elif defined(Q_OS_MACOS)
        const QString app = QFileDialog::getOpenFileName(this, QStringLiteral("アプリケーションを選択"),
                                                         QStringLiteral("/Applications"),
                                                         QStringLiteral("アプリケーション (*.app)"));
        if (!app.isEmpty()) {
            QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-a"), app, target});
        }
#else
        const QString app = QFileDialog::getOpenFileName(this, QStringLiteral("アプリケーションを選択"));
        if (!app.isEmpty()) {
            QProcess::startDetached(app, {target});
        }
#endif
    }

    // Order a folder's direct children by name or by modified date. Folders are kept before files
    // (matching the default scan order); descending reverses within each group. Recorded as one
    // undoable step (it only changes the persisted custom order).
    void sortFolderChildren(const QString& folderPath, bool byDate, bool descending)
    {
        const QFileInfo folderInfo(folderPath);
        if (!folderInfo.exists() || !folderInfo.isDir()) {
            return;
        }
        QDir dir(folderInfo.absoluteFilePath());
        QFileInfoList entries = dir.entryInfoList(
            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::NoSort);
        std::sort(entries.begin(), entries.end(), [byDate, descending](const QFileInfo& a, const QFileInfo& b) {
            if (a.isDir() != b.isDir()) {
                return a.isDir();  // folders first, regardless of direction
            }
            int cmp;
            if (byDate) {
                const QDateTime ta = a.lastModified();
                const QDateTime tb = b.lastModified();
                cmp = ta < tb ? -1 : (ta > tb ? 1 : 0);
                if (cmp == 0) {
                    cmp = QString::compare(a.fileName(), b.fileName(), Qt::CaseInsensitive);
                }
            } else {
                cmp = QString::compare(a.fileName(), b.fileName(), Qt::CaseInsensitive);
            }
            return descending ? cmp > 0 : cmp < 0;
        });

        QStringList order;
        for (const QFileInfo& entry : entries) {
            if (entry.fileName() == QStringLiteral(".mycel")) {
                continue;
            }
            order.append(entry.fileName());
        }

        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        fileOrders_[orderKeyForDirectory(folderInfo.absoluteFilePath())] = order;
        saveOrderFile();
        rebuild(false);
        restoreFolderSelection(folderInfo.absoluteFilePath());
        recordDebugEvent(QStringLiteral("sort folder: %1 by=%2 %3")
                             .arg(relativeKeyForPath(folderPath),
                                  byDate ? QStringLiteral("date") : QStringLiteral("name"),
                                  descending ? QStringLiteral("desc") : QStringLiteral("asc")));
        recordHistory(byDate ? QStringLiteral("日時で並べ替え") : QStringLiteral("名前で並べ替え"), {}, {},
                      historyBefore, historySelection);
    }

    bool isGoScriptFile(const QString& path) const
    {
        const QFileInfo info(path);
        return info.isFile() && info.suffix().compare(QStringLiteral("go"), Qt::CaseInsensitive) == 0;
    }

    // Scripts Mycel can run in a pipeline (Python or Go source).
    bool isPipelineScriptFile(const QString& path) const
    {
        const QFileInfo info(path);
        const QString suffix = info.suffix().toLower();
        return info.isFile() && (suffix == QStringLiteral("py") || suffix == QStringLiteral("go"));
    }

    // Interpreter for a script as {program, leadingArgs}; leadingArgs precede the script path
    // (e.g. {"run"} for `go run`). An empty program means no runner is installed/known.
    std::pair<QString, QStringList> pipelineRunnerFor(const QString& scriptPath) const
    {
        const QString suffix = QFileInfo(scriptPath).suffix().toLower();
        if (suffix == QStringLiteral("go")) {
            return {QStandardPaths::findExecutable(QStringLiteral("go")), {QStringLiteral("run")}};
        }
        if (suffix == QStringLiteral("py")) {
            for (const QString& candidate :
                 {QStringLiteral("python"), QStringLiteral("python3"), QStringLiteral("py")}) {
                const QString exe = QStandardPaths::findExecutable(candidate);
                if (!exe.isEmpty()) {
                    return {exe, {}};
                }
            }
        }
        return {QString(), {}};
    }

    // Run `program args...` in workingDir, streaming combined stdout/stderr into a non-modal
    // window. onFinished(exitCode) runs when the process ends (e.g. to refresh outputs); the
    // process is killed if the window closes.
    void runProcessWithOutputDialog(const QString& title, const QString& program, const QStringList& args,
                                    const QString& workingDir, std::function<void(int)> onFinished = {})
    {
        auto* dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(title);
        dialog->resize(760, 460);
        auto* layout = new QVBoxLayout(dialog);
        auto* output = new QPlainTextEdit(dialog);
        output->setReadOnly(true);
        output->setStyleSheet(QStringLiteral(
            "QPlainTextEdit { background: #111827; color: #e5e7eb; border: none; "
            "font-family: Menlo, Consolas, monospace; font-size: 12px; }"));
        auto* buttonRow = new QHBoxLayout();
        auto* stopButton = new QPushButton(QStringLiteral("停止"), dialog);
        auto* closeButton = new QPushButton(QStringLiteral("閉じる"), dialog);
        buttonRow->addWidget(stopButton);
        buttonRow->addStretch(1);
        buttonRow->addWidget(closeButton);
        layout->addWidget(output, 1);
        layout->addLayout(buttonRow);

        auto* process = new QProcess(dialog);
        process->setWorkingDirectory(workingDir);
        process->setProcessChannelMode(QProcess::MergedChannels);

        auto appendOutput = [output](const QString& text) {
            output->moveCursor(QTextCursor::End);
            output->insertPlainText(text);
            output->moveCursor(QTextCursor::End);
        };
        appendOutput(QStringLiteral("$ %1 %2\n\n").arg(QFileInfo(program).fileName(), args.join(QLatin1Char(' '))));

        connect(process, &QProcess::readyReadStandardOutput, dialog, [process, appendOutput] {
            appendOutput(QString::fromUtf8(process->readAllStandardOutput()));
        });
        connect(process, &QProcess::finished, dialog,
                [appendOutput, stopButton, onFinished](int code, QProcess::ExitStatus status) {
                    appendOutput(status == QProcess::CrashExit
                                     ? QStringLiteral("\n[プロセスが異常終了しました]")
                                     : QStringLiteral("\n[終了コード %1]").arg(code));
                    stopButton->setEnabled(false);
                    if (onFinished) {
                        onFinished(status == QProcess::CrashExit ? -1 : code);
                    }
                });
        connect(process, &QProcess::errorOccurred, dialog, [appendOutput, stopButton](QProcess::ProcessError) {
            appendOutput(QStringLiteral("\n[プロセスの起動に失敗しました]"));
            stopButton->setEnabled(false);
        });
        connect(stopButton, &QPushButton::clicked, dialog, [process] {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
            }
        });
        connect(closeButton, &QPushButton::clicked, dialog, &QDialog::close);
        connect(dialog, &QDialog::finished, process, [process](int) {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
                process->waitForFinished(1000);
            }
        });

        dialog->show();
        process->start(program, args);
    }

    // Run a single Go source file as a script (`go run <file>`), showing the output window.
    void runGoScript(const QString& filePath)
    {
        const QFileInfo info(filePath);
        if (!info.exists() || !info.isFile()) {
            return;
        }
        const QString goExe = QStandardPaths::findExecutable(QStringLiteral("go"));
        if (goExe.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 QStringLiteral("go コマンドが見つかりません。Go ツールチェーンをインストールし、PATH に追加してください。"));
            return;
        }
        recordDebugEvent(QStringLiteral("go run: %1").arg(relativeKeyForPath(filePath)));
        runProcessWithOutputDialog(QStringLiteral("Go 実行: %1").arg(info.fileName()), goExe,
                                   {QStringLiteral("run"), info.absoluteFilePath()}, info.absolutePath());
    }

    // The input feeding a script = the file linked INTO it (link.to == script).
    QString pipelineInputFor(const QString& scriptPath) const
    {
        for (const FileLink& link : fileLinks_) {
            if (link.to == scriptPath) {
                return link.from;
            }
        }
        return QString();
    }

    // The output of a script = the file linked OUT of it (link.from == script).
    QString pipelineOutputFor(const QString& scriptPath) const
    {
        for (const FileLink& link : fileLinks_) {
            if (link.from == scriptPath) {
                return link.to;
            }
        }
        return QString();
    }

    // Create an empty output file beside the script and link script -> output, as one undo step.
    QString createPipelineOutput(const QString& scriptPath, const QString& inputPath)
    {
        const QFileInfo scriptInfo(scriptPath);
        const QString dirPath = scriptInfo.absolutePath();
        QString base = QStringLiteral("output");
        QString ext = QStringLiteral("txt");
        if (!inputPath.isEmpty()) {
            const QFileInfo inputInfo(inputPath);
            base = inputInfo.completeBaseName() + QStringLiteral(".out");
            if (!inputInfo.suffix().isEmpty()) {
                ext = inputInfo.suffix();
            }
        }
        QString name = QStringLiteral("%1.%2").arg(base, ext);
        QString path = QDir(dirPath).filePath(name);
        for (int n = 2; QFileInfo::exists(path); ++n) {
            name = QStringLiteral("%1 %2.%3").arg(base).arg(n).arg(ext);
            path = QDir(dirPath).filePath(name);
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("出力ファイルを作成できませんでした。"));
            return QString();
        }
        file.close();
        beginHistoryGroup(QStringLiteral("出力ファイル作成"));
        appendCreatedItemToOrder(dirPath, name, false, scriptInfo.fileName());
        saveOrderFile();
        rebuild(false);
        const QString trashPath = allocateTrashPath(name);
        recordHistory(QStringLiteral("出力ファイル作成"), {{trashPath, path}}, {{path, trashPath}},
                      captureMetadataSnapshot(), selectedNodePaths());
        if (Node* from = nodeForPath(scriptPath)) {
            if (Node* to = nodeForPath(path)) {
                addFileLink(from, to);
            }
        }
        endHistoryGroup();
        return path;
    }

    // Run a pipeline stage: the script reads the linked input file and writes the linked output
    // file, invoked as `runner script <input> <output>`. Input/output are resolved from the
    // horizontal links into/out of the script node (output auto-created if absent).
    void runPipelineForScript(const QString& scriptPath)
    {
        const QFileInfo scriptInfo(scriptPath);
        if (!scriptInfo.exists() || scriptInfo.isDir()) {
            return;
        }
        const std::pair<QString, QStringList> runner = pipelineRunnerFor(scriptPath);
        if (runner.first.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 scriptInfo.suffix().toLower() == QStringLiteral("py")
                                     ? QStringLiteral("python が見つかりません。Python をインストールし、PATH に追加してください。")
                                     : QStringLiteral("このスクリプトを実行するコマンドが見つかりません。"));
            return;
        }
        const QString inputPath = pipelineInputFor(scriptPath);
        if (inputPath.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 QStringLiteral("入力ファイルが接続されていません。入力ファイルをこのスクリプトへドラッグしてリンクしてください。"));
            return;
        }
        QString outputPath = pipelineOutputFor(scriptPath);
        if (outputPath.isEmpty()) {
            outputPath = createPipelineOutput(scriptPath, inputPath);
            if (outputPath.isEmpty()) {
                return;
            }
        }

        QStringList args = runner.second;
        args << scriptInfo.absoluteFilePath() << QDir::toNativeSeparators(inputPath)
             << QDir::toNativeSeparators(outputPath);
        recordDebugEvent(QStringLiteral("pipeline run: %1 (in=%2 out=%3)")
                             .arg(relativeKeyForPath(scriptPath), relativeKeyForPath(inputPath),
                                  relativeKeyForPath(outputPath)));
        const QString outPath = outputPath;
        runProcessWithOutputDialog(QStringLiteral("パイプライン実行: %1").arg(scriptInfo.fileName()),
                                   runner.first, args, scriptInfo.absolutePath(),
                                   [this, outPath](int code) {
                                       rebuild(false);  // reflect the regenerated output file
                                       if (code == 0) {
                                           selectNodePath(outPath, true);
                                       }
                                   });
    }

    bool openSelectedNode()
    {
        Node* node = singleSelectedNode();
        if (!node) {
            return false;
        }
        openNode(node);
        view_->setFocus(Qt::ShortcutFocusReason);
        return true;
    }

    bool canEditTextFile(Node* node) const
    {
        if (!node || node->isDir) {
            return false;
        }
        const QFileInfo info(node->path);
        return info.exists() && info.isFile() && isTextPreviewFile(info) && info.size() <= 4 * 1024 * 1024;
    }

    bool canEditTextFilePath(const QString& path) const
    {
        const QFileInfo info(path);
        return info.exists() && info.isFile() && isTextPreviewFile(info) && info.size() <= 4 * 1024 * 1024;
    }

    void editTextFile(Node* node)
    {
        if (!canEditTextFile(node)) {
            QMessageBox::information(this, QStringLiteral("Mycel"), QStringLiteral("このファイルは内蔵エディタで編集できません。"));
            return;
        }

        openCenteredTextEditor(node->path);
    }

    void editTextFilePath(const QString& path)
    {
        Node* node = nodeForPath(path);
        if (node) {
            editTextFile(node);
            return;
        }
        if (!canEditTextFilePath(path)) {
            QMessageBox::information(this, QStringLiteral("Mycel"), QStringLiteral("このファイルは内蔵エディタで編集できません。"));
            return;
        }

        openCenteredTextEditor(path);
    }

    // `path` is taken by value on purpose: callers pass a reference into a Node's
    // `path` member (e.g. focusEditorForNode(node->path)), and rebuild() below frees
    // the whole node tree. A copy keeps the string alive for the post-rebuild uses.
    bool openCenteredTextEditor(QString path)
    {
        if (!canEditTextFilePath(path)) {
            return false;
        }
        if (!saveSideEditorNow()) {
            return false;
        }

        const bool previousSuppressSelectionUpdate = suppressSideEditorSelectionUpdate_;
        suppressSideEditorSelectionUpdate_ = true;
        selectNodePath(path);
        suppressSideEditorSelectionUpdate_ = previousSuppressSelectionUpdate;

        sideEditorEditing_ = false;
        if (sidePreviewStack_ &&
            (sidePreviewStack_->currentWidget() == sideEditor_ ||
             (sideEditorPanel_ && sideEditorPanel_->isVisible()))) {
            loadSidePreviewFile(path);
        }

        TextEditorDialog dialog(path, this);
        centerTextEditorDialog(&dialog);
        dialog.exec();
        if (dialog.wasSaved()) {
            rebuild(false);
            selectNodePath(path);
            if (sideEditorPanel_ && sideEditorPanel_->isVisible()) {
                loadSidePreviewFile(path);
            }
        }
        focusBoard();
        return true;
    }

    void centerTextEditorDialog(QDialog* dialog) const
    {
        if (!dialog) {
            return;
        }

        const QRect anchorRect = view_ && view_->viewport()
                                     ? QRect(view_->viewport()->mapToGlobal(QPoint(0, 0)), view_->viewport()->size())
                                     : QRect(mapToGlobal(QPoint(0, 0)), size());
        const QSize dialogSize = dialog->size();
        QPoint topLeft(anchorRect.center().x() - dialogSize.width() / 2,
                       anchorRect.center().y() - dialogSize.height() / 2);

        const QRect screenRect = screen() ? screen()->availableGeometry() : QRect();
        if (!screenRect.isNull()) {
            const int maxX = std::max(screenRect.left(), screenRect.right() - dialogSize.width() + 1);
            const int maxY = std::max(screenRect.top(), screenRect.bottom() - dialogSize.height() + 1);
            topLeft.setX(std::clamp(topLeft.x(), screenRect.left(), maxX));
            topLeft.setY(std::clamp(topLeft.y(), screenRect.top(), maxY));
        }
        dialog->move(topLeft);
    }

    void updateSideEditorForSelection()
    {
        if (sideEditorEditing_) {
            if (!saveSideEditorNow()) {
                return;
            }
            sideEditorEditing_ = false;
        }
        Node* node = singleSelectedNode();
        if (node && !node->isDir) {
            if (sideEditorPath_ == node->path && !sideEditorEditing_) {
                return;
            }
            loadSidePreviewFile(node->path);
            return;
        }

        if (node && node->isDir) {
            clearSideEditor(QStringLiteral("フォルダが選択されています。"), {});
        } else if (node) {
            clearSideEditor({}, {});
        } else {
            clearSideEditor({}, {});
        }
    }

    void syncEditorPaneVisibility()
    {
        if (!editorPaneAction_ || !sideEditorPanel_) {
            return;
        }

        const bool visible = editorPaneAction_->isChecked();
        sideEditorPanel_->setVisible(visible);
        if (visible) {
            applyEditorPanePosition(editorPanePosition_, false);
            updateSideEditorForSelection();
        }
    }

    void clearSideEditor(const QString& message, const QString& status)
    {
        if (!saveSideEditorNow()) {
            return;
        }
        stopSidePreviewMedia();
        sideEditorSaveTimer_.stop();
        sideEditorPath_.clear();
        sideEditorLastModified_ = {};
        sideEditorDirty_ = false;
        sideEditorEditing_ = false;
        sideEditorLoading_ = true;
        sideEditor_->setReadOnly(true);
        sideEditor_->setPlainText({});
        sideEditor_->setPlaceholderText(message);
        sideEditorLoading_ = false;
        sidePreviewText_->clear();
        sidePreviewText_->setPlainText(message);
        if (auto* imagePreview = dynamic_cast<AspectImagePreview*>(sidePreviewImage_)) {
            imagePreview->setPreviewPixmap({});
        } else {
            sidePreviewImage_->clear();
        }
#if MYCEL_HAS_WEBENGINE
        if (sideHtmlWeb_) {
            sideHtmlWeb_->setUrl(QUrl(QStringLiteral("about:blank")));
        }
#endif
        sidePreviewStack_->setCurrentWidget(sidePreviewText_);
        setSidePaneMode(false, status);
        sideEditorPathLabel_->setText(message);
        sideEditorStatusLabel_->setText(status);
    }

    void loadSideEditorFile(const QString& path)
    {
        if (path == sideEditorPath_ && sideEditorEditing_) {
            return;
        }
        if (!saveSideEditorNow()) {
            return;
        }
        stopSidePreviewMedia();

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            clearSideEditor(QStringLiteral("ファイルを開けませんでした。"), QStringLiteral("読み込み失敗"));
            return;
        }

        sideEditorPath_ = path;
        sideEditorLastModified_ = QFileInfo(path).lastModified();
        sideEditorDirty_ = false;
        sideEditorEditing_ = true;
        pendingFileSystemPaths_.clear();
        fileSystemRefreshTimer_.stop();
        sideEditorLoading_ = true;
        sideEditor_->setReadOnly(false);
        sideEditor_->setPlainText(QString::fromUtf8(file.readAll()));
        sideEditor_->moveCursor(QTextCursor::Start);
        sideEditorLoading_ = false;
        sidePreviewStack_->setCurrentWidget(sideEditor_);
        setSidePaneMode(true, QStringLiteral("編集中"));
        sideEditorPathLabel_->setText(QDir::toNativeSeparators(relativeKeyForPath(path)));
        sideEditorStatusLabel_->setText(QStringLiteral("保存済み"));
    }

    bool showUrlShortcutSidePreview(const QFileInfo& info)
    {
        const auto url = urlForShortcutFile(info);
        if (!url) {
            return false;
        }

        QString cachePath;
        if (const auto embedUrl = youtubeEmbedUrlForFile(info)) {
            const QString youtubeCachePath = youtubeThumbnailCachePathForEmbedUrl(*embedUrl);
            if (!youtubeCachePath.isEmpty() && QFileInfo::exists(youtubeCachePath)) {
                cachePath = youtubeCachePath;
            }
        }
        if (cachePath.isEmpty()) {
            const QString urlCachePath = urlThumbnailCachePathForUrl(*url);
            if (!urlCachePath.isEmpty() && QFileInfo::exists(urlCachePath)) {
                cachePath = urlCachePath;
            }
        }

        if (!cachePath.isEmpty()) {
            QPixmap pixmap(cachePath);
            if (!pixmap.isNull()) {
                if (auto* imagePreview = dynamic_cast<AspectImagePreview*>(sidePreviewImage_)) {
                    imagePreview->setPreviewPixmap(pixmap);
                } else {
                    sidePreviewImage_->setPixmap(pixmap);
                }
                sidePreviewStack_->setCurrentWidget(sidePreviewImage_);
                setSidePaneMode(false, QStringLiteral("URL プレビュー"));
                sideEditorStatusLabel_->setText(QStringLiteral("URL プレビュー"));
                return true;
            }
        }

        const bool started = fetchUrlThumbnailForPreview(info.absoluteFilePath(), *url, false);
        showSidePreviewText(started ? QStringLiteral("サムネイルを取得中です。\n\n%1").arg(url->toString())
                                    : QStringLiteral("URLプレビュー画像を取得できませんでした。\n\n%1").arg(url->toString()),
                            QStringLiteral("URL プレビュー"));
        return true;
    }

    void loadSidePreviewFile(const QString& path)
    {
        if (!saveSideEditorNow()) {
            return;
        }
        stopSidePreviewMedia();
        sideEditorPath_ = path;
        sideEditorLastModified_ = QFileInfo(path).lastModified();
        sideEditorDirty_ = false;
        sideEditorEditing_ = false;
        sideEditorLoading_ = true;
        sideEditor_->setReadOnly(true);
        sideEditor_->setPlainText({});
        sideEditorLoading_ = false;
        sideEditorPathLabel_->setText(QDir::toNativeSeparators(relativeKeyForPath(path)));
        setSidePaneMode(false, QStringLiteral("プレビュー"));

        const QFileInfo info(path);
        if (!info.exists() || !info.isFile()) {
            showSidePreviewText(QStringLiteral("ファイルを開けませんでした。"), QStringLiteral("読み込み失敗"));
            return;
        }
        if (showUrlShortcutSidePreview(info)) {
            return;
        }
        if (isDocumentThumbnailFile(info)) {
            // Generate a thumbnail only once the preview has been opened. Selecting a document with
            // its preview closed must not render anything — show a placeholder instead.
            const bool previewOpened = previewPaths_.contains(info.absoluteFilePath());
            const QPixmap pixmap = previewOpened ? documentThumbnail(info) : cachedDocumentThumbnail(info);
            if (!pixmap.isNull()) {
                if (auto* imagePreview = dynamic_cast<AspectImagePreview*>(sidePreviewImage_)) {
                    imagePreview->setPreviewPixmap(pixmap);
                } else {
                    sidePreviewImage_->setPixmap(pixmap);
                }
                sidePreviewStack_->setCurrentWidget(sidePreviewImage_);
                setSidePaneMode(false, QStringLiteral("1ページ目プレビュー"));
                sideEditorStatusLabel_->setText(QStringLiteral("1ページ目プレビュー"));
            } else {
                showSidePreviewText(QStringLiteral("プレビューを開くと表示します"),
                                    QStringLiteral("ドキュメント"));
            }
            return;
        }
        if (info.size() > 32 * 1024 * 1024 && !isVideoPreviewFile(info)) {
            showSidePreviewText(QStringLiteral("ファイルが大きいためプレビューしません。"), QStringLiteral("プレビュー不可"));
            return;
        }

        if (isImagePreviewFile(info)) {
            const QImage image = boundedPreviewImage(info, 4096);
            if (image.isNull()) {
                showSidePreviewText(QStringLiteral("画像が大きすぎるか、読み込めませんでした。"), QStringLiteral("画像プレビュー失敗"));
                return;
            }
            const QPixmap pixmap = QPixmap::fromImage(image);
            if (auto* imagePreview = dynamic_cast<AspectImagePreview*>(sidePreviewImage_)) {
                imagePreview->setPreviewPixmap(pixmap);
            } else {
                sidePreviewImage_->setPixmap(pixmap);
            }
            sidePreviewStack_->setCurrentWidget(sidePreviewImage_);
            setSidePaneMode(false, QStringLiteral("画像プレビュー"));
            sideEditorStatusLabel_->setText(QStringLiteral("画像プレビュー"));
            return;
        }

        if (isVideoPreviewFile(info)) {
            if (!sidePreviewPlayer_ || !sidePreviewVideo_) {
                showSidePreviewText(QStringLiteral("動画プレビューを利用できません。"), QStringLiteral("動画プレビュー無効"));
                return;
            }
            sidePreviewPlayer_->setSource(QUrl::fromLocalFile(info.absoluteFilePath()));
            sidePreviewStack_->setCurrentWidget(sidePreviewVideo_);
            sidePreviewPlayer_->play();
            setSidePaneMode(false, QStringLiteral("動画プレビュー"));
            sideEditorStatusLabel_->setText(QStringLiteral("動画プレビュー"));
            return;
        }

        if (!isTextPreviewFile(info) || info.size() > 4 * 1024 * 1024) {
            showSidePreviewText(QStringLiteral("このファイルはプレビューできません。"), QStringLiteral("プレビュー不可"));
            return;
        }

        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            showSidePreviewText(QStringLiteral("ファイルを開けませんでした。"), QStringLiteral("読み込み失敗"));
            return;
        }

        const QString text = QString::fromUtf8(file.readAll());
        sidePreviewText_->clear();
        if (isMarkdownPreviewFile(info)) {
            sidePreviewText_->setMarkdown(filterPreviewMetadataLines(text));
            setSidePaneMode(false, QStringLiteral("Markdown プレビュー"));
            sideEditorStatusLabel_->setText(QStringLiteral("Markdown プレビュー"));
        } else if (isHtmlPreviewFile(info)) {
#if MYCEL_HAS_WEBENGINE
            QWebEngineView* web = ensureHtmlPreviewView();
            web->setHtml(text, QUrl::fromLocalFile(info.absolutePath() + QStringLiteral("/")));
            sidePreviewStack_->setCurrentWidget(web);
#else
            sidePreviewText_->setHtml(text);
            sidePreviewStack_->setCurrentWidget(sidePreviewText_);
#endif
            setSidePaneMode(false, QStringLiteral("HTML プレビュー"));
            sideEditorStatusLabel_->setText(QStringLiteral("HTML プレビュー"));
            return;
        } else if (isCsvPreviewFile(info)) {
            sidePreviewText_->setHtml(csvToHtmlTable(text));
            setSidePaneMode(false, QStringLiteral("CSV プレビュー"));
            sideEditorStatusLabel_->setText(QStringLiteral("CSV プレビュー"));
        } else {
            sidePreviewText_->setPlainText(filterPreviewMetadataLines(text));
            setSidePaneMode(false, QStringLiteral("テキストプレビュー"));
            sideEditorStatusLabel_->setText(QStringLiteral("テキストプレビュー"));
        }
        sidePreviewStack_->setCurrentWidget(sidePreviewText_);
    }

#if MYCEL_HAS_WEBENGINE
    // QtWebEngine (Chromium) is heavy to initialize and spawns helper processes, yet it is only
    // needed to preview HTML files. Create the view on first use so launch stays fast and light
    // for the common case where no HTML file is previewed.
    QWebEngineView* ensureHtmlPreviewView()
    {
        if (sideHtmlWeb_) {
            return sideHtmlWeb_;
        }
        sideHtmlWeb_ = new QWebEngineView(sidePreviewStack_);
        sideHtmlWeb_->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
        sidePreviewStack_->addWidget(sideHtmlWeb_);
        sideHtmlWeb_->installEventFilter(this);
        connect(sideHtmlWeb_, &QWebEngineView::loadStarted, this,
                [this] { recordDebugEvent(QStringLiteral("html preview load started")); });
        connect(sideHtmlWeb_, &QWebEngineView::loadFinished, this, [this](bool ok) {
            recordDebugEvent(QStringLiteral("html preview load finished: %1")
                                 .arg(ok ? QStringLiteral("ok") : QStringLiteral("failed")));
        });
        return sideHtmlWeb_;
    }
#endif

    // Documents whose first page can be rendered to a thumbnail (currently PDF).
    bool isDocumentThumbnailFile(const QFileInfo& info) const
    {
#if MYCEL_HAS_PDF
        if (isPdfThumbnailFile(info)) {
            return true;
        }
#endif
        return isEpubThumbnailFile(info);
    }

    // Render the first page (PDF) or cover (EPUB) of a document to an image, at a modest
    // resolution. Empty on failure / unsupported build.
    QImage renderDocumentFirstPage(const QFileInfo& info) const
    {
        constexpr int kRenderWidth = 400;  // preview frames are at most ~500 px wide
#if MYCEL_HAS_PDF
        if (isPdfThumbnailFile(info)) {
            QPdfDocument doc;
            if (doc.load(info.absoluteFilePath()) != QPdfDocument::Error::None || doc.pageCount() < 1) {
                return {};
            }
            QSizeF pointSize = doc.pagePointSize(0);
            if (pointSize.isEmpty() || pointSize.width() <= 0.0) {
                pointSize = QSizeF(595.0, 842.0);  // A4 fallback
            }
            const qreal scale = kRenderWidth / pointSize.width();
            const QSize renderSize(qRound(pointSize.width() * scale), qRound(pointSize.height() * scale));
            return doc.render(0, renderSize, QPdfDocumentRenderOptions());
        }
#endif
        if (isEpubThumbnailFile(info)) {
            QImage cover = epubCoverImage(info);
            if (!cover.isNull() && cover.width() > kRenderWidth) {
                cover = cover.scaledToWidth(kRenderWidth, Qt::SmoothTransformation);
            }
            return cover;
        }
        return {};
    }

    // Disk cache path for a document's thumbnail under .mycel/thumbnails. The key embeds the
    // source's modified time and size so a changed file produces a new thumbnail.
    // Bump this when the rendering (e.g. resolution) changes, so existing caches are invalidated.
    static constexpr int kThumbnailVersion = 400;

    QString thumbnailCachePathFor(const QFileInfo& info) const
    {
        const QString key = QStringLiteral("%1|%2|%3|%4")
                                .arg(info.absoluteFilePath())
                                .arg(info.lastModified().toMSecsSinceEpoch())
                                .arg(info.size())
                                .arg(kThumbnailVersion);
        const QString hash = QString::fromLatin1(
            QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex());
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/thumbnails/") + hash + QStringLiteral(".png"));
    }

    QString documentThumbnailMemKey(const QFileInfo& info) const
    {
        return QStringLiteral("mycel:thumb:%1:%2:%3:%4")
            .arg(info.absoluteFilePath())
            .arg(info.lastModified().toMSecsSinceEpoch())
            .arg(info.size())
            .arg(kThumbnailVersion);
    }

    // Return an already-generated thumbnail from the memory/disk cache, or null. Never renders —
    // used where we must not pay the cost of generating a preview (e.g. on mere selection).
    QPixmap cachedDocumentThumbnail(const QFileInfo& info)
    {
        const QString memKey = documentThumbnailMemKey(info);
        QPixmap pixmap;
        if (QPixmapCache::find(memKey, &pixmap)) {
            return pixmap;
        }
        const QString cachePath = mycelStorageEnabled_ ? thumbnailCachePathFor(info) : QString();
        if (!cachePath.isEmpty() && QFileInfo::exists(cachePath) && pixmap.load(cachePath) && !pixmap.isNull()) {
            QPixmapCache::insert(memKey, pixmap);
            return pixmap;
        }
        return {};
    }

    // First-page thumbnail for a document. Returns the cached image if present, otherwise renders
    // the first page and caches it on disk (.mycel/thumbnails) and in memory. Call this only when
    // the preview is actually being opened, not on selection.
    QPixmap documentThumbnail(const QFileInfo& info)
    {
        QPixmap pixmap = cachedDocumentThumbnail(info);
        if (!pixmap.isNull()) {
            return pixmap;
        }
        const QImage image = renderDocumentFirstPage(info);
        if (image.isNull()) {
            return {};
        }
        pixmap = QPixmap::fromImage(image);
        QPixmapCache::insert(documentThumbnailMemKey(info), pixmap);
        if (mycelStorageEnabled_) {
            const QString cachePath = thumbnailCachePathFor(info);
            QDir().mkpath(QFileInfo(cachePath).absolutePath());
            image.save(cachePath, "PNG");
        }
        return pixmap;
    }

    void showSidePreviewText(const QString& text, const QString& status)
    {
        stopSidePreviewMedia();
        sidePreviewText_->clear();
        sidePreviewText_->setPlainText(text);
        sidePreviewStack_->setCurrentWidget(sidePreviewText_);
        setSidePaneMode(false, status);
        sideEditorStatusLabel_->setText(status);
    }

    void applyTheme(AppTheme theme, bool persist, bool refreshTree)
    {
        uiTheme_ = theme;
        const ThemeColors colors = themeColors(theme);
        if (qApp) {
            qApp->setProperty("mycelTheme", appThemeToString(theme));

            QPalette palette;
            palette.setColor(QPalette::Window, colors.window);
            palette.setColor(QPalette::WindowText, colors.windowText);
            palette.setColor(QPalette::Base, colors.base);
            palette.setColor(QPalette::AlternateBase, colors.alternateBase);
            palette.setColor(QPalette::Text, colors.text);
            palette.setColor(QPalette::Button, colors.button);
            palette.setColor(QPalette::ButtonText, colors.buttonText);
            palette.setColor(QPalette::Highlight, colors.highlight);
            palette.setColor(QPalette::HighlightedText, colors.highlightedText);
            palette.setColor(QPalette::ToolTipBase, colors.base);
            palette.setColor(QPalette::ToolTipText, colors.text);
            qApp->setPalette(palette);
            qApp->setStyleSheet(QStringLiteral(
                                    "QMainWindow, QDialog, QWidget { color: %1; }"
                                    "QToolBar { background: %2; border-bottom: 1px solid %3; spacing: 3px; }"
                                    "QToolButton, QPushButton { background: %4; color: %1; border: 1px solid %3; "
                                    "border-radius: 4px; padding: 4px 8px; }"
                                    "QToolButton:hover, QPushButton:hover { border-color: %5; }"
                                    "QMenu { background: %2; color: %1; border: 1px solid %3; }"
                                    "QMenu::item:selected { background: %5; color: %6; }"
                                    "QScrollBar { background: %2; }")
                                    .arg(cssColor(colors.text),
                                         cssColor(colors.window),
                                         cssColor(colors.inlinePreviewBorder),
                                         cssColor(colors.button),
                                         cssColor(colors.highlight),
                                         cssColor(colors.highlightedText)));
        }

        if (persist) {
            QSettings settings;
            settings.setValue(QStringLiteral("ui/theme"), appThemeToString(theme));
            settings.sync();
        }

        updateThemeActions();
        applyTextPaneTheme();
        if (!sideEditorEditing_ && !sideEditorPath_.isEmpty()) {
            loadSidePreviewFile(sideEditorPath_);
        } else {
            setSidePaneMode(sideEditorEditing_,
                            sideEditorEditing_ ? QStringLiteral("編集中")
                                               : (sideEditorStatusLabel_ ? sideEditorStatusLabel_->text() : QString()));
        }
        if (view_) {
            view_->setBackgroundBrush(colors.canvasBackground);
            view_->viewport()->update();
        }
        scene_.update();
        if (refreshTree && root_) {
            rebuild(false);
        }
    }

    void updateThemeActions()
    {
        if (lightThemeAction_) {
            lightThemeAction_->setChecked(uiTheme_ == AppTheme::Light);
        }
        if (darkThemeAction_) {
            darkThemeAction_->setChecked(uiTheme_ == AppTheme::Dark);
        }
    }

    void applyTextPaneTheme()
    {
        const ThemeColors colors = currentThemeColors();
        if (sidePreviewText_) {
            sidePreviewText_->setStyleSheet(QStringLiteral(
                                                "QTextEdit { background: %1; color: %2; border: 1px solid %3; "
                                                "border-radius: 4px; padding: 8px; selection-background-color: %4; "
                                                "selection-color: %5; }")
                                                .arg(cssColor(colors.previewTextBackground),
                                                     cssColor(colors.previewText),
                                                     cssColor(colors.previewTextBorder),
                                                     cssColor(colors.highlight),
                                                     cssColor(colors.highlightedText)));
            sidePreviewText_->document()->setDefaultStyleSheet(QStringLiteral(
                                                                   "* { color: %1; } a { color: %2; }")
                                                                   .arg(cssColor(colors.previewText),
                                                                        cssColor(colors.highlight)));
        }
        if (sideEditor_) {
            sideEditor_->setStyleSheet(QStringLiteral(
                                           "QPlainTextEdit { background: %1; color: %2; border: 2px solid %3; "
                                           "border-radius: 4px; padding: 8px; selection-background-color: %4; "
                                           "selection-color: %5; }")
                                           .arg(cssColor(colors.editTextBackground),
                                                cssColor(colors.editText),
                                                cssColor(colors.editTextBorder),
                                                cssColor(colors.highlight),
                                                cssColor(colors.highlightedText)));
        }
        if (debugText_) {
            const QColor debugBackground = uiTheme_ == AppTheme::Dark ? QColor("#0f1419") : QColor("#111827");
            debugText_->setStyleSheet(QStringLiteral(
                                          "QPlainTextEdit { background: %1; color: #e5e7eb; border: none; "
                                          "font-family: Menlo, Consolas, monospace; font-size: 11px; }")
                                          .arg(cssColor(debugBackground)));
        }
        if (renameEdit_) {
            applyRenameEditTheme(renameEdit_);
        }
    }

    void applyRenameEditTheme(QLineEdit* edit)
    {
        if (!edit) {
            return;
        }
        const ThemeColors colors = currentThemeColors();
        edit->setStyleSheet(QStringLiteral(
                                "QLineEdit { background: %1; color: %2; border: 1px solid %3; "
                                "border-radius: 5px; padding: 2px 5px; selection-background-color: %4; "
                                "selection-color: %5; }")
                                .arg(cssColor(colors.base),
                                     cssColor(colors.text),
                                     cssColor(colors.highlight),
                                     cssColor(colors.highlight),
                                     cssColor(colors.highlightedText)));
    }

    void setSidePaneMode(bool editing, const QString& detail)
    {
        if (!sideModeLabel_ || !sideEditorPanel_) {
            return;
        }

        const ThemeColors colors = currentThemeColors();
        if (editing) {
            sideModeLabel_->setText(detail.isEmpty()
                                        ? QStringLiteral("EDIT")
                                        : QStringLiteral("EDIT - %1").arg(detail));
            sideModeLabel_->setStyleSheet(QStringLiteral(
                                               "QLabel { background: %1; color: #ffffff; border-radius: 4px; "
                                               "font-weight: 700; padding: 3px 8px; }")
                                               .arg(cssColor(colors.editPanelBorder)));
            sideEditorPanel_->setStyleSheet(QStringLiteral(
                                                 "QWidget#SideEditorPanel { background: %1; border-left: 4px solid %2; }")
                                                 .arg(cssColor(colors.editPanel),
                                                      cssColor(colors.editPanelBorder)));
        } else {
            sideModeLabel_->setText(detail.isEmpty()
                                        ? QStringLiteral("PREVIEW")
                                        : QStringLiteral("PREVIEW - %1").arg(detail));
            sideModeLabel_->setStyleSheet(QStringLiteral(
                                               "QLabel { background: %1; color: #ffffff; border-radius: 4px; "
                                               "font-weight: 700; padding: 3px 8px; }")
                                               .arg(cssColor(colors.previewPanelBorder)));
            sideEditorPanel_->setStyleSheet(QStringLiteral(
                                                 "QWidget#SideEditorPanel { background: %1; border-left: 4px solid %2; }")
                                                 .arg(cssColor(colors.previewPanel),
                                                      cssColor(colors.previewPanelBorder)));
        }
    }

    void stopSidePreviewMedia()
    {
        if (sidePreviewPlayer_) {
            sidePreviewPlayer_->stop();
            sidePreviewPlayer_->setSource(QUrl());
        }
#if MYCEL_HAS_WEBENGINE
        if (sideHtmlWeb_) {
            sideHtmlWeb_->setUrl(QUrl(QStringLiteral("about:blank")));
        }
#endif
    }

    bool saveSideEditorNow()
    {
        if (sideEditorPath_.isEmpty() || !sideEditorDirty_) {
            return true;
        }
        sideEditorSaveTimer_.stop();

        QFile file(sideEditorPath_);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            sideEditorStatusLabel_->setText(QStringLiteral("保存失敗"));
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("右ペインの内容を保存できませんでした。"));
            return false;
        }

        const QByteArray bytes = sideEditor_->toPlainText().toUtf8();
        if (file.write(bytes) != bytes.size()) {
            sideEditorStatusLabel_->setText(QStringLiteral("保存失敗"));
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("右ペインの内容を保存できませんでした。"));
            return false;
        }
        file.close();

        sideEditorLastModified_ = QFileInfo(sideEditorPath_).lastModified();
        sideEditorDirty_ = false;
        sideEditorStatusLabel_->setText(QStringLiteral("保存済み"));
        refreshInlinePreviewForPath(sideEditorPath_);
        if (!sideEditorEditing_) {
            loadSidePreviewFile(sideEditorPath_);
        }
        return true;
    }

    void refreshInlinePreviewForPath(const QString& path)
    {
        if (!previewPaths_.contains(path)) {
            return;
        }

        const auto explicitSize = previewSizes_.find(path);
        const QSizeF previewSize = explicitSize == previewSizes_.end()
                                       ? automaticPreviewSize(QFileInfo(path))
                                       : explicitSize->second;
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node() && nodeItem->node()->path == path) {
                nodeItem->refreshPreviewWidget(previewSize);
                return;
            }
        }
    }

    QStringList inlinePreviewLines(Node* node) const
    {
        if (!node || node->isDir) {
            return {};
        }

        QFileInfo info(node->path);
        QStringList lines;

        if (isImagePreviewFile(info)) {
            lines << QStringLiteral("画像ファイル");
            return lines;
        }

        if (info.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0) {
            lines << QStringLiteral("PDFファイル");
            return lines;
        }

        if (!isTextPreviewFile(info) || info.size() > 1024 * 1024) {
            lines << QStringLiteral("プレビューできないファイルです");
            return lines;
        }

        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            lines << QStringLiteral("プレビューできません");
            return lines;
        }

        QTextStream stream(&file);
        while (lines.size() < 200 && !stream.atEnd()) {
            const QString line = stream.readLine();
            if (!isPreviewMetadataLine(line)) {
                lines << line;
            }
        }
        if (lines.isEmpty()) {
            lines << QStringLiteral("空のファイル");
        }
        return lines;
    }

    bool hasSavedPreviewSizePath(const QString& path) const
    {
        return previewSizes_.find(path) != previewSizes_.end() ||
               previewImageScales_.find(path) != previewImageScales_.end();
    }

    // Drop a file's saved preview size/scale so it reverts to the automatic (default) size.
    void resetPreviewSizePath(const QString& path)
    {
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        const bool hadSize = previewSizes_.erase(path) > 0;
        const bool hadScale = previewImageScales_.erase(path) > 0;
        if (!hadSize && !hadScale) {
            return;
        }
        savePreviewFile();
        relayout();
        selectNodePath(path, true);
        recordHistory(QStringLiteral("プレビューサイズ初期化"), {}, {}, historyBefore, historySelection);
    }

    QString inlineMarkdownPreviewText(Node* node) const
    {
        if (!node || node->isDir) {
            return {};
        }

        QFileInfo info(node->path);
        if (!isMarkdownPreviewFile(info)) {
            return {};
        }

        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QStringLiteral("_Preview unavailable_");
        }

        QByteArray data = file.read(64 * 1024);
        if (data.trimmed().isEmpty()) {
            return QStringLiteral("空のファイル");
        }
        if (!file.atEnd()) {
            data += "\n\n...";
        }
        const QString filtered = filterPreviewMetadataLines(QString::fromUtf8(data));
        return filtered.trimmed().isEmpty() ? QStringLiteral("空のファイル") : filtered;
    }

    void toggleInlinePreview(Node* node)
    {
        if (!node || node->isDir) {
            return;
        }
        toggleInlinePreviewPath(node->path);
    }

    bool isInlinePreviewOpen(Node* node) const
    {
        return node && !node->isDir && previewPaths_.contains(node->path);
    }

    void setInlinePreview(Node* node, bool open)
    {
        if (!node || node->isDir) {
            return;
        }
        const bool currentlyOpen = previewPaths_.contains(node->path);
        if (currentlyOpen == open) {
            return;
        }
        if (open) {
            openInlinePreviewPath(node->path);
        } else {
            closeInlinePreviewPath(node->path);
        }
    }

    void queueInlinePreviewToggle(Node* node)
    {
        if (!node || node->isDir) {
            return;
        }
        queuedCollapsePath_.clear();
        queuedPreviewPath_ = node->path;
        previewClickTimer_.start(QApplication::doubleClickInterval() + 40);
    }

    void queueCollapsedToggle(Node* node)
    {
        if (!node || !node->isDir) {
            return;
        }
        queuedPreviewPath_.clear();
        queuedCollapsePath_ = node->path;
        previewClickTimer_.start(QApplication::doubleClickInterval() + 40);
    }

    void cancelQueuedInlinePreviewToggle()
    {
        previewClickTimer_.stop();
        queuedPreviewPath_.clear();
        queuedCollapsePath_.clear();
    }

    void toggleInlinePreviewPath(const QString& path)
    {
        if (previewPaths_.contains(path)) {
            closeInlinePreviewPath(path);
            return;
        }
        openInlinePreviewPath(path);
    }

    void setPreviewSize(Node* node, const QSizeF& size, bool preferHeight = false)
    {
        if (!node || node->isDir) {
            return;
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        const QFileInfo info(node->path);
        if (isImagePreviewFile(info)) {
            const qreal scale = imagePreviewScaleForSize(info, size, preferHeight);
            previewImageScales_[node->path] = scale;
            previewSizes_[node->path] = imagePreviewSizeForScale(info, scale);
        } else {
            previewImageScales_.erase(node->path);
            previewSizes_[node->path] = clampedPreviewSizeForFile(info, size, preferHeight);
        }
        savePreviewFile();
        relayout();
        recordHistory(QStringLiteral("プレビューサイズ変更"), {}, {}, historyBefore, historySelection);
    }

    void setImagePreviewScale(Node* node, qreal scale)
    {
        if (!node || node->isDir) {
            return;
        }
        const QFileInfo info(node->path);
        if (!isImagePreviewFile(info)) {
            return;
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        const QSizeF size = imagePreviewSizeForScale(info, scale);
        const qreal normalizedScale = imagePreviewScaleForSize(info, size, false);
        previewImageScales_[node->path] = normalizedScale;
        previewSizes_[node->path] = size;
        savePreviewFile();
        relayout();
        recordHistory(QStringLiteral("プレビューサイズ変更"), {}, {}, historyBefore, historySelection);
    }

    void toggleCollapsed(Node* node)
    {
        if (!node || !node->isDir) {
            return;
        }
        toggleCollapsedPath(node->path);
    }

    void toggleCollapsedPath(const QString& path)
    {
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        if (collapsedPaths_.contains(path)) {
            collapsedPaths_.remove(path);
        } else {
            collapsedPaths_.insert(path);
        }
        saveCollapsedFile();
        rebuild(false);
        selectNodePath(path, true);
        recordHistory(QStringLiteral("折り畳み切替"), {}, {}, historyBefore, historySelection);
    }

    QColor colorForNode(const Node* node) const
    {
        if (!node) {
            return neutralStroke();
        }
        const auto found = userColors_.find(node->path);
        return found == userColors_.end() ? neutralStroke() : found->second;
    }

    QColor fillForNode(const Node* node) const
    {
        if (!node) {
            return neutralFill();
        }
        const auto found = userColors_.find(node->path);
        return found == userColors_.end() ? neutralFill() : softFillFromColor(found->second);
    }

    bool hasUserColor(Node* node) const
    {
        return mycelStorageEnabled_ && node && userColors_.find(node->path) != userColors_.end();
    }

    bool hasUserColorPath(const QString& path) const
    {
        return mycelStorageEnabled_ && userColors_.find(path) != userColors_.end();
    }

    bool hasUserColorForNode(const Node* node) const
    {
        return node && userColors_.find(node->path) != userColors_.end();
    }

    bool canDeleteFolder(Node* node) const
    {
        return node && node->isDir && node != root_.get();
    }

    void setNodeColor(Node* node, const QColor& color)
    {
        if (!node || !mycelStorageEnabled_ || !color.isValid()) {
            return;
        }
        const auto it = userColors_.find(node->path);
        if (it != userColors_.end() && it->second == color) {
            return;
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        userColors_[node->path] = color;
        saveColorFile();
        // Hash the file the moment the user marks it as worth tracking, rather than waiting for
        // a scan to get to it: a file colored right after being created/copied in has otherwise
        // not been observed yet, so an external rename before the next scan would be unrecoverable.
        ensureHashCachedForRenameTracking(node->path);
        scene_.update();  // color only affects painting, not layout
        recordHistory(QStringLiteral("色変更"), {}, {}, historyBefore, historySelection);
    }

    void setNodeColorPath(const QString& path, const QColor& color)
    {
        if (path.isEmpty() || !mycelStorageEnabled_ || !color.isValid()) {
            return;
        }
        const auto it = userColors_.find(path);
        if (it != userColors_.end() && it->second == color) {
            return;
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        userColors_[path] = color;
        saveColorFile();
        ensureHashCachedForRenameTracking(path);
        scene_.update();  // color only affects painting, not layout
        selectNodePath(path, true);
        recordHistory(QStringLiteral("色変更"), {}, {}, historyBefore, historySelection);
    }

    void clearNodeColor(Node* node)
    {
        if (!node || !mycelStorageEnabled_ || userColors_.find(node->path) == userColors_.end()) {
            return;
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        userColors_.erase(node->path);
        saveColorFile();
        scene_.update();  // color only affects painting, not layout
        recordHistory(QStringLiteral("色クリア"), {}, {}, historyBefore, historySelection);
    }

    void clearNodeColorPath(const QString& path)
    {
        if (path.isEmpty() || !mycelStorageEnabled_ || userColors_.find(path) == userColors_.end()) {
            return;
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        userColors_.erase(path);
        saveColorFile();
        scene_.update();  // color only affects painting, not layout
        selectNodePath(path, true);
        recordHistory(QStringLiteral("色クリア"), {}, {}, historyBefore, historySelection);
    }

    void beginInlineRename(Node* node)
    {
        if (!node || node == root_.get()) {
            return;
        }

        finishInlineRename(false);
        suspendInlineRenameActivity();

        NodeItem* target = nullptr;
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node() == node) {
                target = nodeItem;
                break;
            }
        }
        if (!target) {
            resumeInlineRenameActivity();
            return;
        }

        QFileInfo info(node->path);
        renamingPath_ = node->path;
        renameEdit_ = new QLineEdit(info.fileName());
        renameEdit_->setFrame(false);
        applyRenameEditTheme(renameEdit_);
        renameEdit_->installEventFilter(this);
        connect(renameEdit_, &QLineEdit::returnPressed, this, [this] {
            QTimer::singleShot(0, this, [this] { finishInlineRename(true); });
        });

        renameProxy_ = scene_.addWidget(renameEdit_);
        renameProxy_->setZValue(RenameLayerZ);
        const QRectF rect = target->labelSceneRect().adjusted(-2.0, -2.0, 8.0, 2.0);
        renameProxy_->setPos(rect.topLeft());
        renameProxy_->resize(rect.size());

        const int dot = info.fileName().lastIndexOf(QLatin1Char('.'));
        if (!node->isDir && dot > 0) {
            renameEdit_->setSelection(0, dot);
        } else {
            renameEdit_->selectAll();
        }
        QTimer::singleShot(0, this, [this] {
            if (!renameEdit_) {
                return;
            }
            renameEdit_->setFocus(Qt::OtherFocusReason);
            const QFileInfo info(renamingPath_);
            const int dot = info.fileName().lastIndexOf(QLatin1Char('.'));
            if (!info.isDir() && dot > 0) {
                renameEdit_->setSelection(0, dot);
            } else {
                renameEdit_->selectAll();
            }
        });
    }

    void renameFile(Node* node)
    {
        beginInlineRename(node);
    }

    void renamePathInline(const QString& path)
    {
        beginInlineRename(nodeForPath(path));
    }

    void renameSelectedItem()
    {
        beginInlineRename(singleSelectedNode());
    }

    void showCheatSheet()
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
                "テキスト編集画面内 Ctrl + S : 保存\n"
                "テキスト編集画面内 Esc : 未保存時に確認して閉じる\n"
                "Tab : 同じ階層の次の表示項目へ移動\n"
                "Shift + Tab : 同じ階層の前の表示項目へ移動\n"
                "↑ / ↓ : 同じフォルダ内の前/次の項目へ移動\n"
                "← : 上位フォルダへ移動\n"
                "→ : フォルダ内の最初の項目へ移動\n"
                "Enter : 選択ファイルのプレビュー表示/非表示、選択フォルダの折りたたみ/展開\n"
                "N : 選択フォルダに NewFile を作成\n"
                "Shift + N : 選択フォルダに NewFolder を作成\n"
                "D : 選択したファイル/フォルダの削除確認を表示\n"
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

    bool renamePathTo(const QString& path, const QString& name)
    {
        QFileInfo info(path);
        if (name.isEmpty() || name == info.fileName()) {
            return false;
        }
        if (name.contains('/') || name.contains('\\')) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("ファイル名に / または \\ は使えません。"));
            return false;
        }

        const QString destination = info.dir().filePath(name);
        // On a case-insensitive filesystem (Windows, default macOS) a case-only rename such as
        // Script.py -> script.py reports the destination as already existing because it resolves to
        // the SAME file. Allow that; only block when a genuinely different item is in the way.
        const bool caseOnlyRename = name.compare(info.fileName(), Qt::CaseInsensitive) == 0;
        if (QFileInfo::exists(destination) && !caseOnlyRename) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("同名のファイルまたはフォルダが既にあります。"));
            return false;
        }
        const bool wasDir = info.isDir();
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        pauseFileSystemWatcher();
        bool renamed = false;
        if (caseOnlyRename && QFileInfo::exists(destination)) {
            // QFile::rename refuses when the target "exists" (the same file under a different
            // case), so route the case change through a unique temporary name.
            QString tempPath = info.dir().filePath(QStringLiteral("~mycel-rename-%1").arg(info.fileName()));
            for (int n = 1; QFileInfo::exists(tempPath); ++n) {
                tempPath = info.dir().filePath(QStringLiteral("~mycel-rename-%1-%2").arg(n).arg(info.fileName()));
            }
            renamed = QFile::rename(path, tempPath) && QFile::rename(tempPath, destination);
            if (!renamed && QFileInfo::exists(tempPath)) {
                QFile::rename(tempPath, path);  // best-effort restore if the second step failed
            }
        } else {
            renamed = QFile::rename(path, destination);
        }
        if (!renamed) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("名前を変更できませんでした。"));
            return false;
        }

        if (mycelStorageEnabled_) {
            QStringList& order = fileOrders_[orderKeyForDirectory(info.absolutePath())];
            const int orderIndex = order.indexOf(info.fileName());
            if (orderIndex >= 0) {
                order[orderIndex] = name;
            }
        }
        rekeyPathMetadataAfterRename(path, destination, wasDir);
        saveColorFile();
        saveOrderFile();
        savePreviewFile();
        saveLinkFile();
        saveCollapsedFile();
        saveHashCacheFile();
        rebuild(false);
        recordHistory(QStringLiteral("名前変更"), {{path, destination}}, {{destination, path}},
                      historyBefore, historySelection);
        return true;
    }

    void deleteFile(Node* node)
    {
        if (!node || node->isDir) {
            return;
        }
        const QString filePath = node->path;
        const QString message = QStringLiteral("このファイルを削除しますか？\n%1").arg(filePath);
        if (QMessageBox::question(this, QStringLiteral("Mycel"), message,
                                  QMessageBox::Yes | QMessageBox::Cancel,
                                  QMessageBox::Cancel) != QMessageBox::Yes) {
            return;
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        const QString trashPath = moveToTrash(filePath);
        if (trashPath.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("ファイルを削除できませんでした。"));
            return;
        }
        removeDeletedPathMetadata(filePath, false);
        saveColorFile();
        saveOrderFile();
        savePreviewFile();
        saveLinkFile();
        saveCollapsedFile();
        saveHashCacheFile();
        rebuild(false);
        recordHistory(QStringLiteral("削除"), {{filePath, trashPath}}, {{trashPath, filePath}},
                      historyBefore, historySelection);
    }

    void deleteFilePath(const QString& path)
    {
        deleteFile(nodeForPath(path));
    }

    void deleteFolder(Node* node)
    {
        if (!node || !node->isDir || node == root_.get()) {
            return;
        }
        const QString folderPath = node->path;
        const QString message = QStringLiteral("このフォルダと配下の項目を削除しますか？\n%1").arg(folderPath);
        if (QMessageBox::question(this, QStringLiteral("Mycel"), message,
                                  QMessageBox::Yes | QMessageBox::Cancel,
                                  QMessageBox::Cancel) != QMessageBox::Yes) {
            return;
        }

        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        const QString trashPath = moveToTrash(folderPath);
        if (trashPath.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("フォルダを削除できませんでした。"));
            return;
        }

        removeDeletedPathMetadata(folderPath, true);
        saveColorFile();
        saveOrderFile();
        savePreviewFile();
        saveLinkFile();
        saveCollapsedFile();
        saveHashCacheFile();
        rebuild(false);
        recordHistory(QStringLiteral("削除"), {{folderPath, trashPath}}, {{trashPath, folderPath}},
                      historyBefore, historySelection);
    }

    void deleteFolderPath(const QString& path)
    {
        deleteFolder(nodeForPath(path));
    }

    // Apply the metadata side effects of a successful move. Pure filesystem work and
    // conflict-name resolution live in FileOperationService; this updates collapsed/color/
    // link/order metadata from the returned old->new path mapping. updateOrder distinguishes
    // the historical single-move rule (files only) from the multi-move rule (files + dirs).
    void applyMovedMetadata(const FileOperationService::MovedEntry& entry, const QString& targetDirPath,
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

    void moveNode(Node* source, Node* targetDir)
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

    void moveDragItemsToFolder(NodeItem* sourceItem, Node* targetDir)
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

    bool canImportExternalUrls(const QMimeData* mimeData, Node* targetDir) const
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

    void importExternalUrlsToFolder(const QList<QUrl>& urls, Node* targetDir)
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

    bool hasPasteableWebUrl(const QMimeData* mimeData) const
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

    bool canPasteClipboardToFolder(Node* targetDir) const
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

    bool canPasteClipboardToFolderPath(const QString& folderPath) const
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

    void pasteClipboardToFolder(Node* targetDir)
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

    void pasteClipboardToFolderPathAction(const QString& folderPath)
    {
        pasteClipboardToFolder(nodeForPath(folderPath));
    }

    NodeItem* folderItemAt(const QPointF& scenePos, const NodeItem* exclude) const
    {
        for (QGraphicsItem* item : scene_.items(scenePos)) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem != exclude && nodeItem->node()->isDir) {
                return nodeItem;
            }
        }
        return nullptr;
    }

    NodeItem* fileItemAt(const QPointF& scenePos, const NodeItem* exclude) const
    {
        for (QGraphicsItem* item : scene_.items(scenePos)) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem != exclude && !nodeItem->node()->isDir) {
                return nodeItem;
            }
        }
        return nullptr;
    }

    std::vector<NodeItem*> selectedTopLevelDragItems(const NodeItem* source) const
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

    bool isMultiDragSelection(const NodeItem* source) const
    {
        return selectedTopLevelDragItems(source).size() > 1;
    }

    void setDragVisuals(NodeItem* source, qreal opacity, qreal zValue)
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

    void previewDragSelection(NodeItem* source)
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

    NodeItem* folderItemForDrop(const NodeItem* source, const QPointF& scenePos) const
    {
        return DropTargetResolver::folderTarget(scene_, source, selectedTopLevelDragItems(source), scenePos);
    }

    NodeItem* linkTargetItemForDrop(const NodeItem* source, const QPointF& scenePos) const
    {
        return DropTargetResolver::linkTarget(scene_, source, isMultiDragSelection(source),
                                              mycelStorageEnabled_, scenePos);
    }

    NodeItem* nodeItemForPath(const QString& path) const
    {
        // O(1) lookup via the index maintained by renderCurrentTree (rebuilt with the scene).
        return path.isEmpty() ? nullptr : nodeItemsByPath_.value(path, nullptr);
    }

    NodeItem* updateInternalDropHover(NodeItem* source, const QPointF& scenePos)
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

    NodeItem* updateLinkDropHover(NodeItem* source, const QPointF& scenePos)
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

    void clearInternalDropHover()
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

    void clearLinkDropHover()
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

    void resetDropHoverState()
    {
        dragHoverFolderPath_.clear();
        dragHoverLinkTargetPath_.clear();
    }

    void addFileLink(Node* from, Node* to)
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

    // All link targets reachable from `sourcePath` by following from->to edges transitively.
    // A link target is displayed beside its source, so when the source moves to another folder
    // its whole chain of targets is moved with it — the visual grouping is kept physical.
    // Cycle-safe via the visited set.
    QStringList linkedDescendantPaths(const QString& sourcePath) const
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

    bool hasIncomingFileLink(Node* node) const
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

    bool hasIncomingFileLinkPath(const QString& path) const
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

    void removeIncomingFileLinks(Node* node)
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

    void removeIncomingFileLinksPath(const QString& path)
    {
        Node* node = nodeForPath(path);
        if (node) {
            removeIncomingFileLinks(node);
        }
    }

    struct FileLinkSiblingGroup {
        int pos = -1;  // path's own position within indices, or -1 if path is not a link target
        std::vector<std::size_t> indices;  // indices into fileLinks_, in link order
    };

    // Every target sharing `path`'s link source, in link order (the order layoutFileLinks fans
    // them out in), plus `path`'s own position in that list.
    FileLinkSiblingGroup fileLinkSiblingGroup(const QString& path) const
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

    // Whether `path` (a file-link target) has a sibling target in the given direction
    // (-1 = earlier/up, +1 = later/down) within the same source's link fan.
    bool canMoveFileLinkTargetPath(const QString& path, int direction) const
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

    // Swaps `path` with its neighbouring target within the same link source's fan, changing the
    // stacking order that layoutFileLinks uses (link order), then re-lays out. This is the only
    // way to reorder file-link targets: they live outside fileOrders_/reorderNodeByY entirely.
    void moveFileLinkTargetPath(const QString& path, int direction)
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

    bool reorderNodeByY(Node* source, const NodeItem* sourceItem, qreal dropCenterY)
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

    void previewReorder(Node* source, const NodeItem* sourceItem, qreal dragCenterY)
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

    void previewMoveDescendants(Node* source, const QPointF& delta)
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

    bool shouldKeepDragPreviewItem(const NodeItem* item, const std::vector<NodeItem*>& keepItems) const
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

    void clearDragPreviewForSource(const NodeItem* source)
    {
        std::vector<NodeItem*> keepItems = selectedTopLevelDragItems(source);
        if (keepItems.empty() && source) {
            keepItems.push_back(const_cast<NodeItem*>(source));
        }
        clearDragPreview(keepItems);
    }

    void clearDragPreview(const std::vector<NodeItem*>& keepItems = {})
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

    bool hasSelectedFiles() const
    {
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && !nodeItem->node()->isDir) {
                return true;
            }
        }
        return false;
    }

    int selectedFileCount() const
    {
        int count = 0;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && !nodeItem->node()->isDir) {
                ++count;
            }
        }
        return count;
    }

    int selectedDeletableItemCount() const
    {
        int count = 0;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem) {
                continue;
            }
            Node* node = nodeItem->node();
            if (node && (!node->isDir || canDeleteFolder(node))) {
                ++count;
            }
        }
        return count;
    }

    void setSelectedFilePreviews(bool open)
    {
        setFilePreviewsForPaths(selectedNodePaths(), open);
    }

    QStringList selectedFilePaths() const
    {
        QStringList paths;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem || !nodeItem->node() || nodeItem->node()->isDir) {
                continue;
            }
            paths.append(nodeItem->node()->path);
        }
        paths.removeDuplicates();
        return paths;
    }

    bool toggleFilePreviewsForPaths(const QStringList& paths)
    {
        QStringList filePaths;
        bool allPreviewsOpen = true;
        for (const QString& path : paths) {
            Node* node = findVisibleNodeByPath(root_.get(), path);
            if (!node || node->isDir) {
                continue;
            }
            filePaths.append(path);
            if (!previewPaths_.contains(path)) {
                allPreviewsOpen = false;
            }
        }
        if (filePaths.isEmpty()) {
            return false;
        }
        setFilePreviewsForPaths(filePaths, !allPreviewsOpen);
        return true;
    }

    void setFilePreviewsForPaths(const QStringList& paths, bool open)
    {
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        bool changed = false;
        QStringList selectedPaths;
        for (const QString& path : paths) {
            Node* node = findVisibleNodeByPath(root_.get(), path);
            if (!node) {
                continue;
            }
            selectedPaths.append(path);
            if (node->isDir) {
                continue;
            }
            if (open) {
                if (!previewPaths_.contains(path) && prepareInlinePreviewOpenPath(path)) {
                    previewPaths_.insert(path);
                    changed = true;
                }
            } else if (previewPaths_.contains(path)) {
                previewPaths_.remove(path);
                changed = true;
            }
        }

        if (changed) {
            savePreviewFile();
            relayout();
            restoreSelection(selectedPaths);
            recordHistory(QStringLiteral("プレビュー切替"), {}, {}, historyBefore, historySelection);
        }
    }

    bool openInlinePreviewPath(const QString& path)
    {
        if (previewPaths_.contains(path) || !prepareInlinePreviewOpenPath(path)) {
            return false;
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        previewPaths_.insert(path);
        savePreviewFile();
        relayout();
        selectNodePath(path);
        recordHistory(QStringLiteral("プレビュー切替"), {}, {}, historyBefore, historySelection);
        return true;
    }

    bool closeInlinePreviewPath(const QString& path)
    {
        if (!previewPaths_.contains(path)) {
            return false;
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        previewPaths_.remove(path);
        savePreviewFile();
        relayout();
        selectNodePath(path);
        recordHistory(QStringLiteral("プレビュー切替"), {}, {}, historyBefore, historySelection);
        return true;
    }

    bool prepareInlinePreviewOpenPath(const QString& path)
    {
        const QFileInfo info(path);
        const auto embedUrl = youtubeEmbedUrlForFile(info);
        if (embedUrl) {
            const QString cachePath = youtubeThumbnailCachePathForEmbedUrl(*embedUrl);
            if (!cachePath.isEmpty() && QFileInfo::exists(cachePath)) {
                return true;
            }

            fetchYouTubeThumbnailForInlinePreview(path, *embedUrl);
            return false;
        }

        const auto shortcutUrl = urlForShortcutFile(info);
        if (!shortcutUrl) {
            return true;
        }

        const QString urlCachePath = urlThumbnailCachePathForUrl(*shortcutUrl);
        if (!urlCachePath.isEmpty() && QFileInfo::exists(urlCachePath)) {
            return true;
        }

        fetchUrlThumbnailForPreview(path, *shortcutUrl, true);
        return false;
    }

    QString youtubeThumbnailCacheDirectoryPath() const
    {
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/youtube-thumbnails"));
    }

    QString youtubeThumbnailCachePathForEmbedUrl(const QString& embedUrl) const
    {
        const QString videoId = youtubeVideoIdFromEmbedUrl(embedUrl);
        if (videoId.isEmpty()) {
            return {};
        }
        return QDir(youtubeThumbnailCacheDirectoryPath()).filePath(videoId + QStringLiteral(".jpg"));
    }

    QString urlThumbnailCacheDirectoryPath() const
    {
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/url-thumbnails"));
    }

    QString urlThumbnailCachePathForUrl(const QUrl& url) const
    {
        if (!url.isValid() || url.scheme().isEmpty()) {
            return {};
        }
        const QString key = url.toString();
        const QString hash = QString::fromLatin1(
            QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex());
        return QDir(urlThumbnailCacheDirectoryPath()).filePath(hash + QStringLiteral(".png"));
    }

    void fetchYouTubeThumbnailForInlinePreview(const QString& path, const QString& embedUrl)
    {
        if (!mycelStorageEnabled_) {
            recordDebugEvent(QStringLiteral("youtube thumbnail skipped: .mycel disabled"));
            return;
        }

        const QString thumbnailUrl = youtubeThumbnailUrlFromEmbedUrl(embedUrl);
        const QString cachePath = youtubeThumbnailCachePathForEmbedUrl(embedUrl);
        if (thumbnailUrl.isEmpty() || cachePath.isEmpty()) {
            recordDebugEvent(QStringLiteral("youtube thumbnail skipped: invalid url"));
            return;
        }
        if (pendingYouTubeThumbnailPaths_.contains(path)) {
            return;
        }

        QDir dir;
        if (!dir.mkpath(youtubeThumbnailCacheDirectoryPath())) {
            recordDebugEvent(QStringLiteral("youtube thumbnail cache mkdir failed"));
            return;
        }

        pendingYouTubeThumbnailPaths_.insert(path);
        recordDebugEvent(QStringLiteral("youtube thumbnail fetch: %1").arg(relativeKeyForPath(path)));

        auto* manager = new QNetworkAccessManager(this);
        QNetworkRequest request{QUrl(thumbnailUrl)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* reply = manager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, manager, reply, path, cachePath] {
            const QNetworkReply::NetworkError error = reply->error();
            const QByteArray data = reply->readAll();
            reply->deleteLater();
            manager->deleteLater();
            pendingYouTubeThumbnailPaths_.remove(path);

            QPixmap pixmap;
            if (error != QNetworkReply::NoError || !pixmap.loadFromData(data)) {
                recordDebugEvent(QStringLiteral("youtube thumbnail fetch failed: %1").arg(relativeKeyForPath(path)));
                return;
            }

            QFile file(cachePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
                file.write(data) != data.size()) {
                recordDebugEvent(QStringLiteral("youtube thumbnail cache write failed"));
                return;
            }
            file.close();

            if (!QFileInfo::exists(path)) {
                return;
            }
            previewPaths_.insert(path);
            savePreviewFile();
            relayout();
            selectNodePath(path);
            recordDebugEvent(QStringLiteral("youtube thumbnail cached: %1").arg(relativeKeyForPath(path)));
        });
    }

    bool fetchUrlThumbnailForPreview(const QString& path, const QUrl& pageUrl, bool openInlinePreview)
    {
        if (!mycelStorageEnabled_) {
            recordDebugEvent(QStringLiteral("url thumbnail skipped: .mycel disabled"));
            return false;
        }

        const QString cachePath = urlThumbnailCachePathForUrl(pageUrl);
        if (cachePath.isEmpty()) {
            recordDebugEvent(QStringLiteral("url thumbnail skipped: invalid url"));
            return false;
        }

        if (openInlinePreview) {
            pendingUrlThumbnailInlineOpenPaths_.insert(path);
        }
        if (pendingUrlThumbnailPaths_.contains(path)) {
            return true;
        }

        QDir dir;
        if (!dir.mkpath(urlThumbnailCacheDirectoryPath())) {
            recordDebugEvent(QStringLiteral("url thumbnail cache mkdir failed"));
            return false;
        }

        pendingUrlThumbnailPaths_.insert(path);
        recordDebugEvent(QStringLiteral("url thumbnail page fetch: %1").arg(relativeKeyForPath(path)));

        auto* pageManager = new QNetworkAccessManager(this);
        QNetworkRequest pageRequest{pageUrl};
        pageRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        pageRequest.setHeader(QNetworkRequest::UserAgentHeader,
                              QStringLiteral("Mozilla/5.0 Mycel/%1").arg(QString::fromLatin1(MYCEL_VERSION)));
        QNetworkReply* pageReply = pageManager->get(pageRequest);
        connect(pageReply, &QNetworkReply::finished, this, [this, pageManager, pageReply, path, pageUrl, cachePath] {
            const QNetworkReply::NetworkError pageError = pageReply->error();
            const QByteArray pageData = pageReply->readAll().left(512 * 1024);
            pageReply->deleteLater();
            pageManager->deleteLater();

            if (pageError != QNetworkReply::NoError) {
                pendingUrlThumbnailPaths_.remove(path);
                pendingUrlThumbnailInlineOpenPaths_.remove(path);
                recordDebugEvent(QStringLiteral("url thumbnail page fetch failed: %1").arg(relativeKeyForPath(path)));
                if (sideEditorPath_ == path && !sideEditorEditing_) {
                    showSidePreviewText(QStringLiteral("URLプレビュー画像を取得できませんでした。\n\n%1")
                                            .arg(pageUrl.toString()),
                                        QStringLiteral("URL プレビュー"));
                }
                return;
            }

            const auto imageUrl = firstImageUrlFromHtml(QString::fromUtf8(pageData), pageUrl);
            if (!imageUrl) {
                pendingUrlThumbnailPaths_.remove(path);
                pendingUrlThumbnailInlineOpenPaths_.remove(path);
                recordDebugEvent(QStringLiteral("url thumbnail image not found: %1").arg(relativeKeyForPath(path)));
                if (sideEditorPath_ == path && !sideEditorEditing_) {
                    showSidePreviewText(QStringLiteral("URLプレビュー画像が見つかりませんでした。\n\n%1")
                                            .arg(pageUrl.toString()),
                                        QStringLiteral("URL プレビュー"));
                }
                return;
            }

            auto* imageManager = new QNetworkAccessManager(this);
            QNetworkRequest imageRequest{*imageUrl};
            imageRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
            imageRequest.setHeader(QNetworkRequest::UserAgentHeader,
                                   QStringLiteral("Mozilla/5.0 Mycel/%1").arg(QString::fromLatin1(MYCEL_VERSION)));
            QNetworkReply* imageReply = imageManager->get(imageRequest);
            connect(imageReply, &QNetworkReply::finished, this,
                    [this, imageManager, imageReply, path, pageUrl, cachePath] {
                        const QNetworkReply::NetworkError imageError = imageReply->error();
                        const QByteArray imageData = imageReply->readAll();
                        imageReply->deleteLater();
                        imageManager->deleteLater();
                        pendingUrlThumbnailPaths_.remove(path);

                        QPixmap pixmap;
                        if (imageError != QNetworkReply::NoError || !pixmap.loadFromData(imageData)) {
                            pendingUrlThumbnailInlineOpenPaths_.remove(path);
                            recordDebugEvent(QStringLiteral("url thumbnail image fetch failed: %1")
                                                 .arg(relativeKeyForPath(path)));
                            if (sideEditorPath_ == path && !sideEditorEditing_) {
                                showSidePreviewText(QStringLiteral("URLプレビュー画像を取得できませんでした。\n\n%1")
                                                        .arg(pageUrl.toString()),
                                                    QStringLiteral("URL プレビュー"));
                            }
                            return;
                        }

                        if (!pixmap.save(cachePath, "PNG")) {
                            pendingUrlThumbnailInlineOpenPaths_.remove(path);
                            recordDebugEvent(QStringLiteral("url thumbnail cache write failed"));
                            return;
                        }

                        if (sideEditorPath_ == path && !sideEditorEditing_) {
                            if (auto* imagePreview = dynamic_cast<AspectImagePreview*>(sidePreviewImage_)) {
                                imagePreview->setPreviewPixmap(pixmap);
                            } else {
                                sidePreviewImage_->setPixmap(pixmap);
                            }
                            sidePreviewStack_->setCurrentWidget(sidePreviewImage_);
                            setSidePaneMode(false, QStringLiteral("URL プレビュー"));
                            sideEditorStatusLabel_->setText(QStringLiteral("URL プレビュー"));
                        }

                        const bool openInline = pendingUrlThumbnailInlineOpenPaths_.contains(path);
                        pendingUrlThumbnailInlineOpenPaths_.remove(path);
                        if (openInline && QFileInfo::exists(path)) {
                            previewPaths_.insert(path);
                            savePreviewFile();
                            relayout();
                            selectNodePath(path);
                        }
                        recordDebugEvent(QStringLiteral("url thumbnail cached: %1").arg(relativeKeyForPath(path)));
                    });
        });
        return true;
    }

    void deleteSelectedItems()
    {
        QStringList filePaths;
        QStringList folderPaths;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem) {
                continue;
            }
            Node* node = nodeItem->node();
            if (!node) {
                continue;
            }
            if (node->isDir) {
                if (canDeleteFolder(node)) {
                    folderPaths.append(node->path);
                }
            } else {
                filePaths.append(node->path);
            }
        }
        filePaths.removeDuplicates();
        folderPaths.removeDuplicates();

        QStringList topLevelFolderPaths;
        for (const QString& folderPath : folderPaths) {
            bool containedBySelectedFolder = false;
            for (const QString& otherFolderPath : folderPaths) {
                if (otherFolderPath != folderPath && isDescendantPath(otherFolderPath, folderPath)) {
                    containedBySelectedFolder = true;
                    break;
                }
            }
            if (!containedBySelectedFolder) {
                topLevelFolderPaths.append(folderPath);
            }
        }
        folderPaths = topLevelFolderPaths;

        QStringList topLevelFilePaths;
        for (const QString& filePath : filePaths) {
            bool containedBySelectedFolder = false;
            for (const QString& folderPath : folderPaths) {
                if (isDescendantPath(folderPath, filePath)) {
                    containedBySelectedFolder = true;
                    break;
                }
            }
            if (!containedBySelectedFolder) {
                topLevelFilePaths.append(filePath);
            }
        }
        filePaths = topLevelFilePaths;

        const int selectedCount = filePaths.size() + folderPaths.size();
        if (selectedCount == 0) {
            return;
        }

        const QString message = QStringLiteral("選択した %1 個の項目を削除しますか？\nフォルダは配下の項目も削除されます。")
                                    .arg(selectedCount);
        if (QMessageBox::question(this, QStringLiteral("Mycel"), message,
                                  QMessageBox::Yes | QMessageBox::Cancel,
                                  QMessageBox::Cancel) != QMessageBox::Yes) {
            return;
        }

        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        std::vector<std::pair<QString, QString>> redoMoves;
        std::vector<std::pair<QString, QString>> undoMoves;

        QStringList failed;
        for (const QString& path : filePaths) {
            const QString trashPath = moveToTrash(path);
            if (trashPath.isEmpty()) {
                failed.append(path);
                continue;
            }
            redoMoves.push_back({path, trashPath});
            undoMoves.push_back({trashPath, path});
            removeDeletedPathMetadata(path, false);
        }

        for (const QString& path : folderPaths) {
            const QString trashPath = moveToTrash(path);
            if (trashPath.isEmpty()) {
                failed.append(path);
                continue;
            }
            redoMoves.push_back({path, trashPath});
            undoMoves.push_back({trashPath, path});
            removeDeletedPathMetadata(path, true);
        }

        saveColorFile();
        saveOrderFile();
        savePreviewFile();
        saveLinkFile();
        saveCollapsedFile();
        saveHashCacheFile();
        rebuild(false);

        if (!redoMoves.empty()) {
            recordHistory(QStringLiteral("削除"), std::move(redoMoves), std::move(undoMoves),
                          historyBefore, historySelection);
        }

        if (!failed.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 QStringLiteral("削除できなかった項目があります。\n%1").arg(failed.join(QLatin1Char('\n'))));
        }
    }

    // ===== Undo/Redo history =================================================
    // A snapshot of every persisted, path-keyed metadata container. Restoring a snapshot
    // re-keys all metadata back to a prior state in one shot, so per-operation undo logic only
    // has to reverse the filesystem moves; the metadata follows from the snapshot.
    // One card of the free-form board layout (see the board-mode section below).
    struct BoardCardState {
        QPointF pos;
        bool visible = true;
    };

    struct MetadataSnapshot {
        QSet<QString> collapsedPaths;
        std::map<QString, QColor> userColors;
        QSet<QString> previewPaths;
        std::map<QString, QSizeF> previewSizes;
        std::map<QString, qreal> previewImageScales;
        std::map<QString, QStringList> fileOrders;
        std::vector<FileLink> fileLinks;
        // Board: the active pattern and its cards at capture time (empty name = no board data).
        QString boardPatternName;
        std::map<QString, BoardCardState> boardCards;
    };
    // One undoable action. redoMoves replays the filesystem changes; undoMoves reverses them.
    // create/delete/import are normalized to moves to/from .mycel/trash so every structural
    // change is expressed as a reversible rename.
    struct HistoryEntry {
        QString description;
        std::vector<std::pair<QString, QString>> redoMoves;
        std::vector<std::pair<QString, QString>> undoMoves;
        MetadataSnapshot before;
        MetadataSnapshot after;
        QStringList selectionBefore;
        QStringList selectionAfter;
    };

    MetadataSnapshot captureMetadataSnapshot() const
    {
        return MetadataSnapshot{collapsedPaths_, userColors_,  previewPaths_, previewSizes_,
                                previewImageScales_, fileOrders_, fileLinks_,
                                boardPatternName_, boardCards_};
    }

    void restoreMetadataSnapshot(const MetadataSnapshot& s)
    {
        collapsedPaths_ = s.collapsedPaths;
        userColors_ = s.userColors;
        previewPaths_ = s.previewPaths;
        previewSizes_ = s.previewSizes;
        previewImageScales_ = s.previewImageScales;
        fileOrders_ = s.fileOrders;
        fileLinks_ = s.fileLinks;
        if (!s.boardPatternName.isEmpty()) {
            if (s.boardPatternName == boardPatternName_) {
                boardCards_ = s.boardCards;
                saveBoardPattern();
            } else {
                // The active pattern changed since this entry: restore into that pattern's file.
                restoreBoardCardsToPatternFile(s.boardPatternName, s.boardCards);
            }
        }
    }

    // Write a card set back into a (non-active) pattern file, keeping its stored view.
    void restoreBoardCardsToPatternFile(const QString& name, const std::map<QString, BoardCardState>& cards)
    {
        if (!mycelStorageEnabled_ || name.isEmpty()) {
            return;
        }
        QJsonObject rootObject = readJsonObjectFile(boardPatternFilePath(name));
        QJsonObject cardsObject;
        for (const auto& [key, state] : cards) {
            QJsonObject card;
            card.insert(QStringLiteral("x"), state.pos.x());
            card.insert(QStringLiteral("y"), state.pos.y());
            card.insert(QStringLiteral("visible"), state.visible);
            cardsObject.insert(key, card);
        }
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("cards"), cardsObject);
        writeJsonFileAtomic(boardPatternFilePath(name), rootObject);
    }

    void saveAllMetadata()
    {
        saveColorFile();
        saveOrderFile();
        savePreviewFile();
        saveLinkFile();
        saveCollapsedFile();
        saveHashCacheFile();
    }

    QString historyTrashDir() const
    {
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/trash"));
    }

    // The undo history (and its trashed items) lives only for the session. Wipe .mycel/trash at
    // startup and shutdown so deleted/undone items don't pile up on disk across runs.
    void cleanHistoryTrash()
    {
        QDir trash(historyTrashDir());
        if (trash.exists()) {
            trash.removeRecursively();
        }
        historyTrashCounter_ = 0;
    }

    // A fresh, collision-free path under .mycel/trash holding one item by its original name, so
    // undo/redo can move it straight back to (or out of) the tree.
    QString allocateTrashPath(const QString& name)
    {
        QString slotDir = QDir(historyTrashDir()).filePath(QString::number(++historyTrashCounter_));
        while (QFileInfo::exists(slotDir)) {
            slotDir = QDir(historyTrashDir()).filePath(QString::number(++historyTrashCounter_));
        }
        QDir().mkpath(slotDir);
        return QDir(slotDir).filePath(name);
    }

    // On Windows QFileSystemWatcher keeps an open handle on every watched directory, which blocks
    // renaming/moving those directories ("Access is denied"). Drop all watched paths before a
    // structural filesystem move; the rebuild that follows re-establishes them via
    // resetFileSystemWatcher().
    void pauseFileSystemWatcher()
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

    // Move a tree item into .mycel/trash and return its new path (empty on failure). Deletes go
    // through here so they can be reversed by undo (the item is recovered, not destroyed).
    QString moveToTrash(const QString& path)
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

    bool applyHistoryMoves(const std::vector<std::pair<QString, QString>>& moves)
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

    void pushHistoryEntry(HistoryEntry entry)
    {
        redoStack_.clear();
        undoStack_.push_back(std::move(entry));
        while (static_cast<int>(undoStack_.size()) > kMaxHistoryEntries) {
            undoStack_.erase(undoStack_.begin());
        }
        updateUndoRedoActions();
    }

    // Record one undoable action. `before` must be captured by the caller at the very start of
    // the op (while paths are still in their old form). When a group is open the moves are
    // folded into it instead of pushed, so a multi-item gesture undoes as a single step.
    void recordHistory(const QString& description,
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

    void beginHistoryGroup(const QString& description)
    {
        if (applyingHistory_ || historyGroup_) {
            return;
        }
        historyGroup_ = std::make_unique<HistoryEntry>();
        historyGroup_->description = description;
        historyGroup_->before = captureMetadataSnapshot();
        historyGroup_->selectionBefore = selectedNodePaths();
    }

    void endHistoryGroup()
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

    void performUndo()
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

    void performRedo()
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

    void updateUndoRedoActions()
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

    bool hasSelectedItems() const
    {
        return !scene_.selectedItems().isEmpty();
    }

    QStringList selectedNodePaths() const
    {
        return selection_.selectedPaths();
    }

    // Selection primitive shared by NodeItem (drag start, click, double-click). additive
    // keeps the current selection for Cmd/Ctrl-click.
    void selectNodeItem(NodeItem* item, bool additive)
    {
        selection_.select(item, additive);
        if (item && item->node()) {
            selectionRangeAnchorPath_ = item->node()->path;
        } else if (!additive) {
            selectionRangeAnchorPath_.clear();
        }
    }

    void selectNodeItem(NodeItem* item, Qt::KeyboardModifiers modifiers)
    {
        const bool range = (modifiers & Qt::ShiftModifier) != 0;
        const bool additive = (modifiers & (Qt::ControlModifier | Qt::MetaModifier)) != 0;
        if (range && selectNodeRangeToItem(item, additive)) {
            return;
        }
        selectNodeItem(item, additive || range);
    }

    void restoreFolderSelection(const QString& folderPath)
    {
        selectNodePath(folderPath);
    }

    bool selectNodePath(const QString& path, bool ensureVisible = false)
    {
        NodeItem* selectedItem = selection_.selectByPath(path);
        selectionRangeAnchorPath_ = selectedItem && selectedItem->node() ? selectedItem->node()->path : QString();
        view_->setFocus(Qt::ShortcutFocusReason);
        if (ensureVisible) {
            ensureNodeItemVisible(selectedItem);
        }
        if (selectedItem && ensureVisible) {
            QTimer::singleShot(0, this, [this, path] { ensureNodePathVisible(path); });
        }
        return selectedItem != nullptr;
    }

    bool restoreSelection(const QStringList& paths, bool ensureVisible = false)
    {
        NodeItem* firstSelectedItem = selection_.selectByPaths(paths);
        selectionRangeAnchorPath_ = firstSelectedItem && firstSelectedItem->node()
                                        ? firstSelectedItem->node()->path
                                        : QString();
        view_->setFocus(Qt::ShortcutFocusReason);
        if (ensureVisible) {
            ensureNodeItemVisible(firstSelectedItem);
        }
        if (ensureVisible && firstSelectedItem && firstSelectedItem->node()) {
            const QString path = firstSelectedItem->node()->path;
            QTimer::singleShot(0, this, [this, path] { ensureNodePathVisible(path); });
        }
        return firstSelectedItem != nullptr;
    }

    void ensureNodeItemVisible(NodeItem* item)
    {
        if (!item || !view_) {
            return;
        }
        const QRectF rect = item->sceneBoundingRect().adjusted(-36.0, -36.0, 36.0, 36.0);
        view_->ensureVisible(rect, 80, 80);
        scheduleViewStateSave();
    }

    void ensureNodePathVisible(const QString& path)
    {
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node() && nodeItem->node()->path == path) {
                ensureNodeItemVisible(nodeItem);
                return;
            }
        }
    }

    bool moveSelectionWithTab(bool reverse)
    {
        if (!root_) {
            return false;
        }

        Node* current = singleSelectedNode();
        if (!current) {
            if (!root_->children.empty()) {
                return selectNodePath(reverse ? root_->children.back()->path : root_->children.front()->path, true);
            }
            view_->setFocus(Qt::ShortcutFocusReason);
            return true;
        }

        Node* parent = findVisibleNodeByPath(root_.get(), current->parentPath);
        if (!parent) {
            parent = root_.get();
        }

        for (size_t index = 0; index < parent->children.size(); ++index) {
            Node* sibling = parent->children[index].get();
            if (sibling != current) {
                continue;
            }
            if (reverse) {
                if (index > 0) {
                    return selectNodePath(parent->children[index - 1]->path, true);
                }
                if (parent != root_.get()) {
                    return selectNodePath(parent->path, true);
                }
                view_->setFocus(Qt::ShortcutFocusReason);
                return true;
            }
            if (index + 1 < parent->children.size()) {
                return selectNodePath(parent->children[index + 1]->path, true);
            }
            if (!current->children.empty()) {
                return selectNodePath(current->children.front()->path, true);
            }
            view_->setFocus(Qt::ShortcutFocusReason);
            return true;
        }

        view_->setFocus(Qt::ShortcutFocusReason);
        return true;
    }

    bool moveSelectionVertically(bool upward)
    {
        if (!root_) {
            return false;
        }

        Node* current = singleSelectedNode();
        if (!current) {
            return false;
        }

        Node* parent = findVisibleNodeByPath(root_.get(), current->parentPath);
        if (!parent) {
            parent = root_.get();
        }

        for (size_t index = 0; index < parent->children.size(); ++index) {
            if (parent->children[index].get() != current) {
                continue;
            }
            if (upward && index > 0) {
                return selectNodePath(parent->children[index - 1]->path, true);
            }
            if (!upward && index + 1 < parent->children.size()) {
                return selectNodePath(parent->children[index + 1]->path, true);
            }
            view_->setFocus(Qt::ShortcutFocusReason);
            return true;
        }

        view_->setFocus(Qt::ShortcutFocusReason);
        return true;
    }

    bool moveSelectionToParent()
    {
        Node* current = singleSelectedNode();
        if (!root_ || !current) {
            return false;
        }
        if (current == root_.get()) {
            view_->setFocus(Qt::ShortcutFocusReason);
            return root_ != nullptr;
        }

        Node* parent = findVisibleNodeByPath(root_.get(), current->parentPath);
        if (!parent) {
            view_->setFocus(Qt::ShortcutFocusReason);
            return true;
        }
        return selectNodePath(parent->path, true);
    }

    bool moveSelectionToFirstChild()
    {
        Node* current = singleSelectedNode();
        if (!root_ || !current) {
            return false;
        }
        if (!current->isDir) {
            for (const FileLink& link : fileLinks_) {
                if (link.from != current->path) {
                    continue;
                }
                if (findVisibleNodeByPath(root_.get(), link.to)) {
                    return selectNodePath(link.to, true);
                }
            }
            view_->setFocus(Qt::ShortcutFocusReason);
            return root_ != nullptr;
        }
        const QString folderPath = current->path;
        if (collapsedPaths_.contains(folderPath)) {
            collapsedPaths_.remove(folderPath);
            saveCollapsedFile();
            rebuild(false);
        }

        Node* folder = findVisibleNodeByPath(root_.get(), folderPath);
        if (folder && !folder->children.empty()) {
            return selectNodePath(folder->children.front()->path, true);
        }
        view_->setFocus(Qt::ShortcutFocusReason);
        return true;
    }

    Node* findVisibleNodeByPath(Node* node, const QString& path) const
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

    QStringList visibleNodePathsInOrder() const
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

    bool selectNodeRangeToItem(NodeItem* item, bool additive)
    {
        if (!item || !item->node()) {
            return false;
        }

        QString anchorPath = selectionRangeAnchorPath_;
        if (anchorPath.isEmpty()) {
            if (Node* selected = singleSelectedNode()) {
                anchorPath = selected->path;
            }
        }
        if (anchorPath.isEmpty()) {
            return false;
        }

        const QString targetPath = item->node()->path;
        const QStringList paths = visibleNodePathsInOrder();
        int anchorIndex = paths.indexOf(anchorPath);
        int targetIndex = paths.indexOf(targetPath);
        if (anchorIndex < 0 || targetIndex < 0) {
            return false;
        }
        if (anchorIndex > targetIndex) {
            std::swap(anchorIndex, targetIndex);
        }

        QStringList rangePaths;
        for (int index = anchorIndex; index <= targetIndex; ++index) {
            rangePaths.append(paths[index]);
        }
        selection_.selectByPaths(rangePaths, additive);
        selectionRangeAnchorPath_ = anchorPath;
        recordDebugEvent(QStringLiteral("selected range: %1 -> %2 (%3)")
                             .arg(relativeKeyForPath(anchorPath),
                                  relativeKeyForPath(targetPath))
                             .arg(rangePaths.size()));
        return true;
    }

    Node* singleSelectedNode() const
    {
        Node* selectedNode = nullptr;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem) {
                continue;
            }
            if (selectedNode) {
                return nullptr;
            }
            selectedNode = nodeItem->node();
        }
        return selectedNode;
    }

    void refreshNode(Node*)
    {
        cancelQueuedInlinePreviewToggle();
        rebuild(false);
    }

    void refreshSelectedItems()
    {
        cancelQueuedInlinePreviewToggle();
        rebuild(false);
    }

    // Loads order/color/preview/link metadata plus the hash cache for the current rootPath_, and
    // reconciles any renames that happened while this root was not open/watched (app not
    // running, or a different root was open) before refreshing the cache for the current state.
    // Order matters: reconciling first lets a vanished old path match a newly appeared path;
    // seeding the cache for the new path first would make it look already-known and hide it from
    // the "appeared" set. Callers still need to call loadCollapsedFile()/applyLargeTreeStartupCollapse()
    // and rebuild() themselves, since collapse handling interacts with each caller's own flow
    // (e.g. openRootFolder() always clears collapsedPaths_ first).
    void loadMetadataAndReconcileHashCache()
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

    // Placeholder shown between the (now instant) first paint and the first tree render; the
    // scene_.clear() in renderCurrentTree()/renderBoardScene() removes it.
    void showStartupLoadingIndicator()
    {
        QGraphicsSimpleTextItem* text = scene_.addSimpleText(QStringLiteral("読み込み中…"));
        QFont font = text->font();
        font.setPointSize(15);
        text->setFont(font);
        text->setBrush(palette().color(QPalette::WindowText));
        text->setPos(140.0, 120.0);
    }

    // Deferred startup, run shortly after the window is first shown. Splits what the
    // constructor used to do synchronously into (a) the bounded work needed to render the
    // visible tree, done here, and (b) the whole-tree walk + hash seeding, pushed to a
    // background thread (startStartupScanThread). The rename reconcile that used to precede
    // the first render now runs when that thread reports back (finishStartupScan), so a file
    // renamed externally while Mycel was closed keeps its metadata a moment later than before
    // instead of blocking the first paint on a full-tree content read.
    void completeDeferredStartup()
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

    void startStartupScanThread()
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
                    this, [this, result = std::move(result)] { finishStartupScan(result); },
                    Qt::QueuedConnection);
            });
        startupScanThread_->start();
    }

    // GUI-thread continuation of the startup scan: reconcile external renames against the
    // walked lists, merge the seeded hashes, and bring the filesystem watcher online with the
    // walked paths -- all without re-walking the tree on this thread.
    void finishStartupScan(const StartupScanResult& result)
    {
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

        startupScanPending_ = false;
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

    void refreshAll()
    {
        cancelQueuedInlinePreviewToggle();
        loadMetadataAndReconcileHashCache();
        if (!loadCollapsedFile()) {
            applyLargeTreeStartupCollapse();
        }
        rebuild(false);
    }

    bool isMetadataPath(const QString& path) const
    {
        const QString metadataRoot = QDir::cleanPath(QDir(rootPath_).filePath(QStringLiteral(".mycel")));
        const QString cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        return cleanPath == metadataRoot || cleanPath.startsWith(metadataRoot + QLatin1Char('/'));
    }

    // Decides whether the current root gets native file watching, and drops any watches left
    // over from a previous root when it does not. Called at construction and whenever
    // rootPath_ changes (open-folder navigation, archive import).
    void updateNativeWatcherModeForRoot()
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

    void resetFileSystemWatcher()
    {
        if (!fileSystemWatcher_ || inlineRenameActivitySuspended_ || nativeWatcherDisabled_) {
            return;
        }
        if (startupScanPending_) {
            // The startup scan thread is already producing the directory/file lists; walking
            // the tree again here (renders during startup call this) would block the GUI
            // thread on the exact traversal the deferred startup moved off it.
            return;
        }
        const TreeWalkResult walk = walkRealTree(rootPath_);
        applyFileSystemWatcherPaths(walk.directories, walk.files);
    }

    // Differential registration: only paths that actually changed are (un)watched. The full
    // remove-all/add-all this replaces measured ~550 ms per call at 10k files on a local SSD
    // -- and it ran on every rebuild.
    void applyFileSystemWatcherPaths(const QStringList& directories, const QStringList& files)
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

    // A watch dies silently when its path is deleted or renamed: Qt stops monitoring but keeps
    // the path listed, so the differential apply above sees it as "already watched" and never
    // re-arms it. The old full re-register revived such watches as a side effect; this does it
    // explicitly, for just the paths that fired in the current notification batch (covers
    // delete-then-recreate of the same name).
    void reviveWatchedPaths(const QSet<QString>& paths)
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

    void scheduleFileSystemRefresh(const QString& path)
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

    QString refreshDirectoryForChangedPath(const QString& path) const
    {
        QFileInfo info(path);
        if (info.exists() && info.isDir()) {
            return info.absoluteFilePath();
        }
        return info.absolutePath();
    }

    QString nearestVisibleDirectoryPath(QString path) const
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

    QStringList pendingRefreshDirectories() const
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

    // Raw changed directories for hash-cache/rename-reconciliation purposes: unlike
    // pendingRefreshDirectories(), this does not substitute collapsed directories with their
    // nearest visible ancestor, since reconciliation reads the real filesystem directly and does
    // not need a directory to be visible (or expanded) in the display tree.
    QStringList pendingChangedDirectoriesForHashSync() const
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

    enum class SubtreeRefresh { Changed, Unchanged, NotFound };

    // Detects files that vanished from one of `directories` and reappeared under a new path in
    // ANY of `directories` -- not necessarily the same one, which is what lets this track a file
    // moved to a different folder outside Mycel, not just renamed in place -- by matching content
    // hashes. This operates directly on the filesystem and the hash cache — never on the
    // displayed Node tree — because collapsed folders, the depth limit, and the per-folder child
    // cap all prune what the tree shows, which would make renames/moves inside them undetectable
    // if this relied on Node children instead.
    // The vanished file's hash comes from the persisted cache (the file itself is gone); the
    // appeared file's hash is computed fresh. A match is only trusted when it is unique on both
    // sides — duplicate content among candidates is left unresolved rather than guessed. On a
    // match, rekeys sidecar metadata and the hash cache from the old path to the new path, the
    // same treatment as an in-app rename/move, so external changes (e.g. by a cloud-sync client)
    // do not orphan color/preview/link/collapsed/order metadata.
    // When `pruneUnmatchedMetadata` is true, a vanished file whose hash matches no appeared file
    // anywhere in `directories` is treated as confirmed deleted and its metadata is purged --
    // only safe when `directories` covers the whole watched tree (background sweep/startup),
    // since a narrower scope could mistake "moved to a folder outside this batch" for "deleted".
    // Returns true if anything changed (rekeyed or removed), so the caller knows whether to
    // persist the affected sidecar files.
    bool reconcileRenamedFiles(const QStringList& directories, bool pruneUnmatchedMetadata)
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

    // Core reconcile. `currentPaths` is the current file listing of `directories` -- either
    // gathered by the wrapper above or reused from the startup scan thread's walk. When
    // `precomputedHashes` is given (startup path), appeared files take their content hash from
    // it instead of being read on this (GUI) thread; the scan hashes every uncached file, so
    // an appeared file missing from it is exactly one computeFileContentHash() would also skip.
    bool reconcileRenamedFiles(const QStringList& directories, const QSet<QString>& currentPaths,
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

    void persistRenameReconciliationResults()
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

    // Periodic safety net: sweeps every real directory for external renames, independent of
    // QFileSystemWatcher notifications. Some cloud-sync clients and network drives do not fire
    // reliable per-directory change events, so relying solely on the watcher can leave a rename
    // unreconciled indefinitely; this catches those on the next tick instead. Skipped while an
    // in-app drag, inline rename, or side-editor session is active, matching the same guards the
    // live watcher-driven refresh uses, so the sweep never fights an in-progress user action.
    // Despite the name, this used to walk and hash the whole tree synchronously on the GUI
    // thread every BackgroundReconcileIntervalMs, freezing the UI on large or network-mounted
    // roots. The walk + hashing now run on a worker thread via runStartupScan (the same pure
    // function the startup scan uses); finishBackgroundReconcile() applies the result on the GUI
    // thread once it comes back.
    void runBackgroundReconcile()
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
                    this, [this, result = std::move(result)] { finishBackgroundReconcile(result); },
                    Qt::QueuedConnection);
            });
        reconcileScanThread_->start();
    }

    // GUI-thread continuation of the periodic reconcile sweep -- mirrors finishStartupScan(),
    // just triggered by the timer instead of the initial load.
    void finishBackgroundReconcile(const StartupScanResult& result)
    {
        reconcileScanPending_ = false;

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

    // Rescan one changed directory and replace its subtree only if the displayed structure
    // actually differs. Returns Unchanged (and keeps the existing Node objects, so live
    // NodeItems stay valid) when a notification did not alter what is shown.
    SubtreeRefresh replaceScannedSubtree(const QString& directoryPath)
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

    void refreshFromFileSystemChange()
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

    void exportArchive()
    {
        if (!saveSideEditorNow()) {
            return;
        }

        const QFileInfo rootInfo(rootPath_);
        const QString defaultName = QStringLiteral("%1.mycel.md").arg(rootInfo.fileName().isEmpty()
                                                                          ? QStringLiteral("Mycel")
                                                                          : rootInfo.fileName());
        const QString defaultPath = QDir(rootInfo.absolutePath()).filePath(defaultName);
        const QString archivePath = QFileDialog::getSaveFileName(
            this, QStringLiteral("Mycel アーカイブを書き出し"), defaultPath,
            QStringLiteral("Mycel Archive (*.mycel.md);;Markdown (*.md);;All Files (*)"));
        if (archivePath.isEmpty()) {
            return;
        }

        QFileInfo archiveInfo(archivePath);
        QDir archiveDir(archiveInfo.absolutePath());
        QString archiveStem = archiveInfo.completeBaseName();
        if (archiveStem.endsWith(QStringLiteral(".mycel"), Qt::CaseInsensitive)) {
            archiveStem.chop(QStringLiteral(".mycel").size());
        }
        if (archiveStem.isEmpty()) {
            archiveStem = QStringLiteral("Mycel");
        }

        const QString assetsDirName = uniqueAssetsDirectoryName(archiveDir, archiveStem + QStringLiteral(".assets"));
        const QString assetsDirPath = archiveDir.filePath(assetsDirName);

        QString output;
        output += QStringLiteral("---\n");
        output += QStringLiteral("mycel_archive: 1\n");
        output += QStringLiteral("root: \"%1\"\n").arg(rootInfo.fileName());
        output += QStringLiteral("created_with: \"%1\"\n").arg(appDisplayName());
        output += QStringLiteral("text_encoding: \"utf-8\"\n");
        output += QStringLiteral("binary_assets: \"%1\"\n").arg(assetsDirName);
        output += QStringLiteral("---\n\n");

        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("name"), rootInfo.fileName());
        rootObject.insert(QStringLiteral("version"), 1);
        output += archiveComment(QStringLiteral("root"), rootObject);
        output += QStringLiteral("\n");

        QStringList failures;
        int binaryCount = 0;
        appendDirectoryToArchive(rootPath_, 0, -1, output, assetsDirPath, assetsDirName, failures, binaryCount);

        output += archiveLinksSection();

        if (!writeBytes(archivePath, output.toUtf8())) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("アーカイブを書き出せませんでした。"));
            return;
        }

        QString message = QStringLiteral("アーカイブを書き出しました。\n%1").arg(archivePath);
        if (binaryCount > 0) {
            message += QStringLiteral("\nバイナリファイルは次のフォルダへコピーしました。\n%1").arg(assetsDirPath);
        }
        if (!failures.isEmpty()) {
            message += QStringLiteral("\n\n処理できなかった項目があります。\n%1").arg(failures.join(QLatin1Char('\n')));
        }
        QMessageBox::information(this, QStringLiteral("Mycel"), message);
    }

    void importArchive()
    {
        if (!saveSideEditorNow()) {
            return;
        }

        const QString archivePath = QFileDialog::getOpenFileName(
            this, QStringLiteral("Mycel アーカイブを読み込み"), rootPath_,
            QStringLiteral("Mycel Archive (*.mycel.md *.md);;All Files (*)"));
        if (archivePath.isEmpty()) {
            return;
        }

        const QString destinationPath = QFileDialog::getExistingDirectory(
            this, QStringLiteral("インポート先ルートフォルダ"), rootPath_);
        if (destinationPath.isEmpty()) {
            return;
        }

        const QString destinationRoot = normalizedDirectoryPath(destinationPath);
        QDir destinationDir(destinationRoot);
        const QFileInfoList existing = destinationDir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        if (!existing.isEmpty()) {
            const int answer = QMessageBox::question(
                this, QStringLiteral("Mycel"),
                QStringLiteral("インポート先フォルダに既存の項目があります。同名ファイルは上書きせずスキップします。\n続行しますか？"),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer != QMessageBox::Yes) {
                return;
            }
        }

        QFile archiveFile(archivePath);
        if (!archiveFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("アーカイブを開けませんでした。"));
            return;
        }

        const QString archiveText = QString::fromUtf8(archiveFile.readAll());
        const QStringList lines = archiveText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        const QDir archiveDir(QFileInfo(archivePath).absolutePath());

        QSet<QString> importedCollapsedPaths;
        std::map<QString, QColor> importedColors;
        QSet<QString> importedPreviewPaths;
        std::map<QString, QSizeF> importedPreviewSizes;
        std::map<QString, std::vector<ArchiveOrderEntry>> importedOrderEntries;
        std::vector<FileLink> importedLinks;
        QStringList failures;
        int importedCount = 0;

        for (int i = 0; i < lines.size(); ++i) {
            if (const auto folder = archiveCommentObject(lines[i], QStringLiteral("folder"))) {
                importArchiveFolder(*folder, destinationRoot, importedCollapsedPaths, importedColors,
                                    importedOrderEntries, failures, importedCount);
                continue;
            }

            if (const auto file = archiveCommentObject(lines[i], QStringLiteral("file"))) {
                importArchiveFile(*file, lines, i, archiveDir, destinationRoot, importedColors,
                                  importedPreviewPaths, importedPreviewSizes, importedOrderEntries,
                                  failures, importedCount);
                continue;
            }

            if (lines[i].trimmed() == QStringLiteral("<!-- mycel:links -->")) {
                importArchiveLinks(lines, i, destinationRoot, importedLinks, failures);
            }
        }

        rootPath_ = destinationRoot;
        rememberRootPath(rootPath_);
        updateNativeWatcherModeForRoot();
        collapsedPaths_ = std::move(importedCollapsedPaths);
        userColors_ = std::move(importedColors);
        previewPaths_ = std::move(importedPreviewPaths);
        previewSizes_ = std::move(importedPreviewSizes);
        previewImageScales_.clear();
        fileLinks_ = std::move(importedLinks);
        fileOrders_.clear();
        for (auto& [key, entries] : importedOrderEntries) {
            std::sort(entries.begin(), entries.end(), [](const ArchiveOrderEntry& a, const ArchiveOrderEntry& b) {
                return a.order < b.order;
            });
            QStringList names;
            for (const ArchiveOrderEntry& entry : entries) {
                if (!entry.name.isEmpty() && !names.contains(entry.name)) {
                    names.append(entry.name);
                }
            }
            fileOrders_[key] = names;
        }

        saveColorFile();
        saveOrderFile();
        savePreviewFile();
        saveLinkFile();
        saveCollapsedFile();
        rebuild(true);
        QTimer::singleShot(0, this, [this] { syncEditorPaneVisibility(); });

        QString message = QStringLiteral("%1 件をインポートしました。\n%2").arg(importedCount).arg(destinationRoot);
        if (!mycelStorageEnabled_) {
            message += QStringLiteral("\n\n--no-mycel モードのため、メタデータはこのセッション中のみ反映されます。");
        }
        if (!failures.isEmpty()) {
            message += QStringLiteral("\n\n処理できなかった項目があります。\n%1").arg(failures.join(QLatin1Char('\n')));
        }
        QMessageBox::information(this, QStringLiteral("Mycel"), message);
    }

    // Detecting "is this root large" requires a full recursive walk (isLargeRootTree). Persist
    // the resulting collapse set immediately so that walk runs at most once per root: after this,
    // loadCollapsedFile() finds collapsed.json and every later launch/root-switch/refresh skips
    // the walk entirely. A tree found to be small is deliberately left unsaved -- writing an
    // empty collapsed.json here would make loadCollapsedFile() report "already decided" forever,
    // so a root that grows past the threshold later would never get auto-collapsed.
    void applyLargeTreeStartupCollapse()
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

    bool isLargeRootTree() const
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

    bool eventFilter(QObject* watched, QEvent* event) override
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
                  keyEvent->key() == Qt::Key_Enter ||
                  isDeleteShortcut(keyEvent->key(), modifiers))) ||
                (modifiers == Qt::ShiftModifier &&
                 (keyEvent->key() == Qt::Key_N || keyEvent->key() == Qt::Key_Tab)) ||
                (modifiers == Qt::ControlModifier &&
                 (keyEvent->key() == Qt::Key_C || keyEvent->key() == Qt::Key_V)) ||
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

    QString cachedYouTubeThumbnailPathForFile(const QString& path) const
    {
        const auto embedUrl = youtubeEmbedUrlForFile(QFileInfo(path));
        if (!embedUrl) {
            return {};
        }
        return youtubeThumbnailCachePathForEmbedUrl(*embedUrl);
    }

    QString cachedUrlThumbnailPathForFile(const QString& path) const
    {
        const auto url = urlForShortcutFile(QFileInfo(path));
        if (!url) {
            return {};
        }
        return urlThumbnailCachePathForUrl(*url);
    }

protected:
    void closeEvent(QCloseEvent* event) override
    {
        if (!saveSideEditorNow()) {
            event->ignore();
            return;
        }
        saveViewState();
        saveCollapsedFile();
        QMainWindow::closeEvent(event);
    }

private:
    bool isInlinePreviewObject(QObject* watched) const
    {
        for (QObject* object = watched; object; object = object->parent()) {
            if (object->property("mycelInlinePreview").toBool()) {
                return true;
            }
        }
        return false;
    }

    QString inlinePreviewPathForObject(QObject* watched) const
    {
        for (QObject* object = watched; object; object = object->parent()) {
            const QVariant path = object->property("mycelInlinePreviewPath");
            if (path.isValid()) {
                return path.toString();
            }
        }
        return {};
    }

    bool isSidePreviewObject(QObject* watched) const
    {
        if (!sidePreviewStack_ || sideEditorEditing_ || sidePreviewStack_->currentWidget() == sideEditor_) {
            return false;
        }

        QWidget* currentPreview = sidePreviewStack_->currentWidget();
        for (QObject* object = watched; object; object = object->parent()) {
            if (object == currentPreview || object == sidePreviewStack_) {
                return true;
            }
            if (sidePreviewText_ && (object == sidePreviewText_ || object == sidePreviewText_->viewport())) {
                return true;
            }
            if (object == sidePreviewImage_ || object == sidePreviewVideo_) {
                return true;
            }
#if MYCEL_HAS_WEBENGINE
            if (object == sideHtmlWeb_) {
                return true;
            }
#endif
        }
        return false;
    }

public:
    void recordDebugEvent(const QString& message)
    {
        const QString line = QStringLiteral("%1  %2")
                                 .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")), message);
        debugEvents_.append(line);
        const QString logPath = qEnvironmentVariable("MYCEL_DEBUG_LOG");
        if (!logPath.isEmpty()) {
            QDir().mkpath(QFileInfo(logPath).absolutePath());
            QFile logFile(logPath);
            if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                QTextStream stream(&logFile);
                stream << line << Qt::endl;
            }
        }
        while (debugEvents_.size() > 80) {
            debugEvents_.removeFirst();
        }
        if (debugText_ && debugDock_ && debugDock_->isVisible()) {
            refreshDebugPane();
        }
    }

private:
    void copyDebugPaneToClipboard()
    {
        if (!debugText_) {
            return;
        }
        refreshDebugPane(QStringLiteral("debug copied"));
        QApplication::clipboard()->setText(debugText_->toPlainText());
    }

    void refreshDebugPane(const QString& eventMessage = QString())
    {
        if (!debugText_ || !debugDock_ || !debugDock_->isVisible()) {
            return;
        }
        if (!eventMessage.isEmpty()) {
            recordDebugEvent(eventMessage);
            return;
        }

        QWidget* focusWidget = QApplication::focusWidget();
        Node* selectedNode = singleSelectedNode();
        const QList<QGraphicsItem*> selectedItems = scene_.selectedItems();

        QStringList lines;
        lines << QStringLiteral("Mycel Debug");
        lines << QStringLiteral("----------");
        lines << QStringLiteral("focusWidget: %1").arg(debugObjectName(focusWidget));
        lines << QStringLiteral("focusWidgetIsInlinePreview: %1").arg(isInlinePreviewObject(focusWidget) ? QStringLiteral("true") : QStringLiteral("false"));
        lines << QStringLiteral("textInputWidgetHasFocus: %1").arg(textInputWidgetHasFocus() ? QStringLiteral("true") : QStringLiteral("false"));
        lines << QStringLiteral("viewHasFocus: %1").arg(view_ && view_->hasFocus() ? QStringLiteral("true") : QStringLiteral("false"));
        lines << QStringLiteral("sceneSelectedItems: %1").arg(selectedItems.size());
        lines << QStringLiteral("singleSelectedNode: %1").arg(selectedNode ? relativeKeyForPath(selectedNode->path) : QStringLiteral("(none)"));
        lines << QStringLiteral("singleSelectedIsDir: %1").arg(selectedNode ? (selectedNode->isDir ? QStringLiteral("true") : QStringLiteral("false")) : QStringLiteral("(none)"));
        lines << QStringLiteral("sideEditorEditing: %1").arg(sideEditorEditing_ ? QStringLiteral("true") : QStringLiteral("false"));
        lines << QStringLiteral("sideEditorPath: %1").arg(sideEditorPath_.isEmpty() ? QStringLiteral("(none)") : relativeKeyForPath(sideEditorPath_));
        lines << QStringLiteral("");
        lines << QStringLiteral("Selected Paths");
        lines << QStringLiteral("--------------");
        const QStringList selectedPaths = selectedNodePaths();
        if (selectedPaths.isEmpty()) {
            lines << QStringLiteral("(none)");
        } else {
            for (const QString& path : selectedPaths) {
                lines << relativeKeyForPath(path);
            }
        }
        lines << QStringLiteral("");
        lines << QStringLiteral("Recent Events");
        lines << QStringLiteral("-------------");
        if (debugEvents_.isEmpty()) {
            lines << QStringLiteral("(none)");
        } else {
            for (auto it = debugEvents_.crbegin(); it != debugEvents_.crend(); ++it) {
                lines << *it;
            }
        }
        debugText_->setPlainText(lines.join(QLatin1Char('\n')));
        debugText_->moveCursor(QTextCursor::Start);
    }

    QString debugObjectName(QObject* object) const
    {
        if (!object) {
            return QStringLiteral("(none)");
        }
        QString text = QString::fromLatin1(object->metaObject()->className());
        if (!object->objectName().isEmpty()) {
            text += QStringLiteral(" objectName=%1").arg(object->objectName());
        }
        return text;
    }

    QString debugKeyName(QKeyEvent* event) const
    {
        if (!event) {
            return QStringLiteral("(none)");
        }
        QString key = QKeySequence(event->key()).toString(QKeySequence::NativeText);
        if (key.isEmpty()) {
            key = QStringLiteral("key=%1").arg(event->key());
        }
        QStringList modifiers;
        const Qt::KeyboardModifiers mods = event->modifiers();
        if (mods & Qt::ControlModifier) {
            modifiers << QStringLiteral("Ctrl");
        }
        if (mods & Qt::ShiftModifier) {
            modifiers << QStringLiteral("Shift");
        }
        if (mods & Qt::AltModifier) {
            modifiers << QStringLiteral("Alt");
        }
        if (mods & Qt::MetaModifier) {
            modifiers << QStringLiteral("Meta");
        }
        if (mods & Qt::KeypadModifier) {
            modifiers << QStringLiteral("Keypad");
        }
        return modifiers.isEmpty() ? key : modifiers.join(QLatin1Char('+')) + QLatin1Char('+') + key;
    }

    void finishInlineRename(bool commit)
    {
        if (finishingRename_ || !renameProxy_ || !renameEdit_) {
            return;
        }

        finishingRename_ = true;
        const QString path = renamingPath_;
        const QString name = renameEdit_->text().trimmed();
        renameEdit_->removeEventFilter(this);
        scene_.removeItem(renameProxy_);
        delete renameProxy_;
        renameProxy_ = nullptr;
        renameEdit_ = nullptr;
        renamingPath_.clear();
        finishingRename_ = false;

        if (commit) {
            const QString destination = QFileInfo(path).dir().filePath(name);
            if (renamePathTo(path, name)) {
                selectNodePath(destination);
            } else {
                selectNodePath(path);
            }
        }
        resumeInlineRenameActivity();
    }

    void suspendInlineRenameActivity()
    {
        if (inlineRenameActivitySuspended_) {
            return;
        }

        inlineRenameActivitySuspended_ = true;
        previewClickTimer_.stop();
        queuedPreviewPath_.clear();
        queuedCollapsePath_.clear();
        fileSystemRefreshTimer_.stop();
        pendingFileSystemPaths_.clear();
        viewStateSaveTimer_.stop();
        sideEditorSaveTimer_.stop();

        pausedWatcherFiles_.clear();
        pausedWatcherDirectories_.clear();
        if (!fileSystemWatcher_) {
            return;
        }

        pausedWatcherFiles_ = fileSystemWatcher_->files();
        pausedWatcherDirectories_ = fileSystemWatcher_->directories();
        if (!pausedWatcherFiles_.isEmpty()) {
            fileSystemWatcher_->removePaths(pausedWatcherFiles_);
        }
        if (!pausedWatcherDirectories_.isEmpty()) {
            fileSystemWatcher_->removePaths(pausedWatcherDirectories_);
        }
        recordDebugEvent(QStringLiteral("inline rename suspended timers and watcher"));
    }

    void resumeInlineRenameActivity()
    {
        if (!inlineRenameActivitySuspended_) {
            return;
        }

        inlineRenameActivitySuspended_ = false;
        if (fileSystemWatcher_) {
            if (!pausedWatcherDirectories_.isEmpty()) {
                fileSystemWatcher_->addPaths(pausedWatcherDirectories_);
            }
            if (!pausedWatcherFiles_.isEmpty()) {
                fileSystemWatcher_->addPaths(pausedWatcherFiles_);
            }
        }
        pausedWatcherFiles_.clear();
        pausedWatcherDirectories_.clear();
        resetFileSystemWatcher();
        recordDebugEvent(QStringLiteral("inline rename resumed timers and watcher"));
    }

    void setVisualPosition(NodeItem* item, const QPointF& position)
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

    QString uniqueAssetsDirectoryName(const QDir& parentDir, const QString& preferredName) const
    {
        QString candidate = preferredName;
        for (int number = 2; parentDir.exists(candidate); ++number) {
            candidate = QStringLiteral("%1 %2").arg(preferredName, QString::number(number));
        }
        return candidate;
    }

    QFileInfoList archiveEntriesForDirectory(const QString& directoryPath) const
    {
        QDir dir(directoryPath);
        QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                  QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
        entries.erase(std::remove_if(entries.begin(), entries.end(), [](const QFileInfo& entry) {
            return entry.fileName() == QStringLiteral(".mycel");
        }), entries.end());

        const auto order = fileOrders_.find(orderKeyForDirectory(directoryPath));
        if (order == fileOrders_.end()) {
            return entries;
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
        return entries;
    }

    void addCommonArchiveMetadata(QJsonObject& object, const QString& path, int order) const
    {
        object.insert(QStringLiteral("path"), archiveRelativePath(relativeKeyForPath(path)));
        object.insert(QStringLiteral("order"), order);

        const auto color = userColors_.find(path);
        if (color != userColors_.end() && color->second.isValid()) {
            object.insert(QStringLiteral("color"), color->second.name(QColor::HexRgb));
        }
    }

    void appendDirectoryToArchive(const QString& directoryPath, int depth, int order, QString& output,
                                  const QString& assetsDirPath, const QString& assetsDirName,
                                  QStringList& failures, int& binaryCount)
    {
        const QFileInfo directoryInfo(directoryPath);
        if (depth > 0) {
            QJsonObject object;
            addCommonArchiveMetadata(object, directoryInfo.absoluteFilePath(), order);
            object.insert(QStringLiteral("collapsed"), collapsedPaths_.contains(directoryInfo.absoluteFilePath()));
            output += archiveComment(QStringLiteral("folder"), object);
            output += markdownHeading(depth + 1, archiveRelativePath(relativeKeyForPath(directoryInfo.absoluteFilePath())));
        }

        const QFileInfoList entries = archiveEntriesForDirectory(directoryInfo.absoluteFilePath());
        for (int i = 0; i < entries.size(); ++i) {
            const QFileInfo& entry = entries[i];
            if (entry.isDir()) {
                appendDirectoryToArchive(entry.absoluteFilePath(), depth + 1, i, output,
                                         assetsDirPath, assetsDirName, failures, binaryCount);
            } else {
                appendFileToArchive(entry, depth + 1, i, output, assetsDirPath, assetsDirName, failures, binaryCount);
            }
        }
    }

    void appendFileToArchive(const QFileInfo& fileInfo, int depth, int order, QString& output,
                             const QString& assetsDirPath, const QString& assetsDirName,
                             QStringList& failures, int& binaryCount)
    {
        QJsonObject object;
        addCommonArchiveMetadata(object, fileInfo.absoluteFilePath(), order);

        const bool previewOpen = previewPaths_.contains(fileInfo.absoluteFilePath());
        object.insert(QStringLiteral("preview"), previewOpen);
        const auto previewSize = previewSizes_.find(fileInfo.absoluteFilePath());
        if (previewSize != previewSizes_.end()) {
            object.insert(QStringLiteral("width"), previewSize->second.width());
            object.insert(QStringLiteral("height"), previewSize->second.height());
        }
        const auto previewScale = previewImageScales_.find(fileInfo.absoluteFilePath());
        if (previewScale != previewImageScales_.end()) {
            object.insert(QStringLiteral("scale"), previewScale->second);
        }

        output += QString();
        if (isTextPreviewFile(fileInfo)) {
            object.insert(QStringLiteral("type"), QStringLiteral("text"));
            QFile file(fileInfo.absoluteFilePath());
            QString text;
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                text = QString::fromUtf8(file.readAll());
            } else {
                object.insert(QStringLiteral("export_error"), QStringLiteral("read_failed"));
                failures.append(fileInfo.absoluteFilePath());
            }

            output += archiveComment(QStringLiteral("file"), object);
            output += markdownHeading(depth + 1, archiveRelativePath(relativeKeyForPath(fileInfo.absoluteFilePath())));
            output += QStringLiteral("```mycel-text\n");
            output += escapedArchiveText(text);
            output += QStringLiteral("\n```\n\n");
            return;
        }

        object.insert(QStringLiteral("type"), QStringLiteral("binary"));
        object.insert(QStringLiteral("size"), static_cast<double>(fileInfo.size()));
        const QString relativePath = archiveRelativePath(relativeKeyForPath(fileInfo.absoluteFilePath()));
        const QString assetRelativePath = archiveRelativePath(QDir(assetsDirName).filePath(relativePath));
        const QString assetDestination = QDir(assetsDirPath).filePath(relativePath);
        object.insert(QStringLiteral("binary"), assetRelativePath);

        QDir assetParent(QFileInfo(assetDestination).absolutePath());
        bool copied = assetParent.mkpath(QStringLiteral(".")) && QFile::copy(fileInfo.absoluteFilePath(), assetDestination);
        if (!copied) {
            object.insert(QStringLiteral("missing"), true);
            failures.append(fileInfo.absoluteFilePath());
        } else {
            ++binaryCount;
        }

        output += archiveComment(QStringLiteral("file"), object);
        output += markdownHeading(depth + 1, relativePath);
    }

    QString archiveLinksSection() const
    {
        QJsonArray links;
        QSet<QString> seen;
        for (const FileLink& link : fileLinks_) {
            if (link.from.isEmpty() || link.to.isEmpty() || link.from == link.to ||
                !QFileInfo::exists(link.from) || !QFileInfo::exists(link.to)) {
                continue;
            }
            const QString from = archiveRelativePath(relativeKeyForPath(link.from));
            const QString to = archiveRelativePath(relativeKeyForPath(link.to));
            if (!isSafeArchiveRelativePath(from) || !isSafeArchiveRelativePath(to)) {
                continue;
            }
            const QString key = from + QLatin1Char('\n') + to;
            if (seen.contains(key)) {
                continue;
            }
            seen.insert(key);

            QJsonObject object;
            object.insert(QStringLiteral("from"), from);
            object.insert(QStringLiteral("to"), to);
            links.append(object);
        }

        if (links.isEmpty()) {
            return {};
        }

        return QStringLiteral("<!-- mycel:links -->\n```json\n%1\n```\n")
            .arg(QString::fromUtf8(QJsonDocument(links).toJson(QJsonDocument::Indented)).trimmed());
    }

    void addImportedOrderEntry(const QString& destinationRoot, const QString& path, int order,
                               std::map<QString, std::vector<ArchiveOrderEntry>>& orderEntries) const
    {
        const QFileInfo info(path);
        const QString parentRelative = QDir(destinationRoot).relativeFilePath(info.absolutePath());
        const QString key = parentRelative == QStringLiteral(".") ? QString() : archiveRelativePath(parentRelative);
        orderEntries[key].push_back({order, info.fileName()});
    }

    void addImportedCommonMetadata(const QJsonObject& object, const QString& path, bool isDir,
                                   QSet<QString>& collapsedPaths, std::map<QString, QColor>& colors,
                                   QSet<QString>& previewPaths, std::map<QString, QSizeF>& previewSizes) const
    {
        const QColor color(object.value(QStringLiteral("color")).toString());
        if (color.isValid()) {
            colors[path] = color;
        }

        if (isDir) {
            if (object.value(QStringLiteral("collapsed")).toBool(false)) {
                collapsedPaths.insert(path);
            }
            return;
        }

        if (object.value(QStringLiteral("preview")).toBool(false)) {
            previewPaths.insert(path);
        }
        const qreal width = object.value(QStringLiteral("width")).toDouble(0.0);
        const qreal height = object.value(QStringLiteral("height")).toDouble(0.0);
        if (width > 0.0 && height > 0.0) {
            previewSizes[path] = clampedPreviewSizeForFile(QFileInfo(path), QSizeF(width, height));
        }
    }

    bool archivePathForObject(const QJsonObject& object, QString& relativePath, QStringList& failures) const
    {
        relativePath = archiveRelativePath(object.value(QStringLiteral("path")).toString());
        if (!isSafeArchiveRelativePath(relativePath)) {
            failures.append(QStringLiteral("不正なパス: %1").arg(relativePath));
            return false;
        }
        return true;
    }

    void importArchiveFolder(const QJsonObject& object, const QString& destinationRoot,
                             QSet<QString>& collapsedPaths, std::map<QString, QColor>& colors,
                             std::map<QString, std::vector<ArchiveOrderEntry>>& orderEntries,
                             QStringList& failures, int& importedCount)
    {
        QString relativePath;
        if (!archivePathForObject(object, relativePath, failures)) {
            return;
        }

        const QString destinationPath = QDir(destinationRoot).filePath(relativePath);
        if (QFileInfo(destinationPath).exists() && !QFileInfo(destinationPath).isDir()) {
            failures.append(destinationPath);
            return;
        }
        if (!QDir().mkpath(destinationPath)) {
            failures.append(destinationPath);
            return;
        }

        QSet<QString> dummyPreviewPaths;
        std::map<QString, QSizeF> dummyPreviewSizes;
        addImportedCommonMetadata(object, QFileInfo(destinationPath).absoluteFilePath(), true,
                                  collapsedPaths, colors, dummyPreviewPaths, dummyPreviewSizes);
        addImportedOrderEntry(destinationRoot, QFileInfo(destinationPath).absoluteFilePath(),
                              object.value(QStringLiteral("order")).toInt(0), orderEntries);
        ++importedCount;
    }

    bool readArchiveTextBlock(const QStringList& lines, int& index, QString& text, QStringList& failures) const
    {
        int cursor = index + 1;
        while (cursor < lines.size() && lines[cursor].trimmed() != QStringLiteral("```mycel-text")) {
            ++cursor;
        }
        if (cursor >= lines.size()) {
            failures.append(QStringLiteral("本文ブロックが見つかりません。"));
            return false;
        }

        ++cursor;
        QStringList body;
        while (cursor < lines.size() && lines[cursor].trimmed() != QStringLiteral("```")) {
            body.append(lines[cursor]);
            ++cursor;
        }
        if (cursor >= lines.size()) {
            failures.append(QStringLiteral("本文ブロックが閉じられていません。"));
            return false;
        }

        text = unescapedArchiveText(body);
        index = cursor;
        return true;
    }

    void importArchiveFile(const QJsonObject& object, const QStringList& lines, int& index, const QDir& archiveDir,
                           const QString& destinationRoot, std::map<QString, QColor>& colors,
                           QSet<QString>& previewPaths, std::map<QString, QSizeF>& previewSizes,
                           std::map<QString, std::vector<ArchiveOrderEntry>>& orderEntries,
                           QStringList& failures, int& importedCount)
    {
        QString relativePath;
        if (!archivePathForObject(object, relativePath, failures)) {
            return;
        }

        const QString destinationPath = QDir(destinationRoot).filePath(relativePath);
        if (QFileInfo(destinationPath).exists()) {
            failures.append(QStringLiteral("既に存在するためスキップ: %1").arg(destinationPath));
            if (object.value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
                QString ignored;
                readArchiveTextBlock(lines, index, ignored, failures);
            }
            return;
        }

        QDir parent(QFileInfo(destinationPath).absolutePath());
        if (!parent.mkpath(QStringLiteral("."))) {
            failures.append(destinationPath);
            return;
        }

        bool imported = false;
        if (object.value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
            QString text;
            if (!readArchiveTextBlock(lines, index, text, failures)) {
                return;
            }
            imported = writeBytes(destinationPath, text.toUtf8());
        } else {
            const QString binaryPath = archiveRelativePath(object.value(QStringLiteral("binary")).toString());
            if (!isSafeArchiveRelativePath(binaryPath)) {
                failures.append(QStringLiteral("不正なバイナリ参照: %1").arg(binaryPath));
                return;
            }
            const QString sourcePath = archiveDir.filePath(binaryPath);
            imported = QFile::copy(sourcePath, destinationPath);
        }

        if (!imported) {
            failures.append(destinationPath);
            return;
        }

        QSet<QString> dummyCollapsedPaths;
        addImportedCommonMetadata(object, QFileInfo(destinationPath).absoluteFilePath(), false,
                                  dummyCollapsedPaths, colors, previewPaths, previewSizes);
        addImportedOrderEntry(destinationRoot, QFileInfo(destinationPath).absoluteFilePath(),
                              object.value(QStringLiteral("order")).toInt(0), orderEntries);
        ++importedCount;
    }

    void importArchiveLinks(const QStringList& lines, int& index, const QString& destinationRoot,
                            std::vector<FileLink>& links, QStringList& failures) const
    {
        int cursor = index + 1;
        while (cursor < lines.size() && lines[cursor].trimmed() != QStringLiteral("```json")) {
            ++cursor;
        }
        if (cursor >= lines.size()) {
            return;
        }

        ++cursor;
        QStringList body;
        while (cursor < lines.size() && lines[cursor].trimmed() != QStringLiteral("```")) {
            body.append(lines[cursor]);
            ++cursor;
        }
        if (cursor >= lines.size()) {
            failures.append(QStringLiteral("関連情報の JSON ブロックが閉じられていません。"));
            return;
        }
        index = cursor;

        const QJsonDocument doc = QJsonDocument::fromJson(body.join(QLatin1Char('\n')).toUtf8());
        if (!doc.isArray()) {
            failures.append(QStringLiteral("関連情報の JSON を読み込めませんでした。"));
            return;
        }

        QSet<QString> seen;
        for (const QJsonValue& value : doc.array()) {
            const QJsonObject object = value.toObject();
            const QString fromRelative = archiveRelativePath(object.value(QStringLiteral("from")).toString());
            const QString toRelative = archiveRelativePath(object.value(QStringLiteral("to")).toString());
            if (!isSafeArchiveRelativePath(fromRelative) || !isSafeArchiveRelativePath(toRelative)) {
                continue;
            }

            const QString from = QFileInfo(QDir(destinationRoot).filePath(fromRelative)).absoluteFilePath();
            const QString to = QFileInfo(QDir(destinationRoot).filePath(toRelative)).absoluteFilePath();
            if (from == to || !QFileInfo::exists(from) || !QFileInfo::exists(to)) {
                continue;
            }

            const QString key = from + QLatin1Char('\n') + to;
            if (seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            links.push_back({from, to});
        }
    }

    void removeDeletedPathMetadata(const QString& deletedPath, bool wasDir)
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

    // Register a freshly created item in the directory's custom order. When `afterName` names an
    // existing sibling, the new item is placed directly after it; otherwise it is appended.
    void appendCreatedItemToOrder(const QString& directoryPath, const QString& newName, bool isDir,
                                  const QString& afterName = QString())
    {
        if (!mycelStorageEnabled_) {
            return;
        }

        // Hash the new file right away rather than waiting for a scan to notice it: a file
        // renamed externally before the next scan/sweep would otherwise have no "before" hash
        // to reconcile against.
        if (!isDir) {
            ensureHashCachedForRenameTracking(QDir(directoryPath).filePath(newName));
        }

        const QString key = orderKeyForDirectory(directoryPath);
        const QStringList previousOrder = fileOrders_[key];
        const QFileInfoList entries = QDir(directoryPath).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                                        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

        QStringList existingNames;
        for (const QFileInfo& entry : entries) {
            if (entry.fileName() == QStringLiteral(".mycel") || entry.fileName() == newName) {
                continue;
            }
            existingNames.append(entry.fileName());
        }

        QStringList order;
        for (const QString& name : previousOrder) {
            if (existingNames.contains(name) && !order.contains(name)) {
                order.append(name);
            }
        }
        for (const QString& name : existingNames) {
            if (!order.contains(name)) {
                order.append(name);
            }
        }
        int insertIndex = order.size();
        if (!afterName.isEmpty()) {
            const int afterIndex = order.indexOf(afterName);
            if (afterIndex >= 0) {
                insertIndex = afterIndex + 1;
            }
        }
        order.insert(insertIndex, newName);
        fileOrders_[key] = order;
    }

    QString availableImportPath(const QString& targetDirectoryPath, const QString& preferredName, bool isDirectory,
                                QSet<QString>* reservedNames = nullptr) const
    {
        return FileOperationService::availableDestination(targetDirectoryPath, preferredName, isDirectory,
                                                          reservedNames);
    }

    bool writeBytes(const QString& path, const QByteArray& bytes) const
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        return file.write(bytes) == bytes.size();
    }

    std::optional<QString> pasteableBinaryFormat(const QMimeData* mimeData) const
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

    QString extensionForMimeFormat(const QString& format) const
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

    bool copyDirectoryRecursively(const QString& sourcePath, const QString& destinationPath) const
    {
        return FileOperationService::copyDirectoryRecursively(sourcePath, destinationPath);
    }

    void rekeyPathMetadataAfterRename(const QString& oldPath, const QString& newPath, bool wasDir)
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

    QString rekeyPathAfterRename(const QString& path, const QString& oldPath, const QString& newPath, bool wasDir) const
    {
        if (path == oldPath) {
            return newPath;
        }
        if (wasDir && isDescendantPath(oldPath, path)) {
            return newPath + path.mid(oldPath.length());
        }
        return path;
    }

    QString orderKeyForDirectory(const QString& directoryPath) const
    {
        const QString relative = QDir(rootPath_).relativeFilePath(directoryPath);
        return relative == QStringLiteral(".") ? QString() : relative;
    }

    QString orderFilePath() const
    {
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/order.json"));
    }

    QString colorFilePath() const
    {
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/colors.json"));
    }

    QString previewFilePath() const
    {
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/previews.json"));
    }

    QString linkFilePath() const
    {
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/links.json"));
    }

    QString collapsedFilePath() const
    {
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/collapsed.json"));
    }

    QString viewStateFilePath() const
    {
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/view.json"));
    }

    QString hashCacheFilePath() const
    {
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/hashcache.json"));
    }

    QString relativeKeyForPath(const QString& path) const
    {
        return QDir(rootPath_).relativeFilePath(path);
    }

    QString absolutePathForKey(const QString& key) const
    {
        return QDir(rootPath_).absoluteFilePath(key);
    }

    void loadOrderFile()
    {
        fileOrders_.clear();
        if (!mycelStorageEnabled_) {
            return;
        }

        QFile file(orderFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonObject directories = doc.object().value(QStringLiteral("directories")).toObject();
        for (auto it = directories.begin(); it != directories.end(); ++it) {
            QStringList names;
            const QJsonArray array = it.value().toArray();
            for (const QJsonValue& value : array) {
                names.append(value.toString());
            }
            fileOrders_[it.key()] = names;
        }
    }

    void saveOrderFile()
    {
        if (!mycelStorageEnabled_) {
            return;
        }

        QDir root(rootPath_);
        if (!root.mkpath(QStringLiteral(".mycel"))) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral(".mycel フォルダを作成できませんでした。"));
            return;
        }

        QJsonObject directories;
        for (const auto& [key, names] : fileOrders_) {
            QJsonArray array;
            for (const QString& name : names) {
                array.append(name);
            }
            directories.insert(key, array);
        }

        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("directories"), directories);

        if (!writeJsonFileAtomic(orderFilePath(), rootObject)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("並び順を保存できませんでした。"));
        }
    }

    void loadColorFile()
    {
        userColors_.clear();
        if (!mycelStorageEnabled_) {
            return;
        }

        QFile file(colorFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonObject colors = doc.object().value(QStringLiteral("colors")).toObject();
        for (auto it = colors.begin(); it != colors.end(); ++it) {
            const QColor color(it.value().toString());
            if (color.isValid()) {
                userColors_[absolutePathForKey(it.key())] = color;
            }
        }
    }

    void saveColorFile()
    {
        if (!mycelStorageEnabled_) {
            return;
        }

        QDir root(rootPath_);
        if (!root.mkpath(QStringLiteral(".mycel"))) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral(".mycel フォルダを作成できませんでした。"));
            return;
        }

        QJsonObject colors;
        for (const auto& [path, color] : userColors_) {
            colors.insert(relativeKeyForPath(path), color.name(QColor::HexRgb));
        }

        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("colors"), colors);

        if (!writeJsonFileAtomic(colorFilePath(), rootObject)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("色設定を保存できませんでした。"));
        }
    }

    void loadHashCacheFile()
    {
        fileHashCache_.clear();
        if (!mycelStorageEnabled_) {
            return;
        }

        QFile file(hashCacheFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonObject files = doc.object().value(QStringLiteral("files")).toObject();
        for (auto it = files.begin(); it != files.end(); ++it) {
            const QJsonObject entry = it.value().toObject();
            FileHashCacheEntry cacheEntry;
            cacheEntry.size = static_cast<qint64>(entry.value(QStringLiteral("size")).toDouble());
            cacheEntry.mtimeMs = static_cast<qint64>(entry.value(QStringLiteral("mtime")).toDouble());
            cacheEntry.hash = QByteArray::fromHex(entry.value(QStringLiteral("hash")).toString().toLatin1());
            if (!cacheEntry.hash.isEmpty()) {
                fileHashCache_[it.key()] = cacheEntry;
            }
        }
    }

    void saveHashCacheFile()
    {
        if (!mycelStorageEnabled_) {
            return;
        }

        QDir root(rootPath_);
        if (!root.mkpath(QStringLiteral(".mycel"))) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral(".mycel フォルダを作成できませんでした。"));
            return;
        }

        QJsonObject files;
        for (const auto& [key, entry] : fileHashCache_) {
            QJsonObject entryObject;
            entryObject.insert(QStringLiteral("size"), static_cast<double>(entry.size));
            entryObject.insert(QStringLiteral("mtime"), static_cast<double>(entry.mtimeMs));
            entryObject.insert(QStringLiteral("hash"), QString::fromLatin1(entry.hash.toHex()));
            files.insert(key, entryObject);
        }

        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("files"), files);

        if (!writeJsonFileAtomic(hashCacheFilePath(), rootObject)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("ハッシュキャッシュを保存できませんでした。"));
        }
    }

    void loadPreviewFile()
    {
        previewPaths_.clear();
        previewSizes_.clear();
        previewImageScales_.clear();
        if (!mycelStorageEnabled_) {
            return;
        }

        QFile file(previewFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonObject previews = doc.object().value(QStringLiteral("previews")).toObject();
        for (auto it = previews.begin(); it != previews.end(); ++it) {
            const QString absolutePath = absolutePathForKey(it.key());
            const QJsonObject preview = it.value().toObject();
            if (preview.value(QStringLiteral("open")).toBool(false)) {
                const QFileInfo info(absolutePath);
                const auto embedUrl = youtubeEmbedUrlForFile(info);
                if (!embedUrl || QFileInfo::exists(youtubeThumbnailCachePathForEmbedUrl(*embedUrl))) {
                    previewPaths_.insert(absolutePath);
                }
            }

            const QFileInfo info(absolutePath);
            const qreal scale = preview.value(QStringLiteral("scale")).toDouble(0.0);
            if (isImagePreviewFile(info) && scale > 0.0) {
                previewImageScales_[absolutePath] = imagePreviewScaleForSize(info, imagePreviewSizeForScale(info, scale), false);
                previewSizes_[absolutePath] = imagePreviewSizeForScale(info, previewImageScales_[absolutePath]);
            } else {
                const qreal width = preview.value(QStringLiteral("width")).toDouble(0.0);
                const qreal height = preview.value(QStringLiteral("height")).toDouble(0.0);
                if (width > 0.0 && height > 0.0) {
                    if (isImagePreviewFile(info)) {
                        const qreal legacyScale = imagePreviewScaleForSize(info, QSizeF(width, height), width < height);
                        previewImageScales_[absolutePath] = legacyScale;
                        previewSizes_[absolutePath] = imagePreviewSizeForScale(info, legacyScale);
                    } else {
                        previewSizes_[absolutePath] = clampedPreviewSizeForFile(info, QSizeF(width, height));
                    }
                }
            }
        }
    }

    void savePreviewFile()
    {
        if (!mycelStorageEnabled_) {
            return;
        }

        QDir root(rootPath_);
        if (!root.mkpath(QStringLiteral(".mycel"))) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral(".mycel フォルダを作成できませんでした。"));
            return;
        }

        QSet<QString> paths = previewPaths_;
        for (const auto& [path, size] : previewSizes_) {
            paths.insert(path);
        }
        for (const auto& [path, scale] : previewImageScales_) {
            paths.insert(path);
        }

        QJsonObject previews;
        for (const QString& path : paths.values()) {
            QJsonObject preview;
            preview.insert(QStringLiteral("open"), previewPaths_.contains(path));
            const auto size = previewSizes_.find(path);
            if (size != previewSizes_.end()) {
                preview.insert(QStringLiteral("width"), size->second.width());
                preview.insert(QStringLiteral("height"), size->second.height());
            }
            const auto scale = previewImageScales_.find(path);
            if (scale != previewImageScales_.end()) {
                preview.insert(QStringLiteral("scale"), scale->second);
            }
            previews.insert(relativeKeyForPath(path), preview);
        }

        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("previews"), previews);

        if (!writeJsonFileAtomic(previewFilePath(), rootObject)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("プレビュー状態を保存できませんでした。"));
        }
    }

    bool loadCollapsedFile()
    {
        collapsedPaths_.clear();
        if (!mycelStorageEnabled_) {
            return false;
        }

        QFile file(collapsedFilePath());
        if (!file.exists()) {
            return false;
        }
        if (!file.open(QIODevice::ReadOnly)) {
            return true;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonArray folders = doc.object().value(QStringLiteral("folders")).toArray();
        for (const QJsonValue& value : folders) {
            const QString key = value.toString();
            if (!key.isEmpty()) {
                collapsedPaths_.insert(absolutePathForKey(key));
            }
        }
        return true;
    }

    void saveCollapsedFile()
    {
        if (!mycelStorageEnabled_) {
            return;
        }

        QDir root(rootPath_);
        if (!root.mkpath(QStringLiteral(".mycel"))) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral(".mycel フォルダを作成できませんでした。"));
            return;
        }

        QJsonArray folders;
        QStringList paths = collapsedPaths_.values();
        paths.sort(Qt::CaseInsensitive);
        for (const QString& path : paths) {
            folders.append(relativeKeyForPath(path));
        }

        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("folders"), folders);

        if (!writeJsonFileAtomic(collapsedFilePath(), rootObject)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("折りたたみ状態を保存できませんでした。"));
        }
    }

    void loadLinkFile()
    {
        fileLinks_.clear();
        if (!mycelStorageEnabled_) {
            return;
        }

        QFile file(linkFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonArray links = doc.object().value(QStringLiteral("links")).toArray();
        QSet<QString> seen;
        for (const QJsonValue& value : links) {
            const QJsonObject link = value.toObject();
            const QString fromKey = link.value(QStringLiteral("from")).toString();
            const QString toKey = link.value(QStringLiteral("to")).toString();
            if (fromKey.isEmpty() || toKey.isEmpty()) {
                continue;
            }
            const QString from = absolutePathForKey(fromKey);
            const QString to = absolutePathForKey(toKey);
            if (from == to) {
                continue;
            }
            const QString key = from + QLatin1Char('\n') + to;
            if (seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            fileLinks_.push_back({from, to});
        }
        // A target sits beside exactly one source (see addFileLink()); links.json can end up
        // with two sources claiming the same target (e.g. an external rename collision reconciled
        // by rekeyPathMetadataAfterRename before this file was last saved), which would otherwise
        // make layoutFileLinks() silently reposition that target once per source.
        dedupeFileLinksKeepingLastPerTarget(fileLinks_);
    }

    void saveLinkFile()
    {
        if (!mycelStorageEnabled_) {
            return;
        }

        QDir root(rootPath_);
        if (!root.mkpath(QStringLiteral(".mycel"))) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral(".mycel フォルダを作成できませんでした。"));
            return;
        }

        QJsonArray links;
        QSet<QString> seen;
        for (const FileLink& link : fileLinks_) {
            if (link.from.isEmpty() || link.to.isEmpty() || link.from == link.to) {
                continue;
            }
            const QString key = link.from + QLatin1Char('\n') + link.to;
            if (seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            QJsonObject object;
            object.insert(QStringLiteral("from"), relativeKeyForPath(link.from));
            object.insert(QStringLiteral("to"), relativeKeyForPath(link.to));
            links.append(object);
        }

        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("links"), links);

        if (!writeJsonFileAtomic(linkFilePath(), rootObject)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("関連設定を保存できませんでした。"));
        }
    }

    QJsonObject currentWindowStateObject() const
    {
        const QRect geometry = (isMaximized() || isFullScreen()) ? normalGeometry() : this->geometry();
        QJsonObject geometryObject;
        geometryObject.insert(QStringLiteral("x"), geometry.x());
        geometryObject.insert(QStringLiteral("y"), geometry.y());
        geometryObject.insert(QStringLiteral("width"), geometry.width());
        geometryObject.insert(QStringLiteral("height"), geometry.height());

        QJsonObject windowObject;
        windowObject.insert(QStringLiteral("geometry"), geometryObject);
        windowObject.insert(QStringLiteral("maximized"), isMaximized());
        windowObject.insert(QStringLiteral("fullScreen"), isFullScreen());
        return windowObject;
    }

    QJsonObject currentViewStateObject() const
    {
        const QTransform transform = view_->transform();

        QJsonObject transformObject;
        transformObject.insert(QStringLiteral("m11"), transform.m11());
        transformObject.insert(QStringLiteral("m12"), transform.m12());
        transformObject.insert(QStringLiteral("m13"), transform.m13());
        transformObject.insert(QStringLiteral("m21"), transform.m21());
        transformObject.insert(QStringLiteral("m22"), transform.m22());
        transformObject.insert(QStringLiteral("m23"), transform.m23());
        transformObject.insert(QStringLiteral("m31"), transform.m31());
        transformObject.insert(QStringLiteral("m32"), transform.m32());
        transformObject.insert(QStringLiteral("m33"), transform.m33());

        QJsonObject scrollObject;
        scrollObject.insert(QStringLiteral("horizontal"), view_->horizontalScrollBar()->value());
        scrollObject.insert(QStringLiteral("vertical"), view_->verticalScrollBar()->value());

        // The viewport centre in scene coordinates. Unlike raw scrollbar values this is
        // independent of the viewport size, so restoring it works even though the window's
        // final size (e.g. maximized) is only reached after the restore runs.
        const QPointF center = view_->mapToScene(view_->viewport()->rect().center());
        QJsonObject centerObject;
        centerObject.insert(QStringLiteral("x"), center.x());
        centerObject.insert(QStringLiteral("y"), center.y());

        const QJsonObject windowObject = currentWindowStateObject();

        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("transform"), transformObject);
        rootObject.insert(QStringLiteral("scroll"), scrollObject);
        rootObject.insert(QStringLiteral("center"), centerObject);
        rootObject.insert(QStringLiteral("window"), windowObject);
        return rootObject;
    }

    std::optional<QJsonObject> loadViewStateObject() const
    {
        if (!mycelStorageEnabled_) {
            return std::nullopt;
        }

        QFile file(viewStateFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            return std::nullopt;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject()) {
            return std::nullopt;
        }
        return doc.object();
    }

    bool readViewStateObject(const QJsonObject& rootObject, QTransform& transform, int& horizontal, int& vertical,
                             QPointF& center, bool& hasCenter) const
    {
        const QJsonObject transformObject = rootObject.value(QStringLiteral("transform")).toObject();
        const QJsonObject scrollObject = rootObject.value(QStringLiteral("scroll")).toObject();
        const QJsonObject centerObject = rootObject.value(QStringLiteral("center")).toObject();
        hasCenter = centerObject.contains(QStringLiteral("x")) && centerObject.contains(QStringLiteral("y"));
        if (hasCenter) {
            center = QPointF(centerObject.value(QStringLiteral("x")).toDouble(0.0),
                             centerObject.value(QStringLiteral("y")).toDouble(0.0));
        }

        const qreal m11 = transformObject.value(QStringLiteral("m11")).toDouble(0.0);
        const qreal m12 = transformObject.value(QStringLiteral("m12")).toDouble(0.0);
        const qreal m13 = transformObject.value(QStringLiteral("m13")).toDouble(0.0);
        const qreal m21 = transformObject.value(QStringLiteral("m21")).toDouble(0.0);
        const qreal m22 = transformObject.value(QStringLiteral("m22")).toDouble(0.0);
        const qreal m23 = transformObject.value(QStringLiteral("m23")).toDouble(0.0);
        const qreal m31 = transformObject.value(QStringLiteral("m31")).toDouble(0.0);
        const qreal m32 = transformObject.value(QStringLiteral("m32")).toDouble(0.0);
        const qreal m33 = transformObject.value(QStringLiteral("m33")).toDouble(0.0);
        if (std::abs(m11) < 0.001 || std::abs(m22) < 0.001 || std::abs(m33) < 0.001) {
            return false;
        }

        transform = QTransform(m11, m12, m13, m21, m22, m23, m31, m32, m33);
        horizontal = scrollObject.value(QStringLiteral("horizontal")).toInt(0);
        vertical = scrollObject.value(QStringLiteral("vertical")).toInt(0);
        return true;
    }

    bool loadViewState(QTransform& transform, int& horizontal, int& vertical,
                       QPointF& center, bool& hasCenter) const
    {
        const std::optional<QJsonObject> rootObject = loadViewStateObject();
        if (!rootObject) {
            return false;
        }

        return readViewStateObject(*rootObject, transform, horizontal, vertical, center, hasCenter);
    }

    void saveViewState()
    {
        if (!view_ || !mycelStorageEnabled_) {
            return;
        }
        viewStateSaveTimer_.stop();

        QJsonObject rootObject;
        if (boardMode_) {
            // Board mode: the canvas belongs to the pattern (saved there); keep the tree's
            // stored transform/centre untouched and refresh only window + board flags.
            rootObject = loadViewStateObject().value_or(QJsonObject());
            rootObject.insert(QStringLiteral("version"), 1);
            rootObject.insert(QStringLiteral("window"), currentWindowStateObject());
            saveBoardPattern();
        } else {
            rootObject = currentViewStateObject();
        }
        QJsonObject boardObject;
        boardObject.insert(QStringLiteral("active"), boardMode_);
        boardObject.insert(QStringLiteral("pattern"), boardPatternName_);
        rootObject.insert(QStringLiteral("board"), boardObject);

        QDir root(rootPath_);
        if (!root.mkpath(QStringLiteral(".mycel"))) {
            return;
        }

        writeJsonFileAtomic(viewStateFilePath(), rootObject);
    }

    void restoreWindowStateFromSettingsFile()
    {
        const std::optional<QJsonObject> rootObject = loadViewStateObject();
        if (!rootObject) {
            return;
        }

        const QJsonObject windowObject = rootObject->value(QStringLiteral("window")).toObject();
        const QJsonObject geometryObject = windowObject.value(QStringLiteral("geometry")).toObject();
        const int width = geometryObject.value(QStringLiteral("width")).toInt(0);
        const int height = geometryObject.value(QStringLiteral("height")).toInt(0);
        if (width >= 320 && height >= 240) {
            resize(width, height);
            move(geometryObject.value(QStringLiteral("x")).toInt(x()),
                 geometryObject.value(QStringLiteral("y")).toInt(y()));
        }

        if (windowObject.value(QStringLiteral("fullScreen")).toBool(false)) {
            setWindowState(Qt::WindowFullScreen);
        } else if (windowObject.value(QStringLiteral("maximized")).toBool(false)) {
            setWindowState(Qt::WindowMaximized);
        } else {
            setWindowState(Qt::WindowNoState);
        }
    }

    // Restacks existing NodeItems (paint order) to match top-to-bottom visual position rather
    // than tree/scan insertion order, so that when a file link's fan happens to overlap a
    // neighbouring node — e.g. a tall open preview spilling past the row it was given — the
    // lower node still paints on top instead of the stacking order being an accident of
    // file-system scan order. Safe to call any time node centers have just been recomputed;
    // does nothing to items sharing a center.y() (their relative order is left as-is).
    void restackNodeItemsByVisualOrder()
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

    void scheduleViewStateSave()
    {
        if (inlineRenameActivitySuspended_ || restoringViewState_ || pendingViewRestoreReapplies_ > 0 ||
            !mycelStorageEnabled_) {
            return;  // ignore scroll churn while the restored view is still settling
        }
        viewStateSaveTimer_.start();
    }

    void renderCurrentTree(bool fitAfterRender)
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

    // Render the chain of parent .mycel roots as folder nodes to the left of the root, each joined to
    // the node on its right by a connector. Returns their combined scene rect (empty for a top root)
    // so the caller can extend the scene bounds to include them.
    QRectF addParentRootItems()
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

    // ==== Board mode (free-form card layout, saved as patterns) ============================
    // See docs/board-mode-plan.ja.md. One pattern = one JSON under .mycel/boards/. Only cards
    // that are placed (visible) and whose real file exists are shown; hidden and new files stay
    // off-screen until called in (Phase 2 list). Positions are free (no snap, unbounded area).
public:
    bool boardModeActive() const { return boardMode_; }

    QString boardsDirPath() const { return QDir(rootPath_).filePath(QStringLiteral(".mycel/boards")); }

    QString boardPatternFilePath(const QString& name) const
    {
        return QDir(boardsDirPath()).filePath(name + QStringLiteral(".json"));
    }

    QStringList availableBoardPatternNames() const
    {
        QStringList names;
        const QStringList files = QDir(boardsDirPath()).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        for (const QString& file : files) {
            names << QFileInfo(file).completeBaseName();
        }
        return names;
    }

    // Sort directory entries with the same custom order the tree uses (fileOrders_).
    void applyCustomEntryOrder(QFileInfoList& entries, const QString& dirPath) const
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

    // All board candidate files as (absolute path, row): .mycel excluded, sub-root boundaries
    // respected, collapse state ignored. One row per FOLDER: the root's direct files form the
    // first row, then each folder (level by level, in tree order) starts a new row with its
    // direct files laid left-to-right in the folder's custom order.
    std::vector<std::pair<QString, int>> collectBoardFiles() const
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

    // A flat display node for one board card (same sizing rules as tree file nodes).
    std::unique_ptr<Node> makeBoardNode(const QString& path, const QPointF& center) const
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

    // Create a new pattern from the current files: one row per folder, left-to-right, all visible.
    // Cards in a row share the same TOP line, and the x-advance uses the full visual width
    // (header + open preview) so nothing overlaps and the gaps stay even.
    void createBoardPattern(const QString& name)
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

    void saveBoardPattern()
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

    bool loadBoardPattern(const QString& name)
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

    void ensureBoardPattern()
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

    // Rebuild the scene from the active pattern: placed+visible cards whose file exists.
    void renderBoardScene(bool restoreView)
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

    void restoreBoardViewOrFit()
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

    // Metadata refresh for board cards (preview open/close/size) without touching positions.
    void refreshBoardCards()
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

    // A board card finished a free-move drag: persist its new position.
    void boardCardMoved(NodeItem* item)
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

    // Hide a card from the active pattern (the file itself is untouched).
    void hideBoardCard(const QString& path)
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

    // Switch between the tree and the board. persistCurrent=false is used at startup where the
    // outgoing state must not be overwritten.
    void setBoardMode(bool on, bool persistCurrent = true)
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

    void switchBoardPattern(const QString& name)
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

    void createBoardPatternInteractive()
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

    void renameBoardPatternInteractive()
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

    void deleteBoardPatternInteractive()
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

    // List the cards that are hidden or not yet placed (e.g. files added after the pattern was
    // created) and place the selected ones: hidden cards return to their remembered position,
    // new cards appear at the current view centre.
    void showBoardHiddenCardsDialog()
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

    void updateBoardPatternMenu()
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

    // ==== end board mode ====================================================================
private:
    // Refreshes the cached content hash for one real file (used for external-rename detection),
    // skipping large files and cloud placeholders. Reuses the cached hash when size and mtime
    // are unchanged, so unchanged files are not re-read on every scan. Returns true if the entry
    // was added or updated, so the caller knows whether to persist the cache.
    bool updateHashCacheEntryForFile(const QFileInfo& info)
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

    // Immediately hashes and persists a single file's cache entry if it is not already cached.
    // Called when the user attaches metadata (color, etc.) to a file: that is a strong signal
    // the file is worth tracking, and waiting for the next directory scan or the periodic
    // background sweep to get to it would leave a window where an external rename right after
    // tagging could not be reconciled (no "before" hash would exist to match against).
    void ensureHashCachedForRenameTracking(const QString& path)
    {
        if (!mycelStorageEnabled_) {
            return;
        }
        if (updateHashCacheEntryForFile(QFileInfo(path))) {
            saveHashCacheFile();
        }
    }

    // Refreshes hash-cache entries for the real, immediate files of one directory (not the
    // display tree, so this works the same whether or not the directory is collapsed/visible).
    bool updateHashCacheForDirectory(const QString& directoryPath)
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

    void rebuild(bool fitAfterRebuild)
    {
        saveSideEditorNow();
        finishInlineRename(false);
        root_ = scanTree(rootPath_, 0, -1, collapsedPaths_, previewPaths_, previewSizes_, fileOrders_, rootPath_,
                         mycelStorageEnabled_);
        if (boardMode_) {
            renderBoardScene(fitAfterRebuild);
            return;
        }
        renderCurrentTree(fitAfterRebuild);
    }

    // Re-apply metadata-derived display fields (preview open state and size) to the cached tree.
    void refreshCachedNodeMetadata(Node& node)
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

    // Metadata-only re-render: preview open/close/size and link changes do not alter the file
    // structure, so re-layout the cached tree instead of re-stat'ing the whole directory tree
    // from disk (rebuild). The layout is recomputed on the same Node objects and the existing
    // NodeItems are moved in place — no scene rebuild, so the selection and inline preview
    // widgets of untouched nodes survive. Falls back to a full re-render if any node has no
    // live item.
    void relayout()
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

    void scheduleRestoreViewStateOrFit()
    {
        QTimer::singleShot(0, this, [this] { restoreViewStateOrFit(); });
    }

    void restoreViewStateOrFit()
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

    void reapplyRestoredViewCenter(int generation)
    {
        if (generation != viewRestoreGeneration_ || pendingViewRestoreReapplies_ <= 0 || !view_ || boardMode_) {
            return;  // a newer restore, a root switch, or a mode switch superseded this re-apply
        }
        --pendingViewRestoreReapplies_;
        restoringViewState_ = true;
        view_->centerOn(pendingViewRestoreCenter_);
        restoringViewState_ = false;
    }

    void fitToMap()
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

    void applyEditorPanePosition(QString position, bool persist)
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

    QString rootPath_;
    bool mycelStorageEnabled_ = true;
    QSet<QString> collapsedPaths_;
    std::map<QString, QColor> userColors_;
    QSet<QString> previewPaths_;
    QSet<QString> pendingYouTubeThumbnailPaths_;
    QSet<QString> pendingUrlThumbnailPaths_;
    QSet<QString> pendingUrlThumbnailInlineOpenPaths_;
    QString queuedPreviewPath_;
    QString queuedCollapsePath_;
    QTimer previewClickTimer_;
    QFileSystemWatcher* fileSystemWatcher_ = nullptr;
    QTimer fileSystemRefreshTimer_;
    QTimer backgroundReconcileTimer_;
    // True from construction until finishStartupScan() has applied the background walk's
    // results; while set, watcher resets and the periodic reconcile skip their own full-tree
    // walks (the startup scan thread delivers the same data without blocking the GUI thread).
    bool startupScanPending_ = true;
    QThread* startupScanThread_ = nullptr;
    std::shared_ptr<std::atomic_bool> startupScanCancelled_;
    // Mirrors startupScanPending_/startupScanThread_/startupScanCancelled_ for the periodic
    // reconcile sweep (runBackgroundReconcile): the whole-tree walk + hash refresh run on this
    // thread instead of the GUI thread, so a large or network-mounted root does not freeze the
    // UI every BackgroundReconcileIntervalMs.
    bool reconcileScanPending_ = false;
    QThread* reconcileScanThread_ = nullptr;
    std::shared_ptr<std::atomic_bool> reconcileScanCancelled_;
    // True when the root is on a network filesystem: no native watches are registered (see
    // isNetworkFileSystemPath) and the periodic sweep doubles as the structural refresh.
    bool nativeWatcherDisabled_ = false;
    QSet<QString> pendingFileSystemPaths_;
    std::map<QString, QSizeF> previewSizes_;
    std::map<QString, qreal> previewImageScales_;
    std::map<QString, QStringList> fileOrders_;
    std::map<QString, FileHashCacheEntry> fileHashCache_;
    QGraphicsProxyWidget* renameProxy_ = nullptr;
    QLineEdit* renameEdit_ = nullptr;
    QString renamingPath_;
    bool finishingRename_ = false;
    bool inlineRenameActivitySuspended_ = false;
    QStringList pausedWatcherFiles_;
    QStringList pausedWatcherDirectories_;
    MindMapScene scene_;
    SelectionController selection_{scene_};
    QString selectionRangeAnchorPath_;
    QSplitter* editorSplitter_ = nullptr;
    BoardView* view_ = nullptr;
    QDockWidget* debugDock_ = nullptr;
    QPlainTextEdit* debugText_ = nullptr;
    QString dragHoverFolderPath_;
    QString dragHoverLinkTargetPath_;
    QStringList copiedPaths_;
    std::vector<FileLink> fileLinks_;

    // ---- Undo/Redo history (types defined above, near the history methods) -------------
    std::vector<HistoryEntry> undoStack_;
    std::vector<HistoryEntry> redoStack_;
    // Path → live NodeItem index; rebuilt whenever the scene is re-rendered. Keeps hot lookups
    // (drag/resize/selection by path) O(1) instead of scanning every scene item.
    QHash<QString, NodeItem*> nodeItemsByPath_;
    // The scene's connector layer and the parent-root items' combined bounds, kept so a
    // metadata-only relayout can refresh geometry in place without recreating the scene.
    ConnectionLayerItem* connectionLayer_ = nullptr;
    QRectF parentRootItemsBounds_;
    // ---- Board mode state (docs/board-mode-plan.ja.md; BoardCardState is declared above
    // MetadataSnapshot so board cards can ride along in undo/redo snapshots) ----
    bool boardMode_ = false;
    QString boardPatternName_;
    std::map<QString, BoardCardState> boardCards_;  // key = root-relative path
    QJsonObject boardPatternView_;                  // per-pattern view (scale + centre)
    std::vector<std::unique_ptr<Node>> boardNodes_;
    QMenu* boardPatternMenu_ = nullptr;
    QAction* boardModeAction_ = nullptr;
    std::unique_ptr<HistoryEntry> historyGroup_;
    bool applyingHistory_ = false;
    int historyTrashCounter_ = 0;
    static constexpr int kMaxHistoryEntries = 100;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;

    QAction* editorPaneAction_ = nullptr;
    QAction* debugPaneAction_ = nullptr;
    QAction* lightThemeAction_ = nullptr;
    QAction* darkThemeAction_ = nullptr;
    AppTheme uiTheme_ = AppTheme::Light;
    QStringList debugEvents_;
    QWidget* sideEditorPanel_ = nullptr;
    QStackedWidget* sidePreviewStack_ = nullptr;
    PreviewText* sidePreviewText_ = nullptr;
    QLabel* sidePreviewImage_ = nullptr;
#if MYCEL_HAS_WEBENGINE
    QWebEngineView* sideHtmlWeb_ = nullptr;
#endif
    QVideoWidget* sidePreviewVideo_ = nullptr;
    QMediaPlayer* sidePreviewPlayer_ = nullptr;
    QAudioOutput* sidePreviewAudioOutput_ = nullptr;
    TextEditor* sideEditor_ = nullptr;
    QLabel* sideModeLabel_ = nullptr;
    QLabel* sideEditorPathLabel_ = nullptr;
    QLabel* sideEditorStatusLabel_ = nullptr;
    QTimer sideEditorSaveTimer_;
    QTimer viewStateSaveTimer_;
    QString sideEditorPath_;
    QString editorPanePosition_ = QStringLiteral("right");
    QDateTime sideEditorLastModified_;
    bool sideEditorLoading_ = false;
    bool sideEditorDirty_ = false;
    bool sideEditorEditing_ = false;
    bool restoringViewState_ = false;
    // Startup/root-switch view restore: the scene-space centre to re-apply once the window
    // geometry settles, and how many re-applies remain (saves are suppressed meanwhile).
    // The generation counter voids scheduled re-applies superseded by a newer restore/switch.
    QPointF pendingViewRestoreCenter_;
    int pendingViewRestoreReapplies_ = 0;
    int viewRestoreGeneration_ = 0;
    bool suppressSideEditorSelectionUpdate_ = false;
    std::unique_ptr<Node> root_;
};

void NodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    showContextMenuAt(event->screenPos());
    event->accept();
}

void NodeItem::showContextMenuAt(const QPoint& screenPos)
{
    QPointer<MainWindow> window = window_;
    if (!window) {
        return;
    }

    QMenu menu;
    const bool multiItemSelection = isSelected() && window->selectedDeletableItemCount() > 1;
    const QString itemPath = node_->path;
    auto addColorMenu = [window, itemPath](QMenu& parentMenu, std::vector<QAction*>& colorActions, QAction*& clearColorAction) {
        if (!window || !window->mycelStorageEnabled()) {
            return;
        }
        QMenu* colorMenu = parentMenu.addMenu(QStringLiteral("色"));
        for (const auto& [label, color] : colorPalette()) {
            QAction* action = colorMenu->addAction(label);
            action->setData(color);
            action->setIcon(colorDotIcon(color));
            colorActions.push_back(action);
        }
        colorMenu->addSeparator();
        clearColorAction = colorMenu->addAction(QStringLiteral("色をクリア"));
        clearColorAction->setEnabled(window->hasUserColorPath(itemPath));
    };

    if (window->boardModeActive()) {
        // Board mode: no file-structure operations. Preview/open/colour plus "hide from pattern".
        const QString filePath = itemPath;
        QAction* previewAction = menu.addAction(QStringLiteral("プレビューを開く/閉じる"));
        QAction* openAction = menu.addAction(QStringLiteral("開く"));
        QAction* openWithAction = menu.addAction(QStringLiteral("別のアプリで開く…"));
        menu.addSeparator();
        std::vector<QAction*> colorActions;
        QAction* clearColorAction = nullptr;
        addColorMenu(menu, colorActions, clearColorAction);
        menu.addSeparator();
        QAction* hideAction = menu.addAction(QStringLiteral("非表示（パターンから隠す）"));
        QAction* selected = menu.exec(screenPos);
        if (selected == previewAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->toggleInlinePreviewPath(filePath);
                }
            });
        } else if (selected == openAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->openPath(filePath);
                }
            });
        } else if (selected == openWithAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->openWithApplication(filePath);
                }
            });
        } else if (selected == hideAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->hideBoardCard(filePath);
                }
            });
        } else if (selected && selected->data().canConvert<QColor>()) {
            const QColor color = selected->data().value<QColor>();
            QTimer::singleShot(0, window, [window, filePath, color] {
                if (window) {
                    window->setNodeColorPath(filePath, color);
                }
            });
        } else if (selected && selected == clearColorAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->clearNodeColorPath(filePath);
                }
            });
        }
        return;
    }

    if (multiItemSelection) {
        const QStringList selectedFilePaths = window->selectedFilePaths();
        QAction* previewSelectionAction = menu.addAction(QStringLiteral("プレビューを開く/閉じる"));
        previewSelectionAction->setEnabled(!selectedFilePaths.isEmpty());
        QAction* copySelectionAction = menu.addAction(QStringLiteral("コピー"));
        QAction* deleteSelectionAction = menu.addAction(QStringLiteral("削除"));

        QAction* selected = menu.exec(screenPos);
        if (selected == previewSelectionAction) {
            QTimer::singleShot(0, window, [window, selectedFilePaths] {
                if (window) {
                    window->toggleFilePreviewsForPaths(selectedFilePaths);
                }
            });
        } else if (selected == copySelectionAction) {
            QTimer::singleShot(0, window, [window] {
                if (window) {
                    window->copySelectedItems();
                }
            });
        } else if (selected == deleteSelectionAction) {
            QTimer::singleShot(0, window, [window] {
                if (window) {
                    window->deleteSelectedItems();
                }
            });
        }
        return;
    }

    if (node_->isDir) {
        const QString folderPath = itemPath;
        const bool isRootFolder = node_ == window->rootNode();
        if (node_->isSubRoot) {
            QAction* openRootAction = menu.addAction(QStringLiteral("このルートを開く"));
            menu.addSeparator();
            QAction* integrateAction = menu.addAction(QStringLiteral("親に統合（.mycel を解除）"));
            QAction* subSelected = menu.exec(screenPos);
            if (subSelected == openRootAction) {
                QTimer::singleShot(0, window, [window, folderPath] {
                    if (window) {
                        window->switchIntoSubRoot(folderPath);
                    }
                });
            } else if (subSelected == integrateAction) {
                QTimer::singleShot(0, window, [window, folderPath] {
                    if (window) {
                        window->integrateChildRootIntoParent(folderPath);
                    }
                });
            }
            return;
        }
        // Group 1: open / view
        QAction* openAction = menu.addAction(QStringLiteral("開く"));
        QAction* collapseAction = menu.addAction(node_->collapsed ? QStringLiteral("展開") : QStringLiteral("折りたたむ"));
        menu.addSeparator();
        // Group 2: create
        QMenu* newFileMenu = menu.addMenu(QStringLiteral("ファイルを作成"));
        QAction* fileBelowAction = newFileMenu->addAction(QStringLiteral("下の階層に作成"));
        QAction* fileSameAction = newFileMenu->addAction(QStringLiteral("同じ階層に作成"));
        fileSameAction->setEnabled(!isRootFolder);
        QMenu* newFolderMenu = menu.addMenu(QStringLiteral("フォルダを作成"));
        QAction* folderBelowAction = newFolderMenu->addAction(QStringLiteral("下の階層に作成"));
        QAction* folderSameAction = newFolderMenu->addAction(QStringLiteral("同じ階層に作成"));
        folderSameAction->setEnabled(!isRootFolder);
        menu.addSeparator();
        // Group 3: edit / clipboard
        QAction* renameAction = menu.addAction(QStringLiteral("名前変更"));
        renameAction->setEnabled(!isRootFolder);
        QAction* copyAction = menu.addAction(QStringLiteral("コピー"));
        copyAction->setEnabled(!isRootFolder);
        QAction* pasteAction = menu.addAction(QStringLiteral("貼り付け"));
        pasteAction->setEnabled(window->canPasteClipboardToFolderPath(folderPath));
        QAction* deleteAction = menu.addAction(QStringLiteral("削除"));
        deleteAction->setEnabled(!isRootFolder);
        QAction* unlinkAction = menu.addAction(QStringLiteral("関連を解除"));
        unlinkAction->setEnabled(window->hasIncomingFileLinkPath(folderPath));
        QAction* moveLinkUpAction = menu.addAction(QStringLiteral("横リンクを上へ移動"));
        moveLinkUpAction->setEnabled(window->canMoveFileLinkTargetPath(folderPath, -1));
        QAction* moveLinkDownAction = menu.addAction(QStringLiteral("横リンクを下へ移動"));
        moveLinkDownAction->setEnabled(window->canMoveFileLinkTargetPath(folderPath, 1));
        menu.addSeparator();
        // Group 4: organize / appearance
        QMenu* sortMenu = menu.addMenu(QStringLiteral("並べ替え"));
        QAction* sortNameAscAction = sortMenu->addAction(QStringLiteral("名前順（昇順）"));
        QAction* sortNameDescAction = sortMenu->addAction(QStringLiteral("名前順（降順）"));
        sortMenu->addSeparator();
        QAction* sortDateAscAction = sortMenu->addAction(QStringLiteral("日時順（昇順）"));
        QAction* sortDateDescAction = sortMenu->addAction(QStringLiteral("日時順（降順）"));
        std::vector<QAction*> colorActions;
        QAction* clearColorAction = nullptr;
        addColorMenu(menu, colorActions, clearColorAction);
        menu.addSeparator();
        // Group 5: root management
        QAction* makeChildRootAction = menu.addAction(QStringLiteral("子ルートにする（.mycel を作成）"));
        makeChildRootAction->setEnabled(!isRootFolder && window->mycelStorageEnabled());
        QAction* selected = menu.exec(screenPos);
        const QString parentDir = QFileInfo(folderPath).absolutePath();
        if (selected == makeChildRootAction) {
            QTimer::singleShot(0, window, [window, folderPath] {
                if (window) {
                    window->makeFolderChildRoot(folderPath);
                }
            });
            return;
        }
        if (selected == fileBelowAction || selected == fileSameAction) {
            const bool same = (selected == fileSameAction);
            const QString dir = same ? parentDir : folderPath;
            const QString after = same ? folderPath : QString();  // place beside this folder
            QTimer::singleShot(0, window, [window, dir, after] {
                if (window) {
                    window->createFileInDirectory(dir, after);
                }
            });
            return;
        }
        if (selected == folderBelowAction || selected == folderSameAction) {
            const bool same = (selected == folderSameAction);
            const QString dir = same ? parentDir : folderPath;
            const QString after = same ? folderPath : QString();  // place beside this folder
            QTimer::singleShot(0, window, [window, dir, after] {
                if (window) {
                    window->createFolderInDirectory(dir, after);
                }
            });
            return;
        }
        if (selected == sortNameAscAction || selected == sortNameDescAction ||
            selected == sortDateAscAction || selected == sortDateDescAction) {
            const bool byDate = (selected == sortDateAscAction || selected == sortDateDescAction);
            const bool descending = (selected == sortNameDescAction || selected == sortDateDescAction);
            QTimer::singleShot(0, window, [window, folderPath, byDate, descending] {
                if (window) {
                    window->sortFolderChildren(folderPath, byDate, descending);
                }
            });
            return;
        }
        if (selected == collapseAction) {
            QTimer::singleShot(0, window, [window, folderPath] {
                if (window) {
                    window->toggleCollapsedPath(folderPath);
                }
            });
        } else if (selected == renameAction) {
            QTimer::singleShot(0, window, [window, folderPath] {
                if (window) {
                    window->renamePathInline(folderPath);
                }
            });
        } else if (selected == copyAction) {
            QTimer::singleShot(0, window, [window, folderPath] {
                if (window) {
                    window->copyPath(folderPath);
                }
            });
        } else if (selected == pasteAction) {
            QTimer::singleShot(0, window, [window, folderPath] {
                if (window) {
                    window->pasteClipboardToFolderPathAction(folderPath);
                }
            });
        } else if (selected == deleteAction) {
            QTimer::singleShot(0, window, [window, folderPath] {
                if (window) {
                    window->deleteFolderPath(folderPath);
                }
            });
        } else if (selected == unlinkAction) {
            QTimer::singleShot(0, window, [window, folderPath] {
                if (window) {
                    window->removeIncomingFileLinksPath(folderPath);
                }
            });
        } else if (selected == moveLinkUpAction) {
            QTimer::singleShot(0, window, [window, folderPath] {
                if (window) {
                    window->moveFileLinkTargetPath(folderPath, -1);
                }
            });
        } else if (selected == moveLinkDownAction) {
            QTimer::singleShot(0, window, [window, folderPath] {
                if (window) {
                    window->moveFileLinkTargetPath(folderPath, 1);
                }
            });
        } else if (selected && selected->data().canConvert<QColor>()) {
            const QColor color = selected->data().value<QColor>();
            QTimer::singleShot(0, window, [window, folderPath, color] {
                if (window) {
                    window->setNodeColorPath(folderPath, color);
                }
            });
        } else if (clearColorAction && selected == clearColorAction) {
            QTimer::singleShot(0, window, [window, folderPath] {
                if (window) {
                    window->clearNodeColorPath(folderPath);
                }
            });
        } else if (selected == openAction) {
            QTimer::singleShot(0, window, [window, folderPath] {
                if (window) {
                    window->openPath(folderPath);
                }
            });
        }
    } else {
        const QString filePath = itemPath;
        // Group 1: open / view
        QAction* openAction = menu.addAction(QStringLiteral("開く"));
        QAction* openWithAction = menu.addAction(QStringLiteral("別のアプリで開く…"));
        QAction* previewAction = menu.addAction(QStringLiteral("プレビューを開く/閉じる"));
        QAction* editAction = menu.addAction(QStringLiteral("編集"));
        editAction->setEnabled(window->canEditTextFilePath(filePath));
        // Group 2: run (scripts) — present only for runnable scripts
        QAction* runPipelineAction = window->isPipelineScriptFile(filePath)
                                         ? menu.addAction(QStringLiteral("パイプライン実行"))
                                         : nullptr;
        QAction* runGoAction = window->isGoScriptFile(filePath)
                                   ? menu.addAction(QStringLiteral("Go スクリプトとして実行"))
                                   : nullptr;
        if (runPipelineAction || runGoAction) {
            menu.insertSeparator(runPipelineAction ? runPipelineAction : runGoAction);
        }
        menu.addSeparator();
        // Group 3: create
        QMenu* newFileMenu = menu.addMenu(QStringLiteral("ファイルを作成"));
        QAction* newFileSameAction = newFileMenu->addAction(QStringLiteral("同じ階層に作成"));
        QAction* newFileLinkAction = newFileMenu->addAction(QStringLiteral("横リンクで作成"));
        newFileLinkAction->setEnabled(window->mycelStorageEnabled());
        QAction* newFolderSameAction = menu.addAction(QStringLiteral("フォルダを作成（同じ階層）"));
        menu.addSeparator();
        // Group 4: edit / clipboard
        QAction* renameAction = menu.addAction(QStringLiteral("名前変更"));
        QAction* copyAction = menu.addAction(QStringLiteral("コピー"));
        QAction* deleteAction = menu.addAction(QStringLiteral("削除"));
        QAction* unlinkAction = menu.addAction(QStringLiteral("関連を解除"));
        unlinkAction->setEnabled(window->hasIncomingFileLinkPath(filePath));
        QAction* moveLinkUpAction = menu.addAction(QStringLiteral("横リンクを上へ移動"));
        moveLinkUpAction->setEnabled(window->canMoveFileLinkTargetPath(filePath, -1));
        QAction* moveLinkDownAction = menu.addAction(QStringLiteral("横リンクを下へ移動"));
        moveLinkDownAction->setEnabled(window->canMoveFileLinkTargetPath(filePath, 1));
        menu.addSeparator();
        // Group 5: appearance
        QAction* resetPreviewSizeAction = menu.addAction(QStringLiteral("プレビューサイズを初期化"));
        resetPreviewSizeAction->setEnabled(window->hasSavedPreviewSizePath(filePath));
        std::vector<QAction*> colorActions;
        QAction* clearColorAction = nullptr;
        addColorMenu(menu, colorActions, clearColorAction);
        QAction* selected = menu.exec(screenPos);
        const QString parentDir = QFileInfo(filePath).absolutePath();
        if (selected == newFolderSameAction) {
            QTimer::singleShot(0, window, [window, parentDir, filePath] {
                if (window) {
                    window->createFolderInDirectory(parentDir, filePath);  // beside this file
                }
            });
            return;
        }
        if (selected == newFileSameAction) {
            QTimer::singleShot(0, window, [window, parentDir, filePath] {
                if (window) {
                    window->createFileInDirectory(parentDir, filePath);  // beside this file
                }
            });
            return;
        }
        if (selected == newFileLinkAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->createLinkedFileBeside(filePath);
                }
            });
            return;
        }
        if (runPipelineAction && selected == runPipelineAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->runPipelineForScript(filePath);
                }
            });
            return;
        }
        if (runGoAction && selected == runGoAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->runGoScript(filePath);
                }
            });
            return;
        }
        if (selected == previewAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->toggleInlinePreviewPath(filePath);
                }
            });
        } else if (selected == resetPreviewSizeAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->resetPreviewSizePath(filePath);
                }
            });
        } else if (selected == unlinkAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->removeIncomingFileLinksPath(filePath);
                }
            });
        } else if (selected == moveLinkUpAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->moveFileLinkTargetPath(filePath, -1);
                }
            });
        } else if (selected == moveLinkDownAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->moveFileLinkTargetPath(filePath, 1);
                }
            });
        } else if (selected == editAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->editTextFilePath(filePath);
                }
            });
        } else if (selected == renameAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->renamePathInline(filePath);
                }
            });
        } else if (selected == copyAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->copyPath(filePath);
                }
            });
        } else if (selected == deleteAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->deleteFilePath(filePath);
                }
            });
        } else if (selected && selected->data().canConvert<QColor>()) {
            const QColor color = selected->data().value<QColor>();
            QTimer::singleShot(0, window, [window, filePath, color] {
                if (window) {
                    window->setNodeColorPath(filePath, color);
                }
            });
        } else if (clearColorAction && selected == clearColorAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->clearNodeColorPath(filePath);
                }
            });
        } else if (selected == openAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->openPath(filePath);
                }
            });
        } else if (selected == openWithAction) {
            QTimer::singleShot(0, window, [window, filePath] {
                if (window) {
                    window->openWithApplication(filePath);
                }
            });
        }
    }
}

QColor NodeItem::windowColor() const
{
    return window_->colorForNode(node_);
}

QColor NodeItem::windowFill() const
{
    return window_->fillForNode(node_);
}

bool NodeItem::hasUserFill() const
{
    return window_->hasUserColorForNode(node_);
}

QStringList NodeItem::windowInlinePreviewLines() const
{
    return window_->inlinePreviewLines(node_);
}

bool NodeItem::windowIsDocumentThumbnail(const QFileInfo& info) const
{
    return window_->isDocumentThumbnailFile(info);
}

QPixmap NodeItem::windowDocumentThumbnail(const QFileInfo& info) const
{
    return window_->documentThumbnail(info);
}

QString NodeItem::windowMarkdownPreviewText() const
{
    return window_->inlineMarkdownPreviewText(node_);
}

void NodeItem::resizePreview(const QSizeF& size, bool preferHeight)
{
    window_->setPreviewSize(node_, size, preferHeight);
}

void NodeItem::resizeImagePreview(qreal scale)
{
    window_->setImagePreviewScale(node_, scale);
}

void NodeItem::createPreviewWidget()
{
    if (!node_->previewOpen || node_->isDir) {
        return;
    }

    const QFileInfo info(node_->path);
    if (const auto embedUrl = youtubeEmbedUrlForFile(info)) {
        const QString thumbnailPath = window_->cachedYouTubeThumbnailPathForFile(node_->path);
        if (thumbnailPath.isEmpty() || !QFileInfo::exists(thumbnailPath)) {
            return;
        }
        auto* youtubePreview = new YouTubePreview(*embedUrl, thumbnailPath);
        youtubePreview->setProperty("mycelInlinePreviewPath", node_->path);
        previewProxy_ = new QGraphicsProxyWidget(this);
        previewProxy_->setWidget(youtubePreview);
        previewProxy_->setZValue(1.0);
        syncPreviewWidgetGeometry();
        return;
    }

    if (const auto url = urlForShortcutFile(info)) {
        const QString thumbnailPath = window_->cachedUrlThumbnailPathForFile(node_->path);
        if (thumbnailPath.isEmpty() || !QFileInfo::exists(thumbnailPath)) {
            return;
        }
        auto* urlPreview = new UrlThumbnailPreview(*url, thumbnailPath);
        urlPreview->setProperty("mycelInlinePreviewPath", node_->path);
        previewProxy_ = new QGraphicsProxyWidget(this);
        previewProxy_->setWidget(urlPreview);
        previewProxy_->setZValue(1.0);
        syncPreviewWidgetGeometry();
        return;
    }

    if (isImagePreviewFile(info)) {
        return;
    }

    if (window_->isDocumentThumbnailFile(info)) {
        return;  // first-page thumbnail is painted by paintPreviewFrame, no text widget needed
    }

    if (isVideoPreviewFile(info)) {
        auto* videoPreview = new InlineVideoPreview(info.absoluteFilePath());
        videoPreview->setProperty("mycelInlinePreviewPath", node_->path);
        previewProxy_ = new QGraphicsProxyWidget(this);
        previewProxy_->setWidget(videoPreview);
        previewProxy_->setZValue(1.0);
        syncPreviewWidgetGeometry();
        return;
    }

    auto* textEdit = new QTextEdit;
    textEdit->setProperty("mycelInlinePreview", true);
    textEdit->setProperty("mycelInlinePreviewPath", node_->path);
    textEdit->setReadOnly(true);
    textEdit->setUndoRedoEnabled(false);
    textEdit->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    textEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    textEdit->setFrameStyle(0);
    textEdit->setAttribute(Qt::WA_TranslucentBackground);
    textEdit->viewport()->setAttribute(Qt::WA_TranslucentBackground);
    textEdit->viewport()->setProperty("mycelInlinePreview", true);
    textEdit->viewport()->setProperty("mycelInlinePreviewPath", node_->path);
    textEdit->viewport()->setAutoFillBackground(false);
    textEdit->document()->setDocumentMargin(8.0);
    const ThemeColors colors = currentThemeColors();
    textEdit->document()->setDefaultStyleSheet(QStringLiteral("* { color: %1; } a { color: %2; }")
                                                   .arg(cssColor(colors.inlinePreviewText),
                                                        cssColor(colors.highlight)));
    textEdit->setStyleSheet(QStringLiteral(
                                "QTextEdit { background: transparent; border: none; color: %1; "
                                "selection-background-color: %2; selection-color: %3; }")
                                .arg(cssColor(colors.inlinePreviewText),
                                     cssColor(colors.highlight),
                                     cssColor(colors.highlightedText)));

    QFont previewFont;
    if (isMarkdownPreviewFile(info)) {
        previewFont.setPointSize(10);
        textEdit->setFont(previewFont);
        const QString markdown = windowMarkdownPreviewText();
        textEdit->setMarkdown(markdown);
        if (textEdit->toPlainText().trimmed().isEmpty() && !markdown.trimmed().isEmpty()) {
            textEdit->setPlainText(markdown);
        }
    } else {
        previewFont.setPointSize(9);
        previewFont.setFamily(QStringLiteral("Menlo"));
        textEdit->setFont(previewFont);
        textEdit->setPlainText(windowInlinePreviewLines().join(QLatin1Char('\n')));
    }

    textEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(textEdit, &QWidget::customContextMenuRequested, textEdit, [textEdit](const QPoint& pos) {
        QMenu menu;
        QAction* copyAction = menu.addAction(QStringLiteral("コピー"));
        copyAction->setEnabled(textEdit->textCursor().hasSelection());
        QAction* selected = menu.exec(textEdit->mapToGlobal(pos));
        if (selected == copyAction) {
            textEdit->copy();
        }
    });

    previewProxy_ = new QGraphicsProxyWidget(this);
    previewProxy_->setWidget(textEdit);
    previewProxy_->setZValue(1.0);
    syncPreviewWidgetGeometry();
}

void NodeItem::syncPreviewWidgetGeometry()
{
    if (!previewProxy_) {
        return;
    }
    const bool imagePreview = isImagePreviewFile(QFileInfo(node_->path));
    const qreal gripInset = imagePreview ? 38.0 : 18.0;
    const QRectF contentRect = previewRect().adjusted(2.0, 2.0, -gripInset, -gripInset);
    previewProxy_->setPos(contentRect.topLeft());
    previewProxy_->resize(contentRect.size());
    if (QWidget* widget = previewProxy_->widget()) {
        widget->resize(contentRect.size().toSize());
    }
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == QGraphicsItem::ItemPositionHasChanged && node_) {
        node_->center = value.toPointF();
        if (scene()) {
            scene()->update();
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

void NodeItem::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
    if (window_->canImportExternalUrls(event->mimeData(), node_)) {
        externalDropHover_ = true;
        update();
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    event->ignore();
}

void NodeItem::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
    externalDropHover_ = false;
    update();
    event->accept();
}

void NodeItem::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
    if (window_->canImportExternalUrls(event->mimeData(), node_)) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    event->ignore();
}

void NodeItem::dropEvent(QGraphicsSceneDragDropEvent* event)
{
    externalDropHover_ = false;
    update();
    if (!window_->canImportExternalUrls(event->mimeData(), node_)) {
        event->ignore();
        return;
    }

    window_->importExternalUrlsToFolder(event->mimeData()->urls(), node_);
    event->setDropAction(Qt::CopyAction);
    event->accept();
}

void NodeItem::beginDrag(const QPointF& scenePos, Qt::KeyboardModifiers modifiers)
{
    dragStart_ = pos();
    pressStartScene_ = scenePos;
    pressModifiers_ = modifiers;
    dragMoveLogged_ = false;
    window_->recordDebugEvent(QStringLiteral("node press: %1 scene=(%2,%3)")
                                  .arg(node_ ? node_->path : QStringLiteral("(null)"))
                                  .arg(scenePos.x(), 0, 'f', 1)
                                  .arg(scenePos.y(), 0, 'f', 1));
    // A plain (no Ctrl/Meta/Shift) press on an item that is already part of a multi-selection
    // must NOT collapse the selection here — otherwise the group is lost before the drag can
    // move it. Keep the selection intact; if the gesture turns out to be a click (no drag),
    // finishDragAtScene collapses it to this single item on release.
    const bool plainPress = (modifiers & (Qt::ControlModifier | Qt::MetaModifier | Qt::ShiftModifier)) == 0;
    if (!(plainPress && window_->isMultiDragSelection(this))) {
        window_->selectNodeItem(this, modifiers);
    }
    window_->focusBoard();
    setOpacity(0.72);
    setZValue(DragLayerZ);
}

void NodeItem::finishDrag(Qt::MouseButton button, const QPointF& scenePos)
{
    finishDragAtScene(button, scenePos);
}

void NodeItem::selectForContextMenu()
{
    if (!window_) {
        return;
    }
    // Keep an existing (possibly multi-item) selection that already includes this node; otherwise
    // select just this node. Either way the highlight frame stays for the context menu.
    if (!isSelected()) {
        window_->selectNodeItem(this, false);
    }
    window_->focusBoard();
}

void NodeItem::finishDragAtScene(Qt::MouseButton button, const QPointF& scenePos)
{
    // moveDragItemsToFolder / addFileLink / reorderNodeByY can rebuild the scene, which
    // deletes every NodeItem including this one. Capture the MainWindow pointer (it
    // outlives the gesture) and the node path up front, and never touch this / window_ /
    // node_ after a call that may rebuild.
    MainWindow* window = window_;
    const QString nodePath = node_ ? node_->path : QStringLiteral("(null)");
    const bool hadDragMove = dragMoveLogged_;
    dragMoveLogged_ = false;
    const qreal mouseMoveDistance = QLineF(pressStartScene_, scenePos).length();
    if (button == Qt::LeftButton) {
        window->recordDebugEvent(QStringLiteral("node release: %1 mouseMove=%2 itemMove=%3")
                                     .arg(nodePath)
                                     .arg(mouseMoveDistance, 0, 'f', 1)
                                     .arg(QLineF(dragStart_, pos()).length(), 0, 'f', 1));
    }
    if (window->boardModeActive()) {
        // Board mode: a click selects; a drag ends as a free move whose position is persisted.
        window->setDragVisuals(this, 1.0, 10.0);
        if (!hadDragMove && mouseMoveDistance < 8.0) {
            if (button == Qt::LeftButton && scene()) {
                window->selectNodeItem(this, pressModifiers_);
                window->focusBoard();
            }
            return;
        }
        window->boardCardMoved(this);
        return;
    }

    if (!hadDragMove && mouseMoveDistance < 8.0) {
        window->clearInternalDropHover();
        window->clearLinkDropHover();
        window->clearDragPreview();
        window->setDragVisuals(this, 1.0, 10.0);
        if (button == Qt::LeftButton && scene()) {
            window->selectNodeItem(this, pressModifiers_);
            window->focusBoard();
            return;
        }
        return;
    }

    NodeItem* folderTarget = window->folderItemForDrop(this, scenePos);
    if (folderTarget) {
        window->recordDebugEvent(QStringLiteral("node drop folder target: %1 -> %2")
                                     .arg(nodePath,
                                          folderTarget->node() ? folderTarget->node()->path : QStringLiteral("(null)")));
        window->clearInternalDropHover();
        window->clearLinkDropHover();
        window->moveDragItemsToFolder(this, folderTarget->node());  // may delete this
        return;
    }

    NodeItem* linkTarget = window->linkTargetItemForDrop(this, scenePos);
    if (linkTarget) {
        window->recordDebugEvent(QStringLiteral("node drop link target: %1 -> %2")
                                     .arg(nodePath,
                                          linkTarget->node() ? linkTarget->node()->path : QStringLiteral("(null)")));
        window->clearInternalDropHover();
        window->clearLinkDropHover();
        window->clearDragPreview();
        window->addFileLink(linkTarget->node(), node_);  // may delete this
        return;
    }

    if (window->isMultiDragSelection(this)) {
        window->clearInternalDropHover();
        window->clearLinkDropHover();
        window->clearDragPreview();
        window->setDragVisuals(this, 1.0, 10.0);
        return;
    }

    window->clearInternalDropHover();
    window->clearLinkDropHover();
    const qreal dropCenterY = sceneBoundingRect().center().y();
    if (window->reorderNodeByY(node_, this, dropCenterY)) {  // may delete this
        window->recordDebugEvent(QStringLiteral("node drop reordered: %1").arg(nodePath));
        return;
    }
    window->recordDebugEvent(QStringLiteral("node drop no target: %1 scene=(%2,%3)")
                                 .arg(nodePath)
                                 .arg(scenePos.x(), 0, 'f', 1)
                                 .arg(scenePos.y(), 0, 'f', 1));
    window->clearDragPreview();
    window->setDragVisuals(this, 1.0, 10.0);
}

void NodeItem::updateDrag(const QPointF& scenePos)
{
    setPos(dragStart_ + (scenePos - pressStartScene_));
    updateDragAtScene(scenePos);
}

void NodeItem::updateDragAtScene(const QPointF& scenePos)
{
    if ((pos() - dragStart_).manhattanLength() >= 16.0) {
        if (window_->boardModeActive()) {
            return;  // board mode: pure free move — no folder/link/reorder targets
        }
        if (!dragMoveLogged_) {
            dragMoveLogged_ = true;
            window_->recordDebugEvent(QStringLiteral("node drag threshold reached: %1 posDelta=(%2,%3) scene=(%4,%5)")
                                          .arg(node_ ? node_->path : QStringLiteral("(null)"))
                                          .arg(pos().x() - dragStart_.x(), 0, 'f', 1)
                                          .arg(pos().y() - dragStart_.y(), 0, 'f', 1)
                                          .arg(scenePos.x(), 0, 'f', 1)
                                          .arg(scenePos.y(), 0, 'f', 1));
        }
        const bool multiDrag = window_->isMultiDragSelection(this);
        if (multiDrag) {
            window_->setDragVisuals(this, 0.72, 100.0);
            window_->previewDragSelection(this);
        } else if (node_->isDir) {
            window_->previewMoveDescendants(node_, pos() - layoutCenter_);
        }
        if (window_->updateInternalDropHover(this, scenePos)) {
            window_->clearLinkDropHover();
            window_->clearDragPreviewForSource(this);
            return;
        }
        if (window_->updateLinkDropHover(this, scenePos)) {
            window_->clearInternalDropHover();
            window_->clearDragPreviewForSource(this);
            return;
        }
        window_->clearLinkDropHover();
        if (multiDrag) {
            return;
        }
        window_->previewReorder(node_, this, sceneBoundingRect().center().y());
    }
}

void NodeItem::activate()
{
    QPointer<MainWindow> window = window_;
    const QString path = node_->path;
    const bool isDir = node_->isDir;
    const bool isSubRoot = node_->isSubRoot;

    window->cancelQueuedInlinePreviewToggle();
    if (scene()) {
        window->selectNodeItem(this, false);
    }

    QTimer::singleShot(0, window, [window, path, isDir, isSubRoot] {
        if (!window) {
            return;
        }
        if (isSubRoot) {
            window->switchIntoSubRoot(path);  // switch into this sub-root, recording the parent
        } else if (isDir) {
            window->toggleCollapsedPath(path);
        } else {
            window->toggleInlinePreviewPath(path);
        }
        window->focusBoard();
    });
}

// Persist warnings and worse to a small rotating log so crashes and Qt runtime problems can be
// diagnosed after the fact (%LOCALAPPDATA%/Mycel/mycel.log, rotated once past 512KB).
QtMessageHandler g_previousMessageHandler = nullptr;

void mycelMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type != QtDebugMsg && type != QtInfoMsg) {
        static QMutex mutex;
        QMutexLocker locker(&mutex);
        static QFile logFile;
        if (!logFile.isOpen()) {
            const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            if (!dir.isEmpty()) {
                QDir().mkpath(dir);
                logFile.setFileName(dir + QStringLiteral("/mycel.log"));
                if (QFileInfo(logFile.fileName()).size() > 512 * 1024) {
                    QFile::remove(logFile.fileName() + QStringLiteral(".1"));
                    QFile::rename(logFile.fileName(), logFile.fileName() + QStringLiteral(".1"));
                }
                if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
                    // Logging is best-effort; if the file can't be opened we
                    // simply skip file logging (isOpen() check below guards writes).
                }
            }
        }
        if (logFile.isOpen()) {
            const char* level = type == QtWarningMsg    ? "WARN"
                                : type == QtCriticalMsg ? "CRITICAL"
                                                        : "FATAL";
            logFile.write(QStringLiteral("%1 [%2] %3\n")
                              .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                   QLatin1String(level), message)
                              .toUtf8());
            logFile.flush();  // a fatal message must reach disk before the abort
        }
    }
    if (g_previousMessageHandler) {
        g_previousMessageHandler(type, context, message);
    }
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#if MYCEL_HAS_WEBENGINE
    if (qEnvironmentVariableIsEmpty("QTWEBENGINE_CHROMIUM_FLAGS")) {
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
                QByteArrayLiteral("--ignore-gpu-blocklist "
                                  "--enable-gpu-rasterization "
                                  "--enable-zero-copy "
                                  "--enable-accelerated-video-decode "
                                  "--disable-background-timer-throttling "
                                  "--disable-backgrounding-occluded-windows "
                                  "--disable-renderer-backgrounding"));
    }
#endif
    QStringList rawArguments;
    rawArguments.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        rawArguments << QString::fromLocal8Bit(argv[i]);
    }
    StartupOptions options = startupOptions(rawArguments);
    if (options.showVersion) {
        printVersion();
        return 0;
    }

    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Mycel"));
    app.setApplicationName(QStringLiteral("Mycel"));
    // Match assets/mycel.desktop so Wayland/X11 shells associate the window
    // with the installed desktop entry (taskbar icon, app grouping).
    app.setDesktopFileName(QStringLiteral("mycel"));
    g_previousMessageHandler = qInstallMessageHandler(mycelMessageHandler);
    app.setWindowIcon(QIcon(":/icons/mycel.png"));
    if (!resolveStartupStorageMode(options)) {
        return 0;
    }
    MainWindow window(options.rootPath, options.mycelStorageEnabled);
    window.show();
    QTimer::singleShot(0, &window, [&window] { window.activateMainWindow(); });
    QTimer::singleShot(250, &window, [&window] { window.activateMainWindow(); });
    return app.exec();
}
