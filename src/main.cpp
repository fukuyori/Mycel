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

#include "main_window.h"

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
    auto addColorMenu = [window](QMenu& parentMenu, std::vector<QAction*>& colorActions, QAction*& clearColorAction,
                                 bool clearEnabled) {
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
        clearColorAction->setEnabled(clearEnabled);
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
        addColorMenu(menu, colorActions, clearColorAction, window->hasUserColorPath(itemPath));
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
        const QStringList selectedPaths = window->selectedNodePaths();
        bool anyLinked = false;
        bool anyColored = false;
        for (const QString& path : selectedPaths) {
            anyLinked = anyLinked || window->hasIncomingFileLinkPath(path);
            anyColored = anyColored || window->hasUserColorPath(path);
        }
        bool anySavedPreviewSize = false;
        for (const QString& path : selectedFilePaths) {
            anySavedPreviewSize = anySavedPreviewSize || window->hasSavedPreviewSizePath(path);
        }

        QAction* previewSelectionAction = menu.addAction(QStringLiteral("プレビューを開く/閉じる"));
        previewSelectionAction->setEnabled(!selectedFilePaths.isEmpty());
        QAction* copySelectionAction = menu.addAction(QStringLiteral("コピー"));
        QAction* deleteSelectionAction = menu.addAction(QStringLiteral("削除"));
        menu.addSeparator();
        QAction* unlinkSelectionAction = menu.addAction(QStringLiteral("関連を解除"));
        unlinkSelectionAction->setEnabled(anyLinked);
        QAction* resetPreviewSizeSelectionAction = menu.addAction(QStringLiteral("プレビューサイズを初期化"));
        resetPreviewSizeSelectionAction->setEnabled(anySavedPreviewSize);
        std::vector<QAction*> colorActions;
        QAction* clearColorAction = nullptr;
        addColorMenu(menu, colorActions, clearColorAction, anyColored);

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
        } else if (selected == unlinkSelectionAction) {
            QTimer::singleShot(0, window, [window, selectedPaths] {
                if (window) {
                    window->removeIncomingFileLinksForPaths(selectedPaths);
                }
            });
        } else if (selected == resetPreviewSizeSelectionAction) {
            QTimer::singleShot(0, window, [window, selectedFilePaths] {
                if (window) {
                    window->resetPreviewSizesForPaths(selectedFilePaths);
                }
            });
        } else if (selected && selected->data().canConvert<QColor>()) {
            const QColor color = selected->data().value<QColor>();
            QTimer::singleShot(0, window, [window, selectedPaths, color] {
                if (window) {
                    window->setNodeColorForPaths(selectedPaths, color);
                }
            });
        } else if (selected && selected == clearColorAction) {
            QTimer::singleShot(0, window, [window, selectedPaths] {
                if (window) {
                    window->clearNodeColorForPaths(selectedPaths);
                }
            });
        }
        return;
    }

    if (node_->isDir) {
        const QString folderPath = itemPath;
        const bool isRootFolder = node_ == window->rootNode();
        if (node_->isExternalRoot) {
            // A linked external root: only navigation and unlinking. The real folder is
            // never renamed/moved/deleted from this side.
            const QString doorParentPath = node_->parentPath;
            QAction* openRootAction = menu.addAction(QStringLiteral("このルートを開く"));
            menu.addSeparator();
            QAction* unlinkAction = menu.addAction(QStringLiteral("外部ルートのリンクを解除"));
            QAction* doorSelected = menu.exec(screenPos);
            if (doorSelected == openRootAction) {
                QTimer::singleShot(0, window, [window, folderPath] {
                    if (window) {
                        window->switchIntoSubRoot(folderPath);
                    }
                });
            } else if (doorSelected == unlinkAction) {
                QTimer::singleShot(0, window, [window, doorParentPath, folderPath] {
                    if (window) {
                        window->removeExternalRootLinkAt(doorParentPath, folderPath);
                    }
                });
            }
            return;
        }
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
        addColorMenu(menu, colorActions, clearColorAction, window->hasUserColorPath(itemPath));
        menu.addSeparator();
        // Group 5: root management
        QAction* makeChildRootAction = menu.addAction(QStringLiteral("子ルートにする（.mycel を作成）"));
        makeChildRootAction->setEnabled(!isRootFolder && window->mycelStorageEnabled());
        QAction* linkExternalRootAction = menu.addAction(QStringLiteral("外部ルートをリンク…"));
        linkExternalRootAction->setEnabled(window->mycelStorageEnabled());
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
        if (selected == linkExternalRootAction) {
            QTimer::singleShot(0, window, [window, folderPath] {
                if (window) {
                    window->linkExternalRootIntoFolder(folderPath);
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
        addColorMenu(menu, colorActions, clearColorAction, window->hasUserColorPath(itemPath));
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
