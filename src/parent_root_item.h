#pragma once

// The folder-style node representing a parent .mycel root, shown to the left of the current root.
// Extracted verbatim out of main.cpp.

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

// A folder-style node shown to the left of the root that represents a parent .mycel root. Rendered
// identically to a sub-root (folder + Mycel badge + name) and scales with zoom like any node, so the
// parent (seen from the child) matches a child (seen from the parent). Single click selects
// (highlights); double click switches up to that parent root.
class ParentRootItem final : public QGraphicsItem {
public:
    ParentRootItem(QString path, QString name)
        : path_(std::move(path)), name_(std::move(name))
    {
        setFlag(ItemIsSelectable, true);
        setZValue(NodeLayerZ);
        QFont font;
        font.setPointSize(12);
        const QFontMetricsF metrics(font);
        const qreal width = std::max(128.0, metrics.horizontalAdvance(shortLabel(name_)) + 46.0 + 34.0);
        box_ = QRectF(-width / 2.0, -23.0, width, 46.0);
    }

    void setActivateHandler(std::function<void(QString)> handler) { handler_ = std::move(handler); }
    qreal boxWidth() const { return box_.width(); }

    QRectF boundingRect() const override { return box_.adjusted(-8.0, -8.0, 8.0, 8.0); }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        const ThemeColors colors = currentThemeColors();
        painter->setRenderHint(QPainter::Antialiasing, !fastCanvasRendering());
        if (isSelected()) {
            painter->setPen(QPen(colors.nodeSelectedBorder, 2.0));
            painter->setBrush(colors.nodeSelectedFill);
            painter->drawRoundedRect(box_.adjusted(-4.0, -3.0, 4.0, 3.0), 8.0, 8.0);
        }
        painter->drawPixmap(QPointF(box_.left() + 15.0, -18.0), subRootFolderPixmap());
        QFont font = painter->font();
        font.setPointSize(12);
        painter->setFont(font);
        painter->setPen(colors.nodeText);
        painter->drawText(QRectF(box_.left() + 66.0, box_.top(), box_.width() - 76.0, box_.height()),
                          Qt::AlignVCenter | Qt::AlignLeft, shortLabel(name_));
    }

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) override
    {
        if (handler_) {
            handler_(path_);
        }
    }

private:
    QString path_;
    QString name_;
    QRectF box_;
    std::function<void(QString)> handler_;
};
