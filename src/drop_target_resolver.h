#pragma once

// Pure hit-testing over the scene for drag-and-drop: what folder a drop would move into, or
// what file it would link beside. Extracted verbatim out of main.cpp.

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
#include "node_item.h"

// Decides what a drag would drop onto (a folder to move into, or a file to link to)
// from the scene contents and the pointer position. Pure hit-testing over the scene;
// it holds no drag state and performs no mutation, so the gesture orchestration in
// BoardView / NodeItem only has to ask "what is under here" rather than reimplement it.
class DropTargetResolver {
public:
    // The folder node the dragged items would move into, or nullptr. dragItems are the
    // items being moved (excluded as candidates, along with their descendants).
    static NodeItem* folderTarget(const QGraphicsScene& scene, const NodeItem* source,
                                  const std::vector<NodeItem*>& dragItems, const QPointF& scenePos)
    {
        if (!source) {
            return nullptr;
        }

        const auto isMovingItem = [&dragItems](const NodeItem* item) {
            if (!item || !item->node()) {
                return false;
            }
            for (const NodeItem* dragItem : dragItems) {
                if (!dragItem || !dragItem->node()) {
                    continue;
                }
                if (item == dragItem || isDescendantPath(dragItem->node()->path, item->node()->path)) {
                    return true;
                }
            }
            return false;
        };

        constexpr qreal FolderDropIntentMargin = 28.0;
        for (QGraphicsItem* item : scene.items(scenePos)) {
            auto* candidate = dynamic_cast<NodeItem*>(item);
            if (!candidate || candidate == source || !candidate->node()->isDir) {
                continue;
            }
            if (!isMovingItem(candidate)) {
                return candidate;
            }
        }

        const QRectF dropRect = source->sceneBoundingRect().adjusted(-10.0, -10.0, 10.0, 10.0);
        NodeItem* best = nullptr;
        qreal bestOverlap = 0.0;
        const QRectF intentRect = dropRect.united(QRectF(scenePos, QSizeF(1.0, 1.0))
                                                      .adjusted(-FolderDropIntentMargin, -FolderDropIntentMargin,
                                                                FolderDropIntentMargin, FolderDropIntentMargin));
        for (QGraphicsItem* item : scene.items(intentRect, Qt::IntersectsItemBoundingRect)) {
            auto* candidate = dynamic_cast<NodeItem*>(item);
            if (!candidate || candidate == source || !candidate->node()->isDir) {
                continue;
            }
            if (isMovingItem(candidate)) {
                continue;
            }

            const QRectF overlap = dropRect.intersected(candidate->sceneBoundingRect());
            qreal area = overlap.width() * overlap.height();
            if (candidate->sceneBoundingRect().adjusted(-FolderDropIntentMargin, -FolderDropIntentMargin,
                                                        FolderDropIntentMargin, FolderDropIntentMargin)
                    .contains(scenePos)) {
                area += 1.0e9;
            }
            if (area > bestOverlap) {
                bestOverlap = area;
                best = candidate;
            }
        }
        return best;
    }

    // The file node the dragged item would be linked beside, or nullptr. Links require metadata
    // storage, a single dragged source (file or folder), and a non-directory link target (the file
    // whose right edge is hovered). The dragged item is placed to the right of that file.
    static NodeItem* linkTarget(const QGraphicsScene& scene, const NodeItem* source,
                                bool multiDrag, bool mycelStorageEnabled, const QPointF& scenePos)
    {
        constexpr qreal LinkDropIntentMargin = 72.0;
        if (!mycelStorageEnabled || !source || !source->node() || multiDrag) {
            return nullptr;
        }

        for (QGraphicsItem* item : scene.items()) {
            auto* candidate = dynamic_cast<NodeItem*>(item);
            if (!candidate || candidate == source || !candidate->node() || candidate->node()->isDir) {
                continue;  // the link anchor (right-edge target) is always a file
            }
            if (candidate->containsLinkDropScenePoint(scenePos, LinkDropIntentMargin)) {
                return candidate;
            }
        }
        return nullptr;
    }
};
