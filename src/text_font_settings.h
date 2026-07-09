#pragma once

// Shared editor/preview font-size settings, plus the Ctrl+wheel / Ctrl+/- zoom handlers.
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

class TextFontSettings {
public:
    enum class Surface {
        Editor,
        Preview,
    };

    static int configuredPointSize(Surface surface)
    {
        QSettings settings;
        const QString key = surface == Surface::Editor
                                ? QStringLiteral("editor/fontPointSize")
                                : QStringLiteral("preview/fontPointSize");
        return std::clamp(settings.value(key, settings.value(QStringLiteral("editor/fontPointSize"), DefaultPointSize)).toInt(),
                          MinPointSize,
                          MaxPointSize);
    }

    static QFont editorFont(int pointSize)
    {
        QFont font(QStringLiteral("Consolas"));
        font.setStyleHint(QFont::Monospace);
        font.setPointSize(pointSize);
        return font;
    }

    static QFont previewFont(int pointSize)
    {
        QFont font(QStringLiteral("Meiryo"));
        font.setStyleHint(QFont::SansSerif);
        font.setPointSize(pointSize);
        return font;
    }

    static void registerEditor(QPlainTextEdit* editor)
    {
        editors().push_back(editor);
        applyToEditor(editor);
    }

    static void unregisterEditor(QPlainTextEdit* editor)
    {
        auto& registered = editors();
        registered.erase(std::remove(registered.begin(), registered.end(), editor), registered.end());
    }

    static void registerPreview(QTextEdit* preview)
    {
        previews().push_back(preview);
        applyToPreview(preview);
    }

    static void unregisterPreview(QTextEdit* preview)
    {
        auto& registered = previews();
        registered.erase(std::remove(registered.begin(), registered.end(), preview), registered.end());
    }

    static void changeConfiguredPointSize(Surface surface, int delta)
    {
        setConfiguredPointSize(surface, configuredPointSize(surface) + delta);
    }

    static void resetConfiguredPointSize(Surface surface)
    {
        setConfiguredPointSize(surface, DefaultPointSize);
    }

private:
    static constexpr int DefaultPointSize = 10;
    static constexpr int MinPointSize = 7;
    static constexpr int MaxPointSize = 32;

    static std::vector<QPlainTextEdit*>& editors()
    {
        static std::vector<QPlainTextEdit*> registered;
        return registered;
    }

    static std::vector<QTextEdit*>& previews()
    {
        static std::vector<QTextEdit*> registered;
        return registered;
    }

    static void setConfiguredPointSize(Surface surface, int pointSize)
    {
        const int clampedPointSize = std::clamp(pointSize, MinPointSize, MaxPointSize);
        QSettings settings;
        settings.setValue(surface == Surface::Editor
                              ? QStringLiteral("editor/fontPointSize")
                              : QStringLiteral("preview/fontPointSize"),
                          clampedPointSize);
        settings.sync();
        if (surface == Surface::Editor) {
            for (QPlainTextEdit* editor : editors()) {
                applyToEditor(editor);
            }
        } else {
            for (QTextEdit* preview : previews()) {
                applyToPreview(preview);
            }
        }
    }

    static void applyToEditor(QPlainTextEdit* editor)
    {
        if (editor) {
            editor->setFont(editorFont(configuredPointSize(Surface::Editor)));
        }
    }

    static void applyToPreview(QTextEdit* preview)
    {
        if (preview) {
            preview->setFont(previewFont(configuredPointSize(Surface::Preview)));
        }
    }
};

bool handleTextZoomKey(QKeyEvent* event, TextFontSettings::Surface surface)
{
    if (!event || !(event->modifiers() & Qt::ControlModifier)) {
        return false;
    }

    switch (event->key()) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        TextFontSettings::changeConfiguredPointSize(surface, 1);
        event->accept();
        return true;
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        TextFontSettings::changeConfiguredPointSize(surface, -1);
        event->accept();
        return true;
    case Qt::Key_0:
        TextFontSettings::resetConfiguredPointSize(surface);
        event->accept();
        return true;
    default:
        return false;
    }
}

bool handleTextZoomGesture(QEvent* event, qreal& accumulatedZoom, TextFontSettings::Surface surface)
{
    if (!event || event->type() != QEvent::NativeGesture) {
        return false;
    }

    auto* gesture = static_cast<QNativeGestureEvent*>(event);
    if (gesture->gestureType() != Qt::ZoomNativeGesture) {
        return false;
    }

    accumulatedZoom += gesture->value();
    if (std::abs(accumulatedZoom) >= 0.08) {
        TextFontSettings::changeConfiguredPointSize(surface, accumulatedZoom > 0.0 ? 1 : -1);
        accumulatedZoom = 0.0;
    }
    event->accept();
    return true;
}
