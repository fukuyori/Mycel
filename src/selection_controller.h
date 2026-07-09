#pragma once

// Owns every mutation of the scene's node selection. Extracted verbatim out of main.cpp.

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
#include "node_item.h"

// Owns every mutation of the scene's node selection so the rule "what becomes selected"
// lives in one place instead of being duplicated across NodeItem and MainWindow. View
// concerns (focus, scrolling a node into view) stay in MainWindow.
class SelectionController {
public:
    explicit SelectionController(QGraphicsScene& scene) : scene_(scene) {}

    void clear() { scene_.clearSelection(); }

    // Make `item` the selection. additive keeps the current selection (Cmd/Ctrl/Shift-click).
    void select(NodeItem* item, bool additive = false)
    {
        if (!additive) {
            scene_.clearSelection();
        }
        if (item) {
            item->setSelected(true);
        }
    }

    // Select the single node with this path (clearing others). Returns it, or nullptr.
    NodeItem* selectByPath(const QString& path)
    {
        scene_.clearSelection();
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node() && nodeItem->node()->path == path) {
                nodeItem->setSelected(true);
                return nodeItem;
            }
        }
        return nullptr;
    }

    // Select every node whose path is in `paths`. Returns the first selected, or nullptr.
    NodeItem* selectByPaths(const QStringList& paths, bool additive = false)
    {
        if (!additive) {
            scene_.clearSelection();
        }
        NodeItem* first = nullptr;
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node() && paths.contains(nodeItem->node()->path)) {
                nodeItem->setSelected(true);
                if (!first) {
                    first = nodeItem;
                }
            }
        }
        return first;
    }

    // Select every visible node. Returns true if anything was selected.
    bool selectAll()
    {
        scene_.clearSelection();
        bool any = false;
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node()) {
                nodeItem->setSelected(true);
                any = true;
            }
        }
        return any;
    }

    QStringList selectedPaths() const
    {
        QStringList paths;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node()) {
                paths.append(nodeItem->node()->path);
            }
        }
        paths.removeDuplicates();
        return paths;
    }

private:
    QGraphicsScene& scene_;
};
