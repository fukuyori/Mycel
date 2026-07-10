#pragma once

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

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QString rootPath, bool mycelStorageEnabled, QWidget* parent = nullptr);


    ~MainWindow() override;


    bool mycelStorageEnabled() const;


    Node* rootNode() const;


    Node* nodeForPath(const QString& path) const;


    // Create an empty NewFile.txt (conflict-free name) in dirPath, register it, rebuild, record
    // history, and move focus to the new file. Returns the new path (empty on failure).
    QString createFileInDirectory(const QString& dirPath, const QString& afterSiblingPath = QString());


    // Create a NewFolder (conflict-free) in dirPath, register it, rebuild, record history, and
    // move focus to the new folder. Returns the new path (empty on failure).
    QString createFolderInDirectory(const QString& dirPath, const QString& afterSiblingPath = QString());


    // Create a new file next to `filePath` and link it horizontally to the right of it, recorded
    // as a single undoable action.
    void createLinkedFileBeside(const QString& filePath);


    // Thin wrappers kept for existing callers (keyboard shortcuts, selection-based creation).
    void createFolder(Node* parent);


    void createFile(Node* parent, const QString& = {});


    void createFileInFolderPath(const QString& folderPath);


    void createFolderInFolderPath(const QString& folderPath);


    void createFileInSelectedFolder();


    void createFolderInSelectedFolder();


    bool copySelectedItems();


    bool copyNode(Node* node);


    bool copyPath(const QString& path);


    bool pasteCopiedItems();


    bool focusEditorForSelectedFile();


    bool focusEditorForNode(Node* node);


    bool focusEditorForPath(const QString& path);


    void focusBoard();


    void activateMainWindow();


    void saveSideEditorFromShortcut(bool returnFocusToFile);


    QString subjectFileNameFromEditor() const;


    QString editorTextWithoutSubjectLine(const QString& text) const;


    bool toggleSelectedFilePreviews();


    bool toggleSelectedFoldersCollapsed();


    bool toggleSelectedPreviewOrCollapse();


    bool handleBoardShortcut(QKeyEvent* event);


    bool isDeleteShortcut(int key, Qt::KeyboardModifiers modifiers) const;


    bool isSelectAllShortcut(int key, Qt::KeyboardModifiers modifiers) const;


    bool selectAllVisibleNodes();


    bool textInputHasFocus() const;


    void openNode(Node* node);


    // Switch the whole board to a different root folder (used by ファイル > フォルダを開く and the
    // ファイル > 履歴から開く list). Persists view state of the current root before switching.
    void openRootFolder(const QString& dir);


    // Turn a plain folder into a new child root: create its own .mycel, record the current root as
    // its parent, and refresh so the parent now shows it as a sub-root boundary.
    void makeFolderChildRoot(const QString& folderPath);


    // Link a root living outside this tree as a door node inside `folderPath` (folder picker).
    void linkExternalRootIntoFolder(const QString& folderPath);


    // Remove the external-root link shown in `parentDirPath` pointing at `targetPath`.
    // Only the link record is removed; the linked folder itself is never touched.
    void removeExternalRootLinkAt(const QString& parentDirPath, const QString& targetPath);


    // Resolve a link's stored (possibly root-relative) target to an absolute path.
    QString resolvedExternalRootTarget(const ExternalRootLink& link) const;


    // Append the synthetic door nodes for external root links to the freshly scanned tree.
    void injectExternalRootNodes();


    static QJsonObject readJsonObjectFile(const QString& path);


    static void writeJsonObjectFile(const QString& path, const QJsonObject& object);


    // Merge a keyed object section (e.g. "colors"/"directories"/"previews") from a child .mycel file
    // into the parent's, re-keying each entry from child-relative to parent-relative.
    void mergeChildObjectSection(const QString& parentPath, const QString& childPath, const QString& section,
                                 const std::function<QString(const QString&)>& reKey);


    // Merge a string-array section (e.g. "folders") from a child .mycel file into the parent's.
    void mergeChildStringArray(const QString& parentPath, const QString& childPath, const QString& section,
                               const std::function<QString(const QString&)>& reKey);


    // Merge the "links" array (from/to pairs) from a child .mycel file into the parent's.
    void mergeChildLinks(const QString& parentPath, const QString& childPath,
                         const std::function<QString(const QString&)>& reKey);


    // Integrate a sub-root back into its parent: merge the child's metadata (colors, order, previews,
    // collapse, links) into the parent's .mycel with re-keyed paths, remove the child's own .mycel,
    // then reload so the parent shows it as a plain (expandable) folder again.
    void integrateChildRootIntoParent(const QString& childDir);


    // Switch the board into a sub-root, first recording the current root as that child's parent
    // inside the child's own .mycel so the parent bar can be restored whenever the child is opened.
    void switchIntoSubRoot(const QString& childPath);


    // Persist the parent root into <childDir>/.mycel/parent.json. The parent is stored as a path
    // relative to the child so the record survives moving the whole tree.
    void writeParentRootRecord(const QString& childDir, const QString& parentDir);


    // Read the recorded parent root of a root directory, resolved to an absolute path. Empty when no
    // record exists or the recorded parent no longer has its own .mycel.
    QString recordedParentRoot(const QString& rootDir) const;


    // The chain of parent roots from the outermost down to the immediate parent of the opened root.
    // Built by following the parent recorded in each .mycel; if none is recorded, falls back to the
    // nearest ancestor directories on disk that carry a .mycel. Empty for a top-level root.
    QStringList parentRootChain() const;


    void openPath(const QString& path);


    // Let the user choose which application opens this file. On Windows this shows the native
    // "Open with" dialog; on other platforms the user picks an application and Mycel launches it
    // with the file as an argument.
    void openWithApplication(const QString& path);


    // Shift+O: "open with" picker for the single selected file.
    bool openSelectedWithApplication();


    // Order a folder's direct children by name or by modified date. Folders are kept before files
    // (matching the default scan order); descending reverses within each group. Recorded as one
    // undoable step (it only changes the persisted custom order).
    void sortFolderChildren(const QString& folderPath, bool byDate, bool descending);


    bool isGoScriptFile(const QString& path) const;


    // Scripts Mycel can run in a pipeline (Python or Go source).
    bool isPipelineScriptFile(const QString& path) const;


    // Interpreter for a script as {program, leadingArgs}; leadingArgs precede the script path
    // (e.g. {"run"} for `go run`). An empty program means no runner is installed/known.
    std::pair<QString, QStringList> pipelineRunnerFor(const QString& scriptPath) const;


    // Run `program args...` in workingDir, streaming combined stdout/stderr into a non-modal
    // window. onFinished(exitCode) runs when the process ends (e.g. to refresh outputs); the
    // process is killed if the window closes.
    void runProcessWithOutputDialog(const QString& title, const QString& program, const QStringList& args,
                                    const QString& workingDir, std::function<void(int)> onFinished = {});


    // Run a single Go source file as a script (`go run <file>`), showing the output window.
    void runGoScript(const QString& filePath);


    // The input feeding a script = the file linked INTO it (link.to == script).
    QString pipelineInputFor(const QString& scriptPath) const;


    // The output of a script = the file linked OUT of it (link.from == script).
    QString pipelineOutputFor(const QString& scriptPath) const;


    // Create an empty output file beside the script and link script -> output, as one undo step.
    QString createPipelineOutput(const QString& scriptPath, const QString& inputPath);


    // Run a pipeline stage: the script reads the linked input file and writes the linked output
    // file, invoked as `runner script <input> <output>`. Input/output are resolved from the
    // horizontal links into/out of the script node (output auto-created if absent).
    void runPipelineForScript(const QString& scriptPath);


    bool openSelectedNode();


    bool canEditTextFile(Node* node) const;


    bool canEditTextFilePath(const QString& path) const;


    void editTextFile(Node* node);


    void editTextFilePath(const QString& path);


    // `path` is taken by value on purpose: callers pass a reference into a Node's
    // `path` member (e.g. focusEditorForNode(node->path)), and rebuild() below frees
    // the whole node tree. A copy keeps the string alive for the post-rebuild uses.
    bool openCenteredTextEditor(QString path);


    void centerTextEditorDialog(QDialog* dialog) const;


    void updateSideEditorForSelection();


    void syncEditorPaneVisibility();


    void clearSideEditor(const QString& message, const QString& status);


    void loadSideEditorFile(const QString& path);


    bool showUrlShortcutSidePreview(const QFileInfo& info);


    void loadSidePreviewFile(const QString& path);
#if MYCEL_HAS_WEBENGINE
    // QtWebEngine (Chromium) is heavy to initialize and spawns helper processes, yet it is only
    // needed to preview HTML files. Create the view on first use so launch stays fast and light
    // for the common case where no HTML file is previewed.
    QWebEngineView* ensureHtmlPreviewView();
#endif

    // Documents whose first page can be rendered to a thumbnail (currently PDF).
    bool isDocumentThumbnailFile(const QFileInfo& info) const;


    // Render the first page (PDF) or cover (EPUB) of a document to an image, at a modest
    // resolution. Empty on failure / unsupported build.
    QImage renderDocumentFirstPage(const QFileInfo& info) const;


    // Disk cache path for a document's thumbnail under .mycel/thumbnails. The key embeds the
    // source's modified time and size so a changed file produces a new thumbnail.
    // Bump this when the rendering (e.g. resolution) changes, so existing caches are invalidated.
    static constexpr int kThumbnailVersion = 400;

    QString thumbnailCachePathFor(const QFileInfo& info) const;


    QString documentThumbnailMemKey(const QFileInfo& info) const;


    // Return an already-generated thumbnail from the memory/disk cache, or null. Never renders —
    // used where we must not pay the cost of generating a preview (e.g. on mere selection).
    QPixmap cachedDocumentThumbnail(const QFileInfo& info);


    // First-page thumbnail for a document. Returns the cached image if present, otherwise renders
    // the first page and caches it on disk (.mycel/thumbnails) and in memory. Call this only when
    // the preview is actually being opened, not on selection.
    QPixmap documentThumbnail(const QFileInfo& info);


    void showSidePreviewText(const QString& text, const QString& status);


    void applyTheme(AppTheme theme, bool persist, bool refreshTree);


    void updateThemeActions();


    void applyTextPaneTheme();


    void applyRenameEditTheme(QLineEdit* edit);


    void setSidePaneMode(bool editing, const QString& detail);


    void stopSidePreviewMedia();


    bool saveSideEditorNow();


    void refreshInlinePreviewForPath(const QString& path);


    QStringList inlinePreviewLines(Node* node) const;


    bool hasSavedPreviewSizePath(const QString& path) const;


    // Drop a file's saved preview size/scale so it reverts to the automatic (default) size.
    void resetPreviewSizePath(const QString& path);


    // Batch variant for multi-selection: one history entry; returns false when nothing to reset.
    bool resetPreviewSizesForPaths(const QStringList& paths);


    QString inlineMarkdownPreviewText(Node* node) const;


    void toggleInlinePreview(Node* node);


    bool isInlinePreviewOpen(Node* node) const;


    void setInlinePreview(Node* node, bool open);


    void queueInlinePreviewToggle(Node* node);


    void queueCollapsedToggle(Node* node);


    void cancelQueuedInlinePreviewToggle();


    void toggleInlinePreviewPath(const QString& path);


    void setPreviewSize(Node* node, const QSizeF& size, bool preferHeight = false);


    void setImagePreviewScale(Node* node, qreal scale);


    void toggleCollapsed(Node* node);


    void toggleCollapsedPath(const QString& path);


    QColor colorForNode(const Node* node) const;


    QColor fillForNode(const Node* node) const;


    bool hasUserColor(Node* node) const;


    bool hasUserColorPath(const QString& path) const;


    bool hasUserColorForNode(const Node* node) const;


    bool canDeleteFolder(Node* node) const;


    void setNodeColor(Node* node, const QColor& color);


    void setNodeColorPath(const QString& path, const QColor& color);


    void clearNodeColor(Node* node);


    void clearNodeColorPath(const QString& path);


    // Batch variants for multi-selection: one history entry for the whole selection.
    void setNodeColorForPaths(const QStringList& paths, const QColor& color);


    bool clearNodeColorForPaths(const QStringList& paths);


    // Applies colorPalette()[paletteIndex] to the current selection (number-key shortcut).
    bool assignPaletteColorToSelection(int paletteIndex);


    void beginInlineRename(Node* node);


    void renameFile(Node* node);


    void renamePathInline(const QString& path);


    void renameSelectedItem();


    void showCheatSheet();


    bool renamePathTo(const QString& path, const QString& name);


    void deleteFile(Node* node);


    void deleteFilePath(const QString& path);


    void deleteFolder(Node* node);


    void deleteFolderPath(const QString& path);


    // Apply the metadata side effects of a successful move. Pure filesystem work and
    // conflict-name resolution live in FileOperationService; this updates collapsed/color/
    // link/order metadata from the returned old->new path mapping. updateOrder distinguishes
    // the historical single-move rule (files only) from the multi-move rule (files + dirs).
    void applyMovedMetadata(const FileOperationService::MovedEntry& entry, const QString& targetDirPath,
                            bool updateOrder);


    void moveNode(Node* source, Node* targetDir);


    void moveDragItemsToFolder(NodeItem* sourceItem, Node* targetDir);


    bool canImportExternalUrls(const QMimeData* mimeData, Node* targetDir) const;


    void importExternalUrlsToFolder(const QList<QUrl>& urls, Node* targetDir);


    bool hasPasteableWebUrl(const QMimeData* mimeData) const;


    bool canPasteClipboardToFolder(Node* targetDir) const;


    bool canPasteClipboardToFolderPath(const QString& folderPath) const;


    void pasteClipboardToFolder(Node* targetDir);


    void pasteClipboardToFolderPathAction(const QString& folderPath);


    NodeItem* folderItemAt(const QPointF& scenePos, const NodeItem* exclude) const;


    NodeItem* fileItemAt(const QPointF& scenePos, const NodeItem* exclude) const;


    std::vector<NodeItem*> selectedTopLevelDragItems(const NodeItem* source) const;


    bool isMultiDragSelection(const NodeItem* source) const;


    void setDragVisuals(NodeItem* source, qreal opacity, qreal zValue);


    void previewDragSelection(NodeItem* source);


    NodeItem* folderItemForDrop(const NodeItem* source, const QPointF& scenePos) const;


    NodeItem* linkTargetItemForDrop(const NodeItem* source, const QPointF& scenePos) const;


    NodeItem* nodeItemForPath(const QString& path) const;


    NodeItem* updateInternalDropHover(NodeItem* source, const QPointF& scenePos);


    NodeItem* updateLinkDropHover(NodeItem* source, const QPointF& scenePos);


    void clearInternalDropHover();


    void clearLinkDropHover();


    void resetDropHoverState();


    void addFileLink(Node* from, Node* to);


    // All link targets reachable from `sourcePath` by following from->to edges transitively.
    // A link target is displayed beside its source, so when the source moves to another folder
    // its whole chain of targets is moved with it — the visual grouping is kept physical.
    // Cycle-safe via the visited set.
    QStringList linkedDescendantPaths(const QString& sourcePath) const;


    bool hasIncomingFileLink(Node* node) const;


    bool hasIncomingFileLinkPath(const QString& path) const;


    void removeIncomingFileLinks(Node* node);


    void removeIncomingFileLinksPath(const QString& path);


    // Unlinks every listed path that is a link target, as a single history entry.
    bool removeIncomingFileLinksForPaths(const QStringList& paths);


    struct FileLinkSiblingGroup {
        int pos = -1;  // path's own position within indices, or -1 if path is not a link target
        std::vector<std::size_t> indices;  // indices into fileLinks_, in link order
    };

    // Every target sharing `path`'s link source, in link order (the order layoutFileLinks fans
    // them out in), plus `path`'s own position in that list.
    FileLinkSiblingGroup fileLinkSiblingGroup(const QString& path) const;


    // Whether `path` (a file-link target) has a sibling target in the given direction
    // (-1 = earlier/up, +1 = later/down) within the same source's link fan.
    bool canMoveFileLinkTargetPath(const QString& path, int direction) const;


    // Swaps `path` with its neighbouring target within the same link source's fan, changing the
    // stacking order that layoutFileLinks uses (link order), then re-lays out. This is the only
    // way to reorder file-link targets: they live outside fileOrders_/reorderNodeByY entirely.
    void moveFileLinkTargetPath(const QString& path, int direction);


    bool reorderNodeByY(Node* source, const NodeItem* sourceItem, qreal dropCenterY);


    // Ctrl+Up/Down: moves the selected node one step among its siblings (link targets move
    // within their link fan instead).
    bool reorderSelectedNode(int direction);


    // Ctrl+Left: moves the selected node into its grandparent folder. A link target is instead
    // reconnected one level up its link chain (or unlinked when already at the chain's top).
    bool promoteSelectedNode();


    void previewReorder(Node* source, const NodeItem* sourceItem, qreal dragCenterY);


    void previewMoveDescendants(Node* source, const QPointF& delta);


    bool shouldKeepDragPreviewItem(const NodeItem* item, const std::vector<NodeItem*>& keepItems) const;


    void clearDragPreviewForSource(const NodeItem* source);


    void clearDragPreview(const std::vector<NodeItem*>& keepItems = {});


    bool hasSelectedFiles() const;


    int selectedFileCount() const;


    int selectedDeletableItemCount() const;


    void setSelectedFilePreviews(bool open);


    QStringList selectedFilePaths() const;


    bool toggleFilePreviewsForPaths(const QStringList& paths);


    void setFilePreviewsForPaths(const QStringList& paths, bool open);


    bool openInlinePreviewPath(const QString& path);


    bool closeInlinePreviewPath(const QString& path);


    bool prepareInlinePreviewOpenPath(const QString& path);


    QString youtubeThumbnailCacheDirectoryPath() const;


    QString youtubeThumbnailCachePathForEmbedUrl(const QString& embedUrl) const;


    QString urlThumbnailCacheDirectoryPath() const;


    QString urlThumbnailCachePathForUrl(const QUrl& url) const;


    void fetchYouTubeThumbnailForInlinePreview(const QString& path, const QString& embedUrl);


    bool fetchUrlThumbnailForPreview(const QString& path, const QUrl& pageUrl, bool openInlinePreview);


    void deleteSelectedItems();


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
        std::vector<ExternalRootLink> externalRootLinks;
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

    MetadataSnapshot captureMetadataSnapshot() const;


    void restoreMetadataSnapshot(const MetadataSnapshot& s);


    // Write a card set back into a (non-active) pattern file, keeping its stored view.
    void restoreBoardCardsToPatternFile(const QString& name, const std::map<QString, BoardCardState>& cards);


    void saveAllMetadata();


    QString historyTrashDir() const;


    // The undo history (and its trashed items) lives only for the session. Wipe .mycel/trash at
    // startup and shutdown so deleted/undone items don't pile up on disk across runs.
    void cleanHistoryTrash();


    // A fresh, collision-free path under .mycel/trash holding one item by its original name, so
    // undo/redo can move it straight back to (or out of) the tree.
    QString allocateTrashPath(const QString& name);


    // On Windows QFileSystemWatcher keeps an open handle on every watched directory, which blocks
    // renaming/moving those directories ("Access is denied"). Drop all watched paths before a
    // structural filesystem move; the rebuild that follows re-establishes them via
    // resetFileSystemWatcher().
    void pauseFileSystemWatcher();


    // Move a tree item into .mycel/trash and return its new path (empty on failure). Deletes go
    // through here so they can be reversed by undo (the item is recovered, not destroyed).
    QString moveToTrash(const QString& path);


    bool applyHistoryMoves(const std::vector<std::pair<QString, QString>>& moves);


    void pushHistoryEntry(HistoryEntry entry);


    // Record one undoable action. `before` must be captured by the caller at the very start of
    // the op (while paths are still in their old form). When a group is open the moves are
    // folded into it instead of pushed, so a multi-item gesture undoes as a single step.
    void recordHistory(const QString& description,
                       std::vector<std::pair<QString, QString>> redoMoves,
                       std::vector<std::pair<QString, QString>> undoMoves,
                       const MetadataSnapshot& before, const QStringList& selectionBefore);


    void beginHistoryGroup(const QString& description);


    void endHistoryGroup();


    void performUndo();


    void performRedo();


    void updateUndoRedoActions();


    bool hasSelectedItems() const;


    QStringList selectedNodePaths() const;


    // Selection primitive shared by NodeItem (drag start, click, double-click). additive
    // keeps the current selection for Cmd/Ctrl-click.
    void selectNodeItem(NodeItem* item, bool additive);


    void selectNodeItem(NodeItem* item, Qt::KeyboardModifiers modifiers);


    void restoreFolderSelection(const QString& folderPath);


    bool selectNodePath(const QString& path, bool ensureVisible = false);


    bool restoreSelection(const QStringList& paths, bool ensureVisible = false);


    void ensureNodeItemVisible(NodeItem* item);


    void ensureNodePathVisible(const QString& path);


    bool moveSelectionWithTab(bool reverse);


    bool moveSelectionVertically(bool upward);


    // Shift+Up/Down: grows/shrinks the contiguous selection range in visible node order.
    bool extendSelectionVertically(bool upward);


    bool moveSelectionToParent();


    bool moveSelectionToFirstChild();


    Node* findVisibleNodeByPath(Node* node, const QString& path) const;


    QStringList visibleNodePathsInOrder() const;


    bool selectNodeRangeToItem(NodeItem* item, bool additive);


    Node* singleSelectedNode() const;


    void refreshNode(Node*);


    void refreshSelectedItems();


    // Loads order/color/preview/link metadata plus the hash cache for the current rootPath_, and
    // reconciles any renames that happened while this root was not open/watched (app not
    // running, or a different root was open) before refreshing the cache for the current state.
    // Order matters: reconciling first lets a vanished old path match a newly appeared path;
    // seeding the cache for the new path first would make it look already-known and hide it from
    // the "appeared" set. Callers still need to call loadCollapsedFile()/applyLargeTreeStartupCollapse()
    // and rebuild() themselves, since collapse handling interacts with each caller's own flow
    // (e.g. openRootFolder() always clears collapsedPaths_ first).
    void loadMetadataAndReconcileHashCache();


    // Placeholder shown between the (now instant) first paint and the first tree render; the
    // scene_.clear() in renderCurrentTree()/renderBoardScene() removes it.
    void showStartupLoadingIndicator();


    // Deferred startup, run shortly after the window is first shown. Splits what the
    // constructor used to do synchronously into (a) the bounded work needed to render the
    // visible tree, done here, and (b) the whole-tree walk + hash seeding, pushed to a
    // background thread (startStartupScanThread). The rename reconcile that used to precede
    // the first render now runs when that thread reports back (finishStartupScan), so a file
    // renamed externally while Mycel was closed keeps its metadata a moment later than before
    // instead of blocking the first paint on a full-tree content read.
    void completeDeferredStartup();


    void startStartupScanThread();


    // GUI-thread continuation of the startup scan: reconcile external renames against the
    // walked lists, merge the seeded hashes, and bring the filesystem watcher online with the
    // walked paths -- all without re-walking the tree on this thread.
    void finishStartupScan(const QString& scannedRootPath, const StartupScanResult& result);


    void refreshAll();


    bool isMetadataPath(const QString& path) const;


    // Decides whether the current root gets native file watching, and drops any watches left
    // over from a previous root when it does not. Called at construction and whenever
    // rootPath_ changes (open-folder navigation, archive import).
    void updateNativeWatcherModeForRoot();


    void resetFileSystemWatcher();
    void finishFileSystemWatcherReset(const QString& scannedRootPath, const TreeWalkResult& walk);


    // Differential registration: only paths that actually changed are (un)watched. The full
    // remove-all/add-all this replaces measured ~550 ms per call at 10k files on a local SSD
    // -- and it ran on every rebuild.
    void applyFileSystemWatcherPaths(const QStringList& directories, const QStringList& files);


    // A watch dies silently when its path is deleted or renamed: Qt stops monitoring but keeps
    // the path listed, so the differential apply above sees it as "already watched" and never
    // re-arms it. The old full re-register revived such watches as a side effect; this does it
    // explicitly, for just the paths that fired in the current notification batch (covers
    // delete-then-recreate of the same name).
    void reviveWatchedPaths(const QSet<QString>& paths);


    void scheduleFileSystemRefresh(const QString& path);


    QString refreshDirectoryForChangedPath(const QString& path) const;


    QString nearestVisibleDirectoryPath(QString path) const;


    QStringList pendingRefreshDirectories() const;


    // Raw changed directories for hash-cache/rename-reconciliation purposes: unlike
    // pendingRefreshDirectories(), this does not substitute collapsed directories with their
    // nearest visible ancestor, since reconciliation reads the real filesystem directly and does
    // not need a directory to be visible (or expanded) in the display tree.
    QStringList pendingChangedDirectoriesForHashSync() const;


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
    bool reconcileRenamedFiles(const QStringList& directories, bool pruneUnmatchedMetadata);


    // Core reconcile. `currentPaths` is the current file listing of `directories` -- either
    // gathered by the wrapper above or reused from the startup scan thread's walk. When
    // `precomputedHashes` is given (startup path), appeared files take their content hash from
    // it instead of being read on this (GUI) thread; the scan hashes every uncached file, so
    // an appeared file missing from it is exactly one computeFileContentHash() would also skip.
    bool reconcileRenamedFiles(const QStringList& directories, const QSet<QString>& currentPaths,
                               bool pruneUnmatchedMetadata,
                               const std::map<QString, FileHashCacheEntry>* precomputedHashes);


    void persistRenameReconciliationResults();


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
    void runBackgroundReconcile();


    // GUI-thread continuation of the periodic reconcile sweep -- mirrors finishStartupScan(),
    // just triggered by the timer instead of the initial load.
    void finishBackgroundReconcile(const QString& scannedRootPath, const StartupScanResult& result);


    // Rescan one changed directory and replace its subtree only if the displayed structure
    // actually differs. Returns Unchanged (and keeps the existing Node objects, so live
    // NodeItems stay valid) when a notification did not alter what is shown.
    SubtreeRefresh replaceScannedSubtree(const QString& directoryPath);


    void refreshFromFileSystemChange();


    void exportArchive();


    void importArchive();


    // Detecting "is this root large" requires a full recursive walk (isLargeRootTree). Persist
    // the resulting collapse set immediately so that walk runs at most once per root: after this,
    // loadCollapsedFile() finds collapsed.json and every later launch/root-switch/refresh skips
    // the walk entirely. A tree found to be small is deliberately left unsaved -- writing an
    // empty collapsed.json here would make loadCollapsedFile() report "already decided" forever,
    // so a root that grows past the threshold later would never get auto-collapsed.
    void applyLargeTreeStartupCollapse();


    bool isLargeRootTree() const;


    bool eventFilter(QObject* watched, QEvent* event) override;


    QString cachedYouTubeThumbnailPathForFile(const QString& path) const;


    QString cachedUrlThumbnailPathForFile(const QString& path) const;


protected:
    void closeEvent(QCloseEvent* event) override;


private:
    bool isInlinePreviewObject(QObject* watched) const;


    QString inlinePreviewPathForObject(QObject* watched) const;


    bool isSidePreviewObject(QObject* watched) const;


public:
    void recordDebugEvent(const QString& message);


private:
    void copyDebugPaneToClipboard();


    void refreshDebugPane(const QString& eventMessage = QString());


    QString debugObjectName(QObject* object) const;


    QString debugKeyName(QKeyEvent* event) const;


    void finishInlineRename(bool commit);


    void suspendInlineRenameActivity();


    void resumeInlineRenameActivity();


    void setVisualPosition(NodeItem* item, const QPointF& position);


    QString uniqueAssetsDirectoryName(const QDir& parentDir, const QString& preferredName) const;


    QFileInfoList archiveEntriesForDirectory(const QString& directoryPath) const;


    void addCommonArchiveMetadata(QJsonObject& object, const QString& path, int order) const;


    void appendDirectoryToArchive(const QString& directoryPath, int depth, int order, QString& output,
                                  const QString& assetsDirPath, const QString& assetsDirName,
                                  QStringList& failures, int& binaryCount);


    void appendFileToArchive(const QFileInfo& fileInfo, int depth, int order, QString& output,
                             const QString& assetsDirPath, const QString& assetsDirName,
                             QStringList& failures, int& binaryCount);


    QString archiveLinksSection() const;


    void addImportedOrderEntry(const QString& destinationRoot, const QString& path, int order,
                               std::map<QString, std::vector<ArchiveOrderEntry>>& orderEntries) const;


    void addImportedCommonMetadata(const QJsonObject& object, const QString& path, bool isDir,
                                   QSet<QString>& collapsedPaths, std::map<QString, QColor>& colors,
                                   QSet<QString>& previewPaths, std::map<QString, QSizeF>& previewSizes) const;


    bool archivePathForObject(const QJsonObject& object, QString& relativePath, QStringList& failures) const;


    void importArchiveFolder(const QJsonObject& object, const QString& destinationRoot,
                             QSet<QString>& collapsedPaths, std::map<QString, QColor>& colors,
                             std::map<QString, std::vector<ArchiveOrderEntry>>& orderEntries,
                             QStringList& failures, int& importedCount);


    bool readArchiveTextBlock(const QStringList& lines, int& index, QString& text, QStringList& failures) const;


    void importArchiveFile(const QJsonObject& object, const QStringList& lines, int& index, const QDir& archiveDir,
                           const QString& destinationRoot, std::map<QString, QColor>& colors,
                           QSet<QString>& previewPaths, std::map<QString, QSizeF>& previewSizes,
                           std::map<QString, std::vector<ArchiveOrderEntry>>& orderEntries,
                           QStringList& failures, int& importedCount);


    void importArchiveLinks(const QStringList& lines, int& index, const QString& destinationRoot,
                            std::vector<FileLink>& links, QStringList& failures) const;


    void removeDeletedPathMetadata(const QString& deletedPath, bool wasDir);


    // Register a freshly created item in the directory's custom order. When `afterName` names an
    // existing sibling, the new item is placed directly after it; otherwise it is appended.
    void appendCreatedItemToOrder(const QString& directoryPath, const QString& newName, bool isDir,
                                  const QString& afterName = QString());


    QString availableImportPath(const QString& targetDirectoryPath, const QString& preferredName, bool isDirectory,
                                QSet<QString>* reservedNames = nullptr) const;


    bool writeBytes(const QString& path, const QByteArray& bytes) const;


    std::optional<QString> pasteableBinaryFormat(const QMimeData* mimeData) const;


    QString extensionForMimeFormat(const QString& format) const;


    bool copyDirectoryRecursively(const QString& sourcePath, const QString& destinationPath) const;


    void rekeyPathMetadataAfterRename(const QString& oldPath, const QString& newPath, bool wasDir);


    QString rekeyPathAfterRename(const QString& path, const QString& oldPath, const QString& newPath, bool wasDir) const;


    QString orderKeyForDirectory(const QString& directoryPath) const;


    QString orderFilePath() const;


    QString colorFilePath() const;


    QString previewFilePath() const;


    QString linkFilePath() const;


    QString externalRootsFilePath() const;


    QString collapsedFilePath() const;


    QString viewStateFilePath() const;


    QString hashCacheFilePath() const;


    QString relativeKeyForPath(const QString& path) const;


    QString absolutePathForKey(const QString& key) const;


    void loadOrderFile();


    void saveOrderFile();


    void loadColorFile();


    void saveColorFile();


    void loadHashCacheFile();


    void saveHashCacheFile();


    void loadPreviewFile();


    void savePreviewFile();


    bool loadCollapsedFile();


    void saveCollapsedFile();


    void loadLinkFile();


    void saveLinkFile();


    void loadExternalRootsFile();


    void saveExternalRootsFile();


    QJsonObject currentWindowStateObject() const;


    QJsonObject currentViewStateObject() const;


    std::optional<QJsonObject> loadViewStateObject() const;


    bool readViewStateObject(const QJsonObject& rootObject, QTransform& transform, int& horizontal, int& vertical,
                             QPointF& center, bool& hasCenter) const;


    bool loadViewState(QTransform& transform, int& horizontal, int& vertical,
                       QPointF& center, bool& hasCenter) const;


    void saveViewState();


    void restoreWindowStateFromSettingsFile();


    // Restacks existing NodeItems (paint order) to match top-to-bottom visual position rather
    // than tree/scan insertion order, so that when a file link's fan happens to overlap a
    // neighbouring node — e.g. a tall open preview spilling past the row it was given — the
    // lower node still paints on top instead of the stacking order being an accident of
    // file-system scan order. Safe to call any time node centers have just been recomputed;
    // does nothing to items sharing a center.y() (their relative order is left as-is).
    void restackNodeItemsByVisualOrder();


    void scheduleViewStateSave();


    void renderCurrentTree(bool fitAfterRender);


    // Render the chain of parent .mycel roots as folder nodes to the left of the root, each joined to
    // the node on its right by a connector. Returns their combined scene rect (empty for a top root)
    // so the caller can extend the scene bounds to include them.
    QRectF addParentRootItems();


    // ==== Board mode (free-form card layout, saved as patterns) ============================
    // See docs/board-mode-plan.ja.md. One pattern = one JSON under .mycel/boards/. Only cards
    // that are placed (visible) and whose real file exists are shown; hidden and new files stay
    // off-screen until called in (Phase 2 list). Positions are free (no snap, unbounded area).
public:
    bool boardModeActive() const;


    QString boardsDirPath() const;


    QString boardPatternFilePath(const QString& name) const;


    QStringList availableBoardPatternNames() const;


    // Sort directory entries with the same custom order the tree uses (fileOrders_).
    void applyCustomEntryOrder(QFileInfoList& entries, const QString& dirPath) const;


    // All board candidate files as (absolute path, row): .mycel excluded, sub-root boundaries
    // respected, collapse state ignored. One row per FOLDER: the root's direct files form the
    // first row, then each folder (level by level, in tree order) starts a new row with its
    // direct files laid left-to-right in the folder's custom order.
    std::vector<std::pair<QString, int>> collectBoardFiles() const;


    // A flat display node for one board card (same sizing rules as tree file nodes).
    std::unique_ptr<Node> makeBoardNode(const QString& path, const QPointF& center) const;


    // Create a new pattern from the current files: one row per folder, left-to-right, all visible.
    // Cards in a row share the same TOP line, and the x-advance uses the full visual width
    // (header + open preview) so nothing overlaps and the gaps stay even.
    void createBoardPattern(const QString& name);


    void saveBoardPattern();


    bool loadBoardPattern(const QString& name);


    void ensureBoardPattern();


    // Rebuild the scene from the active pattern: placed+visible cards whose file exists.
    void renderBoardScene(bool restoreView);


    void restoreBoardViewOrFit();


    // Metadata refresh for board cards (preview open/close/size) without touching positions.
    void refreshBoardCards();


    // A board card finished a free-move drag: persist its new position.
    void boardCardMoved(NodeItem* item);


    // Hide a card from the active pattern (the file itself is untouched).
    void hideBoardCard(const QString& path);


    // Switch between the tree and the board. persistCurrent=false is used at startup where the
    // outgoing state must not be overwritten.
    void setBoardMode(bool on, bool persistCurrent = true);


    void switchBoardPattern(const QString& name);


    void createBoardPatternInteractive();


    void renameBoardPatternInteractive();


    void deleteBoardPatternInteractive();


    // List the cards that are hidden or not yet placed (e.g. files added after the pattern was
    // created) and place the selected ones: hidden cards return to their remembered position,
    // new cards appear at the current view centre.
    void showBoardHiddenCardsDialog();


    void updateBoardPatternMenu();


    // ==== end board mode ====================================================================
private:
    // Refreshes the cached content hash for one real file (used for external-rename detection),
    // skipping large files and cloud placeholders. Reuses the cached hash when size and mtime
    // are unchanged, so unchanged files are not re-read on every scan. Returns true if the entry
    // was added or updated, so the caller knows whether to persist the cache.
    bool updateHashCacheEntryForFile(const QFileInfo& info);


    // Immediately hashes and persists a single file's cache entry if it is not already cached.
    // Called when the user attaches metadata (color, etc.) to a file: that is a strong signal
    // the file is worth tracking, and waiting for the next directory scan or the periodic
    // background sweep to get to it would leave a window where an external rename right after
    // tagging could not be reconciled (no "before" hash would exist to match against).
    void ensureHashCachedForRenameTracking(const QString& path);


    // Refreshes hash-cache entries for the real, immediate files of one directory (not the
    // display tree, so this works the same whether or not the directory is collapsed/visible).
    bool updateHashCacheForDirectory(const QString& directoryPath);


    void rebuild(bool fitAfterRebuild);


    // Re-apply metadata-derived display fields (preview open state and size) to the cached tree.
    void refreshCachedNodeMetadata(Node& node);


    // Metadata-only re-render: preview open/close/size and link changes do not alter the file
    // structure, so re-layout the cached tree instead of re-stat'ing the whole directory tree
    // from disk (rebuild). The layout is recomputed on the same Node objects and the existing
    // NodeItems are moved in place — no scene rebuild, so the selection and inline preview
    // widgets of untouched nodes survive. Falls back to a full re-render if any node has no
    // live item.
    void relayout();


    void scheduleRestoreViewStateOrFit();


    void restoreViewStateOrFit();


    void reapplyRestoredViewCenter(int generation);


    void fitToMap();


    void applyEditorPanePosition(QString position, bool persist);


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
    // Mirrors the same pattern for resetFileSystemWatcher(): that used to walk the tree
    // synchronously on the GUI thread on every rebuild/board-mode toggle/rename resume.
    bool fileWatcherResetPending_ = false;
    QThread* fileWatcherResetThread_ = nullptr;
    std::shared_ptr<std::atomic_bool> fileWatcherResetCancelled_;
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
    QString selectionRangeCursorPath_;  // moving end of the keyboard range selection
    QSplitter* editorSplitter_ = nullptr;
    BoardView* view_ = nullptr;
    QDockWidget* debugDock_ = nullptr;
    QPlainTextEdit* debugText_ = nullptr;
    QString dragHoverFolderPath_;
    QString dragHoverLinkTargetPath_;
    QStringList copiedPaths_;
    std::vector<FileLink> fileLinks_;
    std::vector<ExternalRootLink> externalRootLinks_;

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
    std::unique_ptr<Node> root_;};
