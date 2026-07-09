#pragma once

// The parent-child / file-link connector layer. Extracted verbatim out of main.cpp.

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
#include "tree_model.h"

class ConnectionLayerItem final : public QGraphicsItem {
public:
    ConnectionLayerItem(Node* root, const std::vector<FileLink>* links)
        : root_(root), links_(links)
    {
        setZValue(ConnectionLayerZ);
        setAcceptedMouseButtons(Qt::NoButton);
        rebuildNodeIndex();
    }

    // After a metadata-only relayout the node positions/bounds changed in place: refresh the
    // cached bounds and the path index without recreating the layer.
    void refreshGeometry()
    {
        prepareGeometryChange();
        rebuildNodeIndex();
        update();
    }

    QRectF boundingRect() const override
    {
        if (!root_) {
            return {};
        }
        return root_->subtreeBounds.adjusted(-240.0, -240.0, 240.0, 240.0);
    }

    QPainterPath shape() const override
    {
        return {};
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*) override
    {
        if (!root_) {
            return;
        }
        painter->setRenderHint(QPainter::Antialiasing, !fastCanvasRendering());
        const QRectF exposed = option ? option->exposedRect.adjusted(-180.0, -180.0, 180.0, 180.0)
                                      : boundingRect();
        drawEdges(painter, *root_, exposed);
        drawFileLinks(painter, exposed);
    }

private:
    void rebuildNodeIndex()
    {
        nodesByPath_.clear();
        if (!root_) {
            return;
        }
        indexNode(*root_);
    }

    void indexNode(const Node& node)
    {
        nodesByPath_.insert(node.path, &node);
        for (const auto& child : node.children) {
            indexNode(*child);
        }
    }

    const Node* nodeByPath(const QString& path) const
    {
        return nodesByPath_.value(path, nullptr);
    }

    void drawFileLinks(QPainter* painter, const QRectF& exposed) const
    {
        if (!root_ || !links_) {
            return;
        }

        for (const FileLink& link : *links_) {
            const Node* from = nodeByPath(link.from);
            const Node* to = nodeByPath(link.to);
            if (!from || !to || from->isDir) {  // source must be a file; target may be a folder
                continue;
            }

            const QPainterPath path = TreeLayoutEngine::fileLinkPath(*from, *to);
            // Inflate before the visibility test: a perfectly horizontal link (target on the
            // same row as its source) has a zero-height controlPointRect, and
            // QRectF::intersects() always reports false for an empty rect, which dropped the line.
            if (!path.controlPointRect().adjusted(-2.0, -2.0, 2.0, 2.0).intersects(exposed)) {
                continue;
            }
            QColor color = neutralStroke();
            color.setAlpha(connectorLineAlpha());
            painter->setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(path);
        }
    }

    bool hasIncomingFileLink(const Node& node) const
    {
        if (!links_) {
            return false;
        }
        for (const FileLink& link : *links_) {
            if (link.to == node.path) {  // a linked folder connects via the link line, not its parent edge
                return true;
            }
        }
        return false;
    }

    void drawEdges(QPainter* painter, const Node& node, const QRectF& exposed) const
    {
        if (node.children.empty()) {
            return;
        }

        for (const auto& child : node.children) {
            // File-link targets connect via the link line; skip their parent-child edge.
            if (hasIncomingFileLink(*child)) {
                drawEdges(painter, *child, exposed);
                continue;
            }
            const QPainterPath path = TreeLayoutEngine::parentChildEdgePath(node, *child);
            QColor color = neutralStroke();
            color.setAlpha(connectorLineAlpha());
            const qreal width = child->isDir ? (node.depth == 0 ? 3.0 : 2.5) : 1.5;
            painter->setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(path);
            drawEdges(painter, *child, exposed);
        }
    }

    Node* root_ = nullptr;
    const std::vector<FileLink>* links_ = nullptr;
    QHash<QString, const Node*> nodesByPath_;
};
