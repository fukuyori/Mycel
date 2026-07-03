#include <QtCore/QDir>
#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QEvent>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QDirIterator>
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


constexpr int MaxDepth = 5;
constexpr int MaxChildren = 90;
constexpr int LargeTreeAutoCollapseFileThreshold = 500;
constexpr qreal XStep = 260.0;
constexpr qreal ParentChildGap = 80.0;
constexpr qreal YStep = 72.0;
constexpr qreal FreeCanvasMargin = 12000.0;

QString appVersion()
{
    return QStringLiteral(MYCEL_VERSION);
}

QString appDisplayName()
{
    return QStringLiteral("Mycel %1").arg(appVersion());
}

QString windowTitleForRoot(const QString& rootPath, bool mycelStorageEnabled)
{
    if (mycelStorageEnabled) {
        return QStringLiteral("%1 - %2").arg(appDisplayName(), rootPath);
    }
    return QStringLiteral("%1 - %2 [--no-mycel]").arg(appDisplayName(), rootPath);
}

void printVersion()
{
    const QString line = QStringLiteral("Mycel %1\n").arg(appVersion());
#ifdef Q_OS_WIN
    const auto writeLineToHandle = [&line](HANDLE output) {
        if (output == nullptr || output == INVALID_HANDLE_VALUE) {
            return false;
        }
        const QByteArray data = line.toLocal8Bit();
        DWORD written = 0;
        return WriteFile(output, data.constData(), static_cast<DWORD>(data.size()), &written, nullptr) != 0 &&
               written == static_cast<DWORD>(data.size());
    };

    if (writeLineToHandle(GetStdHandle(STD_OUTPUT_HANDLE))) {
        return;
    }
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        writeLineToHandle(GetStdHandle(STD_OUTPUT_HANDLE));
        FreeConsole();
        return;
    }
#endif
    QTextStream stream(stdout);
    stream << line;
    stream.flush();
}

struct Node {
    QString path;
    QString parentPath;
    QString name;
    bool isDir = false;
    bool collapsed = false;
    bool isSubRoot = false;  // a child directory that has its own .mycel: shown as a boundary node
    int depth = 0;
    int branch = -1;
    int hiddenChildren = 0;
    bool previewOpen = false;
    QPointF center;
    QSizeF size;
    QSizeF previewSize;
    QRectF subtreeBounds;
    std::vector<std::unique_ptr<Node>> children;
};

struct FileLink {
    QString from;
    QString to;
};

struct ArchiveOrderEntry {
    int order = 0;
    QString name;
};

enum class AppTheme {
    Light,
    Dark,
};

struct ThemeColors {
    QColor window;
    QColor windowText;
    QColor base;
    QColor alternateBase;
    QColor text;
    QColor button;
    QColor buttonText;
    QColor highlight;
    QColor highlightedText;
    QColor canvasBackground;
    QColor nodeText;
    QColor nodeSelectedBorder;
    QColor nodeSelectedFill;
    QColor nodeStroke;
    QColor nodeFill;
    QColor previewPanel;
    QColor previewPanelBorder;
    QColor previewTextBackground;
    QColor previewTextBorder;
    QColor previewText;
    QColor editPanel;
    QColor editPanelBorder;
    QColor editTextBackground;
    QColor editTextBorder;
    QColor editText;
    QColor inlinePreviewBackground;
    QColor inlinePreviewText;
    QColor inlinePreviewBorder;
    QColor badgeBackground;
    QColor badgeBorder;
    QColor badgeText;
    QColor filePage;
    QColor fileStroke;
    QColor fileInk;
    QColor linkAccent;
};

ThemeColors themeColors(AppTheme theme)
{
    if (theme == AppTheme::Dark) {
        return {
            QColor("#151a1e"),
            QColor("#e6edf3"),
            QColor("#1f252b"),
            QColor("#262d34"),
            QColor("#e6edf3"),
            QColor("#2a323a"),
            QColor("#e6edf3"),
            QColor("#4da3ff"),
            QColor("#06111f"),
            QColor("#11161b"),
            QColor("#e6edf3"),
            QColor("#4da3ff"),
            QColor(77, 163, 255, 48),
            QColor("#89939c"),
            QColor("#20272d"),
            QColor("#162234"),
            QColor("#4da3ff"),
            QColor("#172331"),
            QColor("#4d7fab"),
            QColor("#e6edf3"),
            QColor("#14251e"),
            QColor("#46a37b"),
            QColor("#14241d"),
            QColor("#46a37b"),
            QColor("#e6edf3"),
            QColor("#1b232b"),
            QColor("#e6edf3"),
            QColor("#59636d"),
            QColor("#3a2f16"),
            QColor("#9c7b2c"),
            QColor("#f5d67d"),
            QColor("#20272d"),
            QColor("#87929c"),
            QColor("#cdd6df"),
            QColor("#4cc58a"),
        };
    }

    return {
        QColor("#f6f8fa"),
        QColor("#172321"),
        QColor("#ffffff"),
        QColor("#f1f5f8"),
        QColor("#172321"),
        QColor("#f5f7f9"),
        QColor("#172321"),
        QColor("#2f7de1"),
        QColor("#ffffff"),
        QColor("#fdfcf8"),
        QColor("#172321"),
        QColor("#0b63ce"),
        QColor("#dbeafe"),
        QColor("#879198"),
        QColor("#f7fafb"),
        QColor("#eef6ff"),
        QColor("#2f7de1"),
        QColor("#f7fbff"),
        QColor("#b7d4f2"),
        QColor("#172321"),
        QColor("#edf7f1"),
        QColor("#2f8f68"),
        QColor("#f4fbf7"),
        QColor("#2f8f68"),
        QColor("#172321"),
        QColor("#ffffff"),
        QColor("#243036"),
        QColor("#c4ccd1"),
        QColor("#fff6dd"),
        QColor("#9b7a32"),
        QColor("#6f561f"),
        QColor("#ffffff"),
        QColor("#879198"),
        QColor("#59666d"),
        QColor("#0f9f6e"),
    };
}

AppTheme appThemeFromString(const QString& value)
{
    return value.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0 ? AppTheme::Dark : AppTheme::Light;
}

QString appThemeToString(AppTheme theme)
{
    return theme == AppTheme::Dark ? QStringLiteral("dark") : QStringLiteral("light");
}

AppTheme currentAppTheme()
{
    if (qApp && qApp->property("mycelTheme").toString() == QStringLiteral("dark")) {
        return AppTheme::Dark;
    }
    return AppTheme::Light;
}

ThemeColors currentThemeColors()
{
    return themeColors(currentAppTheme());
}

QString cssColor(const QColor& color)
{
    return color.name(QColor::HexRgb);
}

QColor neutralStroke()
{
    return currentAppTheme() == AppTheme::Dark ? QColor("#aeb9c6") : QColor("#5f6f79");
}

int connectorLineAlpha()
{
    return currentAppTheme() == AppTheme::Dark ? 220 : 205;
}

constexpr qreal ConnectionLayerZ = 0.0;
constexpr qreal NodeLayerZ = 10.0;
constexpr qreal DragLayerZ = 100.0;
constexpr qreal RenameLayerZ = 300.0;

bool g_fastCanvasRendering = false;

bool fastCanvasRendering()
{
    return g_fastCanvasRendering;
}

QColor neutralFill()
{
    return currentThemeColors().nodeFill;
}

QColor softFillFromColor(QColor color)
{
    color.setAlpha(currentAppTheme() == AppTheme::Dark ? 72 : 42);
    return color;
}

std::vector<std::pair<QString, QColor>> colorPalette()
{
    return {
        {QStringLiteral("青"), QColor(QStringLiteral("#2563eb"))},
        {QStringLiteral("水色"), QColor(QStringLiteral("#0891b2"))},
        {QStringLiteral("緑"), QColor(QStringLiteral("#16a34a"))},
        {QStringLiteral("黄"), QColor(QStringLiteral("#ca8a04"))},
        {QStringLiteral("橙"), QColor(QStringLiteral("#ea580c"))},
        {QStringLiteral("赤"), QColor(QStringLiteral("#dc2626"))},
        {QStringLiteral("紫"), QColor(QStringLiteral("#9333ea"))},
        {QStringLiteral("灰"), QColor(QStringLiteral("#64748b"))},
    };
}

QIcon colorDotIcon(const QColor& color)
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(QRectF(3.0, 3.0, 10.0, 10.0));
    return QIcon(pixmap);
}

QString shortLabel(const QString& text)
{
    constexpr int limit = 26;
    if (text.size() <= limit) {
        return text;
    }
    return text.left(limit - 1) + QStringLiteral("…");
}

bool isTextPreviewFile(const QFileInfo& info);
bool isMarkdownPreviewFile(const QFileInfo& info);
bool isHtmlPreviewFile(const QFileInfo& info);
bool isCsvPreviewFile(const QFileInfo& info);
bool isImagePreviewFile(const QFileInfo& info);
bool isVideoPreviewFile(const QFileInfo& info);
std::optional<QString> youtubeEmbedUrlForFile(const QFileInfo& info);

QSize imagePixelSizeForFile(const QFileInfo& info)
{
    QImageReader reader(info.absoluteFilePath());
    QSize imageSize = reader.size();
    if (!imageSize.isValid()) {
        const QPixmap pixmap(info.absoluteFilePath());
        imageSize = pixmap.size();
    }
    return imageSize;
}

// Reads an image bounded for preview use. Images whose pixel count is absurd (decompression bombs)
// are rejected outright, and large images are decoded downscaled instead of at full resolution so a
// big file never loads its full-size bitmap into memory. maxSide caps the longest decoded edge
// (<= 0 means no downscale). Returns a null image when the file cannot be read or is too large.
QImage boundedPreviewImage(const QFileInfo& info, int maxSide)
{
    QImageReader reader(info.absoluteFilePath());
    reader.setAutoTransform(true);
    const QSize sourceSize = reader.size();  // header-only read, does not decode pixels
    if (sourceSize.isValid() && !sourceSize.isEmpty()) {
        constexpr qint64 kMaxSourcePixels = 120LL * 1000 * 1000;  // ~120 MP guards against bombs
        if (static_cast<qint64>(sourceSize.width()) * sourceSize.height() > kMaxSourcePixels) {
            return {};
        }
        if (maxSide > 0 && (sourceSize.width() > maxSide || sourceSize.height() > maxSide)) {
            reader.setScaledSize(sourceSize.scaled(maxSide, maxSide, Qt::KeepAspectRatio));
        }
    }
    return reader.read();
}

QPixmap cachedImagePixmapForFile(const QFileInfo& info)
{
    const QString path = info.absoluteFilePath();
    const QString cacheKey = QStringLiteral("mycel:image:%1:%2:%3")
                                 .arg(path)
                                 .arg(info.lastModified().toMSecsSinceEpoch())
                                 .arg(info.size());
    QPixmap pixmap;
    if (QPixmapCache::find(cacheKey, &pixmap)) {
        return pixmap;
    }

    // Decode at most at preview resolution; the inline frame never needs full-size source bitmaps.
    const QImage image = boundedPreviewImage(info, 2560);
    if (!image.isNull()) {
        pixmap = QPixmap::fromImage(image);
        QPixmapCache::insert(cacheKey, pixmap);
    }
    return pixmap;
}

QSizeF clampedImagePreviewSize(const QFileInfo& info, const QSizeF& size, bool preferHeight)
{
    constexpr qreal minWidth = 72.0;
    constexpr qreal minHeight = 48.0;
    constexpr qreal maxWidth = 3200.0;
    constexpr qreal maxHeight = 2400.0;

    const QSize imageSize = imagePixelSizeForFile(info);
    if (!imageSize.isValid() || imageSize.isEmpty()) {
        return QSizeF(std::clamp(size.width(), minWidth, maxWidth),
                      std::clamp(size.height(), minHeight, maxHeight));
    }

    const qreal imageWidth = static_cast<qreal>(imageSize.width());
    const qreal imageHeight = static_cast<qreal>(imageSize.height());
    if (preferHeight) {
        const qreal scale = size.height() / imageHeight;
        const qreal minScale = std::max(minWidth / imageWidth, minHeight / imageHeight);
        const qreal maxScale = std::min(maxWidth / imageWidth, maxHeight / imageHeight);
        const qreal clampedScale = std::clamp(scale, minScale, maxScale);
        return QSizeF(imageWidth * clampedScale, imageHeight * clampedScale);
    }

    const qreal scale = size.width() / imageWidth;
    const qreal minScale = std::max(minWidth / imageWidth, minHeight / imageHeight);
    const qreal maxScale = std::min(maxWidth / imageWidth, maxHeight / imageHeight);
    const qreal clampedScale = std::clamp(scale, minScale, maxScale);
    return QSizeF(imageWidth * clampedScale, imageHeight * clampedScale);
}

qreal imagePreviewScaleForSize(const QFileInfo& info, const QSizeF& size, bool preferHeight)
{
    const QSize imageSize = imagePixelSizeForFile(info);
    if (!imageSize.isValid() || imageSize.isEmpty()) {
        return 1.0;
    }

    const qreal scale = preferHeight
                            ? size.height() / static_cast<qreal>(imageSize.height())
                            : size.width() / static_cast<qreal>(imageSize.width());
    const qreal minScale = std::max(72.0 / static_cast<qreal>(imageSize.width()),
                                    48.0 / static_cast<qreal>(imageSize.height()));
    const qreal maxScale = std::min(3200.0 / static_cast<qreal>(imageSize.width()),
                                    2400.0 / static_cast<qreal>(imageSize.height()));
    return std::clamp(scale, minScale, maxScale);
}

QSizeF imagePreviewSizeForScale(const QFileInfo& info, qreal scale)
{
    const QSize imageSize = imagePixelSizeForFile(info);
    if (!imageSize.isValid() || imageSize.isEmpty()) {
        return clampedImagePreviewSize(info, QSizeF(460.0, 260.0), false);
    }

    const qreal imageWidth = static_cast<qreal>(imageSize.width());
    const qreal imageHeight = static_cast<qreal>(imageSize.height());
    const qreal minScale = std::max(72.0 / imageWidth, 48.0 / imageHeight);
    const qreal maxScale = std::min(3200.0 / imageWidth, 2400.0 / imageHeight);
    const qreal clampedScale = std::clamp(scale, minScale, maxScale);
    return QSizeF(imageWidth * clampedScale, imageHeight * clampedScale);
}

#if MYCEL_HAS_PDF
// First-page size in points, cached so the PDF is not reloaded on every layout/paint.
QSizeF pdfFirstPagePointSize(const QFileInfo& info)
{
    static QHash<QString, QSizeF> cache;
    const QString key = info.absoluteFilePath() + QLatin1Char('|') +
                        QString::number(info.lastModified().toMSecsSinceEpoch());
    const auto found = cache.constFind(key);
    if (found != cache.constEnd()) {
        return found.value();
    }
    QSizeF pts;
    QPdfDocument doc;
    if (doc.load(info.absoluteFilePath()) == QPdfDocument::Error::None && doc.pageCount() > 0) {
        pts = doc.pagePointSize(0);
    }
    cache.insert(key, pts);  // cache even empty results to avoid reloading bad/locked files
    return pts;
}
#endif

bool isPdfThumbnailFile(const QFileInfo& info)
{
    return info.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0;
}

bool isEpubThumbnailFile(const QFileInfo& info)
{
    return info.suffix().compare(QStringLiteral("epub"), Qt::CaseInsensitive) == 0;
}

// Extract the cover image from an EPUB (a ZIP): META-INF/container.xml -> OPF package document ->
// the manifest's cover image (EPUB3 properties="cover-image", or EPUB2 <meta name="cover">), with
// a fallback to the first image in the manifest. Returns a null image on failure.
QImage epubCoverImage(const QFileInfo& info)
{
    QZipReader zip(info.absoluteFilePath());
    if (!zip.isReadable()) {
        return {};
    }

    const QByteArray container = zip.fileData(QStringLiteral("META-INF/container.xml"));
    if (container.isEmpty()) {
        return {};
    }
    QString opfPath;
    {
        QXmlStreamReader xml(container);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("rootfile")) {
                opfPath = xml.attributes().value(QLatin1String("full-path")).toString();
                break;
            }
        }
    }
    if (opfPath.isEmpty()) {
        return {};
    }

    const QByteArray opf = zip.fileData(opfPath);
    if (opf.isEmpty()) {
        return {};
    }
    const QString opfDir = QFileInfo(opfPath).path();

    QString coverImageHref;                  // EPUB3: item with properties="cover-image"
    QString coverMetaId;                     // EPUB2: <meta name="cover" content="ID">
    QString firstImageHref;                  // fallback
    QHash<QString, QString> idToHref;        // manifest item id -> href
    {
        QXmlStreamReader xml(opf);
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement()) {
                continue;
            }
            if (xml.name() == QLatin1String("meta")) {
                const auto attrs = xml.attributes();
                if (attrs.value(QLatin1String("name")) == QLatin1String("cover")) {
                    coverMetaId = attrs.value(QLatin1String("content")).toString();
                }
            } else if (xml.name() == QLatin1String("item")) {
                const auto attrs = xml.attributes();
                const QString id = attrs.value(QLatin1String("id")).toString();
                const QString href = attrs.value(QLatin1String("href")).toString();
                const QString media = attrs.value(QLatin1String("media-type")).toString();
                const QString props = attrs.value(QLatin1String("properties")).toString();
                if (!id.isEmpty()) {
                    idToHref.insert(id, href);
                }
                if (props.contains(QLatin1String("cover-image"))) {
                    coverImageHref = href;
                }
                if (firstImageHref.isEmpty() && media.startsWith(QLatin1String("image/"))) {
                    firstImageHref = href;
                }
            }
        }
    }

    QString href = coverImageHref;
    if (href.isEmpty() && !coverMetaId.isEmpty()) {
        href = idToHref.value(coverMetaId);
    }
    if (href.isEmpty()) {
        href = firstImageHref;
    }
    if (href.isEmpty()) {
        return {};
    }

    // hrefs are relative to the OPF location and may be percent-encoded.
    href = QUrl::fromPercentEncoding(href.toUtf8());
    QString imagePath = opfDir.isEmpty() ? href : opfDir + QLatin1Char('/') + href;
    imagePath = QDir::cleanPath(imagePath);
    const QByteArray imageData = zip.fileData(imagePath);
    if (imageData.isEmpty()) {
        return {};
    }
    // Check the pixel count before decoding: a crafted EPUB can carry a tiny compressed cover that
    // expands to a huge bitmap (decompression bomb).
    QBuffer buffer;
    buffer.setData(imageData);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    const QSize coverSize = reader.size();
    if (coverSize.isValid() && !coverSize.isEmpty() &&
        static_cast<qint64>(coverSize.width()) * coverSize.height() > 120LL * 1000 * 1000) {
        return {};
    }
    return reader.read();
}

// Cover image pixel size, cached so the EPUB is not re-extracted for every layout pass.
QSizeF epubCoverPixelSize(const QFileInfo& info)
{
    static QHash<QString, QSizeF> cache;
    const QString key = info.absoluteFilePath() + QLatin1Char('|') +
                        QString::number(info.lastModified().toMSecsSinceEpoch());
    const auto found = cache.constFind(key);
    if (found != cache.constEnd()) {
        return found.value();
    }
    const QImage cover = epubCoverImage(info);
    const QSizeF size = cover.isNull() ? QSizeF() : QSizeF(cover.size());
    cache.insert(key, size);
    return size;
}

QSizeF clampedPreviewSizeForFile(const QFileInfo& info, const QSizeF& size, bool preferHeight = false)
{
    if (isImagePreviewFile(info)) {
        return clampedImagePreviewSize(info, size, preferHeight);
    }
    // Documents (PDF first page / EPUB cover) keep their source aspect ratio so the frame always
    // matches the thumbnail (no side gaps); resizing drives the size from the dragged dimension.
    QSizeF source;
#if MYCEL_HAS_PDF
    if (isPdfThumbnailFile(info)) {
        source = pdfFirstPagePointSize(info);
    }
#endif
    if (source.isEmpty() && isEpubThumbnailFile(info)) {
        source = epubCoverPixelSize(info);
    }
    if (!source.isEmpty() && source.width() > 0.0 && source.height() > 0.0) {
        const qreal aspect = source.width() / source.height();
        qreal height = preferHeight ? size.height() : size.width() / aspect;
        height = std::clamp(height, 90.0, 700.0);
        return QSizeF(height * aspect, height);
    }
    return QSizeF(std::clamp(size.width(), 260.0, 1000.0),
                  std::clamp(size.height(), 110.0, 900.0));
}

// Image and document (PDF/EPUB) previews keep their source aspect ratio, so resizing them drives a
// single dragged dimension. Every other preview frame (text, etc.) resizes freely on both axes.
bool isAspectLockedPreviewFile(const QFileInfo& info)
{
    if (isImagePreviewFile(info)) {
        return true;
    }
#if MYCEL_HAS_PDF
    if (isPdfThumbnailFile(info)) {
        return true;
    }
#endif
    return isEpubThumbnailFile(info);
}

// Target frame size while dragging the resize grip. Aspect-locked previews move one axis (the
// dominant drag direction); free previews follow the grip on both axes.
QSizeF previewResizeTargetSize(const QFileInfo& info, const QSizeF& startSize, const QPointF& delta)
{
    if (isAspectLockedPreviewFile(info)) {
        return std::abs(delta.x()) >= std::abs(delta.y())
                   ? QSizeF(startSize.width() + delta.x(), startSize.height())
                   : QSizeF(startSize.width(), startSize.height() + delta.y());
    }
    return QSizeF(startSize.width() + delta.x(), startSize.height() + delta.y());
}

QSizeF automaticPreviewSize(const QFileInfo& info)
{
    constexpr qreal width = 460.0;
    constexpr qreal minHeight = 46.0;
    constexpr qreal maxHeight = 320.0;

    if (isImagePreviewFile(info)) {
        QSize imageSize = imagePixelSizeForFile(info);
        if (!imageSize.isValid() || imageSize.isEmpty()) {
            return QSizeF(width, 260.0);
        }

        const qreal imageWidth = imageSize.width();
        const qreal imageHeight = imageSize.height();
        const qreal longestSide = std::max(imageWidth, imageHeight);
        const qreal scale = longestSide > width ? width / longestSide : 1.0;

        const qreal previewWidth = imageWidth * scale;
        const qreal previewHeight = imageHeight * scale;
        return clampedPreviewSizeForFile(info, QSizeF(previewWidth, previewHeight));
    }

    {
        // Size the preview frame to the document's aspect (PDF first page / EPUB cover).
        QSizeF source;
#if MYCEL_HAS_PDF
        if (isPdfThumbnailFile(info)) {
            source = pdfFirstPagePointSize(info);
        }
#endif
        if (source.isEmpty() && isEpubThumbnailFile(info)) {
            source = epubCoverPixelSize(info);
        }
        if (isPdfThumbnailFile(info) || isEpubThumbnailFile(info)) {
            if (!source.isEmpty() && source.width() > 0.0 && source.height() > 0.0) {
                const qreal scale = std::min(width / source.width(), maxHeight / source.height());
                return QSizeF(source.width() * scale, source.height() * scale);
            }
            return QSizeF(width * 0.55, maxHeight);  // portrait fallback
        }
    }

    if (isVideoPreviewFile(info)) {
        return QSizeF(520.0, 300.0);
    }

    if (youtubeEmbedUrlForFile(info)) {
        return QSizeF(560.0, 340.0);
    }

    if (isMarkdownPreviewFile(info)) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QSizeF(width, minHeight);
        }

        QTextDocument doc;
        QFont font;
        font.setPointSize(10);
        doc.setDefaultFont(font);
        QByteArray data = file.read(64 * 1024);
        if (!file.atEnd()) {
            data += "\n\n...";
        }
        const QString markdown = QString::fromUtf8(data);
        QTextOption option = doc.defaultTextOption();
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        doc.setDefaultTextOption(option);
        doc.setDefaultStyleSheet(QStringLiteral("* { color: #243036; } a { color: #1168b3; }"));
        doc.setMarkdown(markdown);
        if (doc.toPlainText().trimmed().isEmpty() && !markdown.trimmed().isEmpty()) {
            doc.setPlainText(markdown);
        }
        doc.setTextWidth(width - 24.0);
        return QSizeF(width, std::clamp(doc.size().height() + 24.0, minHeight, maxHeight));
    }

    if (info.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0 ||
        !isTextPreviewFile(info) || info.size() > 1024 * 1024) {
        return QSizeF(width, minHeight);
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QSizeF(width, minHeight);
    }

    QTextStream stream(&file);
    QStringList lines;
    while (lines.size() < 24 && !stream.atEnd()) {
        lines << stream.readLine();
    }
    if (lines.isEmpty()) {
        lines << QStringLiteral("空のファイル");
    }

    QTextDocument doc;
    QFont font;
    font.setPointSize(9);
    font.setFamily(QStringLiteral("Menlo"));
    doc.setDefaultFont(font);
    QTextOption option = doc.defaultTextOption();
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    doc.setDefaultTextOption(option);
    doc.setPlainText(lines.join(QLatin1Char('\n')));
    doc.setTextWidth(width - 24.0);
    return QSizeF(width, std::clamp(doc.size().height() + 24.0, minHeight, 220.0));
}

bool directoryHasMycel(const QString& path)
{
    return QFileInfo(QDir(path).filePath(QStringLiteral(".mycel"))).isDir();
}

// Atomically write a JSON document (temp file + rename via QSaveFile) so a crash or power loss
// mid-write can never corrupt existing .mycel metadata — important on cloud-synced folders.
// On Windows the final rename fails while a sync client (Dropbox etc.) briefly holds the target
// open, so retry a few times and, as a last resort, fall back to a direct write: that save loses
// atomicity but the data is never dropped.
bool writeJsonFileAtomic(const QString& path, const QJsonObject& object)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Indented);

    for (int attempt = 0; attempt < 3; ++attempt) {
        if (attempt > 0) {
            QThread::msleep(30);
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            continue;
        }
        file.write(payload);
        if (file.commit()) {
            return true;
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(payload) == payload.size();
}

// The exact folder-with-Mycel-badge glyph used for sub-root nodes, rendered into a pixmap so the
// same mark can be reused wherever a Mycel sub-root/parent-root is shown (e.g. the parent overlay).
// Keeps the parent (as shown by the child) identical in appearance to a sub-root (as shown by the parent).
QPixmap subRootFolderPixmap()
{
    static QPixmap cached;
    if (!cached.isNull()) {
        return cached;
    }
    QPixmap pm(46, 40);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPointF at(3.0, 1.0);
    QPainterPath path;
    path.moveTo(at.x(), at.y() + 9.0);
    path.quadTo(at.x(), at.y() + 4.0, at.x() + 5.0, at.y() + 4.0);
    path.lineTo(at.x() + 16.0, at.y() + 4.0);
    path.lineTo(at.x() + 22.0, at.y() + 10.0);
    path.lineTo(at.x() + 36.0, at.y() + 10.0);
    path.quadTo(at.x() + 40.0, at.y() + 10.0, at.x() + 40.0, at.y() + 14.0);
    path.lineTo(at.x() + 40.0, at.y() + 31.0);
    path.quadTo(at.x() + 40.0, at.y() + 36.0, at.x() + 35.0, at.y() + 36.0);
    path.lineTo(at.x() + 5.0, at.y() + 36.0);
    path.quadTo(at.x(), at.y() + 36.0, at.x(), at.y() + 31.0);
    path.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#ffc94d"));
    p.drawPath(path);
    p.setPen(QPen(QColor("#f6b93f"), 1.8));
    p.drawLine(QPointF(at.x() + 3.0, at.y() + 14.0), QPointF(at.x() + 37.0, at.y() + 14.0));
    const QPixmap badge = QPixmap(QStringLiteral(":/icons/mycel.png"))
                              .scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!badge.isNull()) {
        p.drawPixmap(QPointF(at.x() + 22.0, at.y() + 18.0), badge);
    }
    p.end();
    cached = pm;
    return cached;
}

std::unique_ptr<Node> scanTree(const QString& path, int depth, int branch,
                               const QSet<QString>& collapsedPaths, const QSet<QString>& previewPaths,
                               const std::map<QString, QSizeF>& previewSizes,
                               const std::map<QString, QStringList>& fileOrders,
                               const QString& rootPath, bool detectSubRoots)
{
    QFileInfo info(path);
    auto node = std::make_unique<Node>();
    node->path = info.absoluteFilePath();
    node->parentPath = info.absoluteDir().absolutePath();
    node->name = info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName();
    node->isDir = info.isDir();
    // A child directory carrying its own .mycel is a sub-root: display it as a single boundary node
    // (do not descend into it). The opened root itself (depth 0) is never a sub-root.
    node->isSubRoot = detectSubRoots && node->isDir && depth >= 1 &&
                      directoryHasMycel(info.absoluteFilePath());
    node->collapsed = node->isDir && collapsedPaths.contains(info.absoluteFilePath());
    node->previewOpen = !node->isDir && previewPaths.contains(info.absoluteFilePath());
    node->depth = depth;
    node->branch = branch;

    QFont font;
    font.setPointSize(depth == 0 ? 17 : (node->isDir ? 12 : 11));
    QFontMetricsF metrics(font);
    const qreal iconWidth = node->isDir ? 46.0 : 30.0;
    const qreal minWidth = node->isDir ? (depth == 0 ? 178.0 : 128.0) : 104.0;
    const qreal height = node->isDir ? (depth == 0 ? 64.0 : 46.0) : 30.0;
    node->size = QSizeF(std::max(minWidth, metrics.horizontalAdvance(shortLabel(node->name)) + iconWidth + 34.0),
                        height);
    if (node->previewOpen) {
        const auto found = previewSizes.find(node->path);
        node->previewSize = found == previewSizes.end() ? automaticPreviewSize(info) : found->second;
    }

    if (!node->isDir || depth >= MaxDepth || node->isSubRoot) {
        return node;
    }

    QDir dir(node->path);
    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                              QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    entries.erase(std::remove_if(entries.begin(), entries.end(), [](const QFileInfo& entry) {
        return entry.fileName() == QStringLiteral(".mycel");
    }), entries.end());

    const QString orderKey = QDir(rootPath).relativeFilePath(node->path);
    const QString normalizedOrderKey = orderKey == QStringLiteral(".") ? QString() : orderKey;
    const auto order = fileOrders.find(normalizedOrderKey);
    if (order != fileOrders.end()) {
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

    if (node->collapsed) {
        node->hiddenChildren = static_cast<int>(entries.size());
        return node;
    }

    const int visible = std::min(MaxChildren, static_cast<int>(entries.size()));
    for (int i = 0; i < visible; ++i) {
        node->children.push_back(scanTree(entries[i].absoluteFilePath(), depth + 1, branch,
                                          collapsedPaths, previewPaths, previewSizes, fileOrders, rootPath,
                                          detectSubRoots));
    }
    node->hiddenChildren = entries.size() - visible;
    return node;
}

void assignTopLevelBranches(Node& root)
{
    root.branch = -1;
    for (int i = 0; i < static_cast<int>(root.children.size()); ++i) {
        const int branch = i;
        std::function<void(Node&)> visit = [&](Node& node) {
            node.branch = branch;
            for (auto& child : node.children) {
                visit(*child);
            }
        };
        visit(*root.children[i]);
    }
}

Node* findNodeByPath(Node& node, const QString& path)
{
    if (node.path == path) {
        return &node;
    }
    for (auto& child : node.children) {
        if (Node* found = findNodeByPath(*child, path)) {
            return found;
        }
    }
    return nullptr;
}

// True when two scans describe the same displayed tree: same paths, same file/dir kind,
// and the same children in the same order, recursively. Used to skip re-rendering when a
// filesystem notification (e.g. a cloud-sync mtime touch) did not change what is shown.
bool sameVisibleStructure(const Node& a, const Node& b)
{
    if (a.path != b.path || a.isDir != b.isDir || a.children.size() != b.children.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.children.size(); ++i) {
        if (!sameVisibleStructure(*a.children[i], *b.children[i])) {
            return false;
        }
    }
    return true;
}

// Generates the displayed geometry: node coordinates, the parent-child and file-link
// connection paths, and per-node bounds. Both the bounds calculation and ConnectionLayer's
// drawing go through the same path builders here, so a connector can never fall outside the
// bounds that were reserved for it (which used to make lines disappear).
struct TreeLayoutEngine {
    // Paths of file-link targets — these are laid out beside their link source, not in the
    // normal tree column.
    static QSet<QString> linkedTargetPaths(const std::vector<FileLink>& links)
    {
        QSet<QString> targets;
        for (const FileLink& link : links) {
            targets.insert(link.to);
        }
        return targets;
    }

    // Assign node centers. Children that are file-link targets are skipped here and placed
    // beside their link source by layoutFileLinks instead.
    static void layoutTree(Node& node, qreal leftX, qreal& yCursor, const QSet<QString>& linkedTargets,
                           Node& root, const std::vector<FileLink>& links)
    {
        node.center.setX(leftX + node.size.width() / 2.0);
        const auto isTreeChild = [&linkedTargets](const std::unique_ptr<Node>& child) {
            return !linkedTargets.contains(child->path);
        };
        const bool hasTreeChildren = std::any_of(node.children.begin(), node.children.end(), isTreeChild);
        if (!hasTreeChildren) {
            node.center.setY(yCursor);
            // Reserve room for the whole link fan (e.g. a tall .pdf linked to this short .txt),
            // not just this node's own preview, so the next row clears it.
            yCursor += nodeRowSpan(node, root, links);
            return;
        }

        const qreal childLeftX = leftX + std::max(XStep, node.size.width() + ParentChildGap);
        for (auto& child : node.children) {
            if (linkedTargets.contains(child->path)) {
                continue;
            }
            layoutTree(*child, childLeftX, yCursor, linkedTargets, root, links);
        }

        const auto first = std::find_if(node.children.begin(), node.children.end(), isTreeChild);
        const auto last = std::find_if(node.children.rbegin(), node.children.rend(), isTreeChild);
        node.center.setY(((*first)->center.y() + (*last)->center.y()) / 2.0);
    }

    static void translateTree(Node& node, const QPointF& delta)
    {
        node.center += delta;
        for (auto& child : node.children) {
            translateTree(*child, delta);
        }
    }

    // The curved connector drawn from a parent node to one of its children.
    static QPainterPath parentChildEdgePath(const Node& parent, const Node& child)
    {
        const QPointF start(parent.center.x() + parent.size.width() / 2.0, parent.center.y());
        const QPointF end(child.center.x() - child.size.width() / 2.0, child.center.y());
        const qreal distance = std::max<qreal>(96.0, end.x() - start.x());
        const qreal splitX = start.x() + std::clamp(distance * 0.46, 58.0, 128.0);
        QPainterPath path(start);
        path.cubicTo(QPointF(start.x() + distance * 0.16, start.y()),
                     QPointF(splitX - 34.0, end.y()),
                     QPointF(splitX, end.y()));
        path.cubicTo(QPointF(splitX + 30.0, end.y()),
                     QPointF(end.x() - 38.0, end.y()),
                     end);
        return path;
    }

    // The curved connector drawn for a user-created file link between two file nodes. Uses
    // the same split-and-fan shape as parent-child edges so multiple targets of one source
    // spread up and down consistently, instead of a plain diagonal S-curve.
    static QPainterPath fileLinkPath(const Node& from, const Node& to)
    {
        const QPointF anchor = linkSourceAnchor(from);
        const QPointF start(anchor.x() + 6.0, anchor.y());
        const QPointF end(to.center.x() - to.size.width() / 2.0 - 6.0, to.center.y());
        // Run horizontally along the source's row past the neighbour previews, then fan up or
        // down at the target column (its left edge, i.e. the right edge of those previews).
        const qreal columnLeft = to.center.x() - to.size.width() / 2.0 - ParentChildGap;
        const qreal splitX = std::min(std::max(columnLeft, start.x() + 20.0), end.x() - 20.0);
        QPainterPath path(start);
        path.lineTo(QPointF(splitX, start.y()));
        path.cubicTo(QPointF(splitX + 34.0, start.y()),
                     QPointF(end.x() - 38.0, end.y()),
                     end);
        return path;
    }

    static qreal nodeVerticalSpan(const Node& node)
    {
        return YStep + (node.previewOpen ? node.previewSize.height() + 28.0 : 0.0);
    }

    // Vertical room a node needs in the tree, including the fan of file-link targets to its right
    // (recursively). A short .txt whose linked .pdf has a tall preview must still reserve the
    // .pdf's height so the next row does not overlap that preview.
    static qreal nodeRowSpan(const Node& node, Node& root, const std::vector<FileLink>& links)
    {
        qreal fan = 0.0;
        for (const FileLink& link : links) {
            if (link.from == node.path) {
                if (Node* to = findNodeByPath(root, link.to)) {
                    fan += nodeRowSpan(*to, root, links);  // stacked targets sum; a chain takes the max
                }
            }
        }
        return std::max(nodeVerticalSpan(node), fan);
    }

    // Right-most x of a node's drawn content (accounts for an open preview wider than the card).
    static qreal nodeContentRightX(const Node& node)
    {
        qreal right = node.center.x() + node.size.width() / 2.0;
        if (node.previewOpen) {
            const qreal previewRight = node.center.x() - node.size.width() / 2.0 +
                                       std::max(node.size.width(), node.previewSize.width());
            right = std::max(right, previewRight);
        }
        return right;
    }

    // Point on the source's right edge where its file links emanate. With an open preview
    // this is the right edge of the preview, vertically centered on it, so links fan out from
    // the preview rather than from the small header above it.
    static QPointF linkSourceAnchor(const Node& node)
    {
        const qreal x = nodeContentRightX(node);
        if (node.previewOpen) {
            const qreal top = node.center.y() - node.size.height() / 2.0;
            return QPointF(x, top + (34.0 + node.previewSize.height()) / 2.0);
        }
        return QPointF(x, node.center.y());
    }

    // Vertical room a link target needs. A folder target expands its own subtree to the right, so it
    // needs the whole subtree height (mirrors layoutTree's yCursor advance); a file needs one row.
    static qreal linkedTargetSpan(const Node& node, const QSet<QString>& linkedTargets,
                                  Node& root, const std::vector<FileLink>& links)
    {
        if (!node.isDir) {
            return nodeVerticalSpan(node);
        }
        const auto isTreeChild = [&linkedTargets](const std::unique_ptr<Node>& child) {
            return !linkedTargets.contains(child->path);
        };
        const bool hasTreeChildren = std::any_of(node.children.begin(), node.children.end(), isTreeChild);
        if (!hasTreeChildren) {
            return nodeRowSpan(node, root, links);
        }
        qreal total = 0.0;
        for (const auto& child : node.children) {
            if (!linkedTargets.contains(child->path)) {
                total += linkedTargetSpan(*child, linkedTargets, root, links);
            }
        }
        return total;
    }

    // Place each file-link target to the right of its link source. Multiple targets of the
    // same source are stacked and centered on the source's link anchor (the preview's right
    // edge when a preview is open), so they fan up and down from there. Runs after layoutTree.
    // A folder target lays out its whole subtree at the link position (like a mini-tree).
    static void layoutFileLinks(Node& root, const std::vector<FileLink>& links)
    {
        // Collect each source's targets, preserving link order.
        std::map<QString, std::vector<Node*>> targetsBySource;
        for (const FileLink& link : links) {
            Node* from = findNodeByPath(root, link.from);
            Node* to = findNodeByPath(root, link.to);
            if (!from || !to || from->isDir || from == to) {  // source must be a file; target may be a folder
                continue;
            }
            targetsBySource[from->path].push_back(to);
        }

        const QSet<QString> linkedTargets = linkedTargetPaths(links);

        for (auto& [sourcePath, targets] : targetsBySource) {
            Node* from = findNodeByPath(root, sourcePath);
            if (!from) {
                continue;
            }
            std::vector<qreal> spans;
            spans.reserve(targets.size());
            qreal totalSpan = 0.0;
            for (const Node* to : targets) {
                const qreal span = linkedTargetSpan(*to, linkedTargets, root, links);
                spans.push_back(span);
                totalSpan += span;
            }
            const QPointF anchor = linkSourceAnchor(*from);
            const qreal stackTop = anchor.y() - totalSpan / 2.0;
            const qreal stackBottom = anchor.y() + totalSpan / 2.0;

            // Push the target column clear of any tree node whose frame (card or open
            // preview) overlaps the rows the stack will occupy, so targets don't land on the
            // preview of the node above or below the source. Skip linked targets and their
            // subtrees (they are positioned here, not yet laid out in the tree).
            qreal columnLeft = anchor.x();
            std::function<void(const Node&)> clearOverlaps = [&](const Node& node) {
                if (linkedTargets.contains(node.path)) {
                    return;
                }
                if (&node != from) {
                    const qreal top = node.center.y() - node.size.height() / 2.0;
                    const qreal bottom = top + (node.previewOpen ? 34.0 + node.previewSize.height()
                                                                 : node.size.height());
                    if (bottom >= stackTop && top <= stackBottom) {
                        columnLeft = std::max(columnLeft, nodeContentRightX(node));
                    }
                }
                for (const auto& child : node.children) {
                    clearOverlaps(*child);
                }
            };
            clearOverlaps(root);

            const qreal linkedLeftX = columnLeft + ParentChildGap;
            qreal slotTop = stackTop;  // center the stack on the source anchor
            for (std::size_t i = 0; i < targets.size(); ++i) {
                Node* to = targets[i];
                const qreal span = spans[i];
                if (to->isDir) {
                    // Lay out the folder subtree at a temporary origin, then translate so the folder
                    // node lands centered in its slot at the link column.
                    qreal localCursor = 0.0;
                    layoutTree(*to, 0.0, localCursor, linkedTargets, root, links);
                    const qreal targetCenterY = slotTop + span / 2.0;
                    translateTree(*to, QPointF(linkedLeftX, targetCenterY - to->center.y()));
                } else {
                    // A node's visible content (header + open preview) hangs DOWN from the card top,
                    // so center that whole extent in the slot — otherwise a tall preview spills over
                    // the target below it.
                    const qreal visualHeight = to->previewOpen ? 34.0 + to->previewSize.height()
                                                               : to->size.height();
                    const qreal centerY = slotTop + (span - visualHeight) / 2.0 + to->size.height() / 2.0;
                    to->center = QPointF(linkedLeftX + to->size.width() / 2.0, centerY);
                }
                slotTop += span;
            }
        }
    }

    static QRectF nodePaintBounds(const Node& node)
    {
        QRectF nodeRect(node.center.x() - node.size.width() / 2.0 - 220.0,
                        node.center.y() - node.size.height() / 2.0 - 80.0,
                        node.size.width() + 440.0,
                        node.size.height() + 160.0);
        if (node.previewOpen) {
            const qreal previewWidth = std::max(node.size.width(), node.previewSize.width());
            const QRectF previewRect(node.center.x() - node.size.width() / 2.0,
                                     node.center.y() - node.size.height() / 2.0,
                                     previewWidth,
                                     34.0 + node.previewSize.height());
            nodeRect = nodeRect.united(previewRect.adjusted(-40.0, -40.0, 80.0, 40.0));
        }
        return nodeRect;
    }

    // Compute and store each node's subtree bounds, reserving room for every parent-child
    // connector via the same path builder ConnectionLayer draws with.
    static QRectF updateSubtreeBounds(Node& node, const QSet<QString>& linkedTargets)
    {
        QRectF bounds = nodePaintBounds(node);
        for (auto& child : node.children) {
            bounds = bounds.united(updateSubtreeBounds(*child, linkedTargets));
            if (linkedTargets.contains(child->path)) {
                continue;  // linked targets connect via the link line, not a parent-child edge
            }
            const QPainterPath edgePath = parentChildEdgePath(node, *child);
            bounds = bounds.united(edgePath.controlPointRect().adjusted(-8.0, -8.0, 8.0, 8.0));
        }
        node.subtreeBounds = bounds;
        return bounds;
    }
};

void visitNodes(Node& node, const std::function<void(Node&)>& fn)
{
    fn(node);
    for (auto& child : node.children) {
        visitNodes(*child, fn);
    }
}

QString youtubeVideoIdFromUrl(const QUrl& url)
{
    if (!url.isValid()) {
        return {};
    }

    QString host = url.host().toLower();
    if (host.startsWith(QStringLiteral("www."))) {
        host.remove(0, 4);
    }
    const QStringList parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);

    QString videoId;
    if (host == QStringLiteral("youtu.be")) {
        if (!parts.isEmpty()) {
            videoId = parts.first();
        }
    } else if (host == QStringLiteral("youtube.com") || host.endsWith(QStringLiteral(".youtube.com"))) {
        if (url.path() == QStringLiteral("/watch")) {
            videoId = QUrlQuery(url).queryItemValue(QStringLiteral("v"));
        } else if (parts.size() >= 2 &&
                   (parts[0] == QStringLiteral("shorts") ||
                    parts[0] == QStringLiteral("embed") ||
                    parts[0] == QStringLiteral("live"))) {
            videoId = parts[1];
        }
    }

    videoId = videoId.trimmed();
    if (videoId.contains(QLatin1Char('?')) || videoId.contains(QLatin1Char('&'))) {
        videoId = videoId.left(videoId.indexOf(QRegularExpression(QStringLiteral("[?&]"))));
    }
    // Strict charset: the id is spliced into URLs (watch page, img.youtube.com thumbnail path),
    // so reject anything beyond the YouTube id alphabet to rule out path manipulation.
    static const QRegularExpression idPattern(QStringLiteral("^[A-Za-z0-9_-]{6,32}$"));
    if (!idPattern.match(videoId).hasMatch()) {
        return {};
    }
    return videoId;
}

std::optional<QString> youtubeEmbedUrlFromText(const QString& text)
{
    static const QRegularExpression urlPattern(
        QStringLiteral(R"((?:URL\s*=\s*)?(https?://[^\s<>"'\]\)]+))"),
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatchIterator it = urlPattern.globalMatch(text);
    while (it.hasNext()) {
        QString token = it.next().captured(1).trimmed();
        token.remove(QRegularExpression(QStringLiteral("[>,。]+$")));
        QUrl url(token);
        if (!url.isValid() || url.scheme().isEmpty()) {
            continue;
        }
        const QString videoId = youtubeVideoIdFromUrl(url);
        if (!videoId.isEmpty()) {
            return QStringLiteral("https://www.youtube.com/embed/%1").arg(videoId);
        }
    }
    return std::nullopt;
}

std::optional<QUrl> firstUrlFromText(const QString& text)
{
    static const QRegularExpression urlPattern(
        QStringLiteral(R"((?:URL\s*=\s*)?(https?://[^\s<>"'\]\)]+))"),
        QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch match = urlPattern.match(text);
    if (!match.hasMatch()) {
        return std::nullopt;
    }

    QString token = match.captured(1).trimmed();
    token.remove(QRegularExpression(QStringLiteral("[>,。]+$")));
    QUrl url(token);
    if (!url.isValid() || url.scheme().isEmpty()) {
        return std::nullopt;
    }
    return url;
}

QString youtubeWatchUrlFromEmbedUrl(const QString& embedUrl)
{
    const QString videoId = youtubeVideoIdFromUrl(QUrl(embedUrl));
    return videoId.isEmpty() ? embedUrl : QStringLiteral("https://www.youtube.com/watch?v=%1").arg(videoId);
}

QString youtubeThumbnailUrlFromEmbedUrl(const QString& embedUrl)
{
    const QString videoId = youtubeVideoIdFromUrl(QUrl(embedUrl));
    return videoId.isEmpty() ? QString() : QStringLiteral("https://img.youtube.com/vi/%1/hqdefault.jpg").arg(videoId);
}

QString youtubeVideoIdFromEmbedUrl(const QString& embedUrl)
{
    return youtubeVideoIdFromUrl(QUrl(embedUrl));
}

std::optional<QString> youtubeEmbedUrlForFile(const QFileInfo& info)
{
    if (!info.exists() || !info.isFile() || info.size() > 64 * 1024) {
        return std::nullopt;
    }
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    return youtubeEmbedUrlFromText(QString::fromUtf8(file.read(64 * 1024)));
}

bool isTextPreviewFile(const QFileInfo& info)
{
    static const QSet<QString> extensions = {
        QStringLiteral("txt"), QStringLiteral("md"), QStringLiteral("rst"),
        QStringLiteral("cpp"), QStringLiteral("cc"), QStringLiteral("cxx"),
        QStringLiteral("h"), QStringLiteral("hpp"), QStringLiteral("c"),
        QStringLiteral("cmake"), QStringLiteral("json"), QStringLiteral("xml"),
        QStringLiteral("yaml"), QStringLiteral("yml"), QStringLiteral("toml"),
        QStringLiteral("js"), QStringLiteral("ts"), QStringLiteral("css"),
        QStringLiteral("html"), QStringLiteral("htm"), QStringLiteral("markdown"),
        QStringLiteral("csv"), QStringLiteral("sh"), QStringLiteral("url")
    };
    return extensions.contains(info.suffix().toLower()) || info.fileName() == QStringLiteral("CMakeLists.txt");
}

bool isMarkdownPreviewFile(const QFileInfo& info)
{
    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown");
}

bool isHtmlPreviewFile(const QFileInfo& info)
{
    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("html") || suffix == QStringLiteral("htm");
}

bool isCsvPreviewFile(const QFileInfo& info)
{
    return info.suffix().compare(QStringLiteral("csv"), Qt::CaseInsensitive) == 0;
}

bool isPreviewMetadataLine(const QString& line)
{
    return line.startsWith(QStringLiteral("Subject:")) || line.startsWith(QStringLiteral("Key:"));
}

QString filterPreviewMetadataLines(const QString& text)
{
    QStringList filtered;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (!isPreviewMetadataLine(line)) {
            filtered.append(line);
        }
    }
    return filtered.join(QLatin1Char('\n'));
}

QStringList parseCsvLine(const QString& line)
{
    QStringList cells;
    QString cell;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('"')) {
            if (quoted && i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                cell.append(ch);
                ++i;
            } else {
                quoted = !quoted;
            }
            continue;
        }
        if (ch == QLatin1Char(',') && !quoted) {
            cells.append(cell);
            cell.clear();
            continue;
        }
        cell.append(ch);
    }
    cells.append(cell);
    return cells;
}

QString csvToHtmlTable(const QString& text)
{
    const ThemeColors colors = currentThemeColors();
    QString html = QStringLiteral(
                       "<html><head><style>"
                       "body{font-family:Meiryo,Segoe UI,sans-serif;color:%1;background:%2;}"
                       "table{border-collapse:collapse;width:100%;font-size:13px;}"
                       "td,th{border:1px solid %3;padding:4px 6px;vertical-align:top;}"
                       "tr:nth-child(even){background:%4;}"
                       "</style></head><body><table>")
                       .arg(cssColor(colors.previewText),
                            cssColor(colors.previewTextBackground),
                            cssColor(colors.previewTextBorder),
                            cssColor(colors.alternateBase));
    const QStringList lines = text.split(QLatin1Char('\n'));
    const qsizetype rowLimit = std::min<qsizetype>(lines.size(), 400);
    for (qsizetype row = 0; row < rowLimit; ++row) {
        html += QStringLiteral("<tr>");
        const QStringList cells = parseCsvLine(lines.at(row));
        for (const QString& cell : cells) {
            html += QStringLiteral("<td>%1</td>").arg(cell.toHtmlEscaped());
        }
        html += QStringLiteral("</tr>");
    }
    if (lines.size() > rowLimit) {
        html += QStringLiteral("<tr><td colspan=\"999\">...</td></tr>");
    }
    html += QStringLiteral("</table></body></html>");
    return html;
}

QString archiveComment(const QString& kind, const QJsonObject& object)
{
    return QStringLiteral("<!-- mycel:%1 %2 -->\n")
        .arg(kind, QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
}

std::optional<QJsonObject> archiveCommentObject(const QString& line, const QString& kind)
{
    const QString trimmed = line.trimmed();
    const QString prefix = QStringLiteral("<!-- mycel:%1 ").arg(kind);
    if (!trimmed.startsWith(prefix) || !trimmed.endsWith(QStringLiteral("-->"))) {
        return std::nullopt;
    }

    const QString json = trimmed.mid(prefix.size(), trimmed.size() - prefix.size() - 3).trimmed();
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) {
        return std::nullopt;
    }
    return doc.object();
}

QString escapedArchiveText(const QString& text)
{
    QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString& line : lines) {
        if (line.startsWith(QLatin1Char('\\')) || line.startsWith(QStringLiteral("```"))) {
            line.prepend(QLatin1Char('\\'));
        }
    }
    return lines.join(QLatin1Char('\n'));
}

QString unescapedArchiveText(const QStringList& lines)
{
    QStringList restored = lines;
    for (QString& line : restored) {
        if (line.startsWith(QLatin1Char('\\'))) {
            line.remove(0, 1);
        }
    }
    return restored.join(QLatin1Char('\n'));
}

QString markdownHeading(int level, QString title)
{
    level = std::clamp(level, 1, 6);
    title.replace(QLatin1Char('\n'), QLatin1Char(' '));
    if (title.trimmed().isEmpty()) {
        title = QStringLiteral("(empty)");
    }
    return QString(level, QLatin1Char('#')) + QLatin1Char(' ') + title + QStringLiteral("\n\n");
}

QString archiveRelativePath(QString path)
{
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QDir::cleanPath(path);
}

bool isSafeArchiveRelativePath(const QString& path)
{
    const QString clean = archiveRelativePath(path);
    if (clean.isEmpty() || clean == QStringLiteral(".") || QDir::isAbsolutePath(clean)) {
        return false;
    }

    const QStringList parts = clean.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return false;
    }
    for (const QString& part : parts) {
        if (part == QStringLiteral(".") || part == QStringLiteral("..") ||
            part == QStringLiteral(".mycel")) {
            return false;
        }
    }
    return true;
}

bool isImagePreviewFile(const QFileInfo& info)
{
    static const QSet<QString> extensions = {
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("bmp"), QStringLiteral("svg")
    };
    return extensions.contains(info.suffix().toLower());
}

bool isVideoPreviewFile(const QFileInfo& info)
{
    static const QSet<QString> extensions = {
        QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("m4v"),
        QStringLiteral("avi"), QStringLiteral("mkv"), QStringLiteral("webm"),
        QStringLiteral("wmv")
    };
    return extensions.contains(info.suffix().toLower());
}

// A short badge plus an accent color drawn on a file icon, so file types are distinguishable at
// a glance. accent is invalid for the generic fallback.
struct FileKindStyle {
    QString badge;
    QColor accent;
};

FileKindStyle fileKindStyleFor(const QFileInfo& info)
{
    const QString s = info.suffix().toLower();
    const QString name = info.fileName();
    auto is = [&s](std::initializer_list<const char*> exts) {
        for (const char* e : exts) {
            if (s == QLatin1String(e)) {
                return true;
            }
        }
        return false;
    };
    // Saturated mid-tones that stay readable on the light file page in both themes.
    const QColor blue(0x2f, 0x6f, 0xed);
    const QColor cyan(0x10, 0x9b, 0xb8);
    const QColor green(0x1f, 0x9d, 0x55);
    const QColor amber(0xc9, 0x84, 0x12);
    const QColor red(0xd6, 0x45, 0x3d);
    const QColor purple(0x8b, 0x5c, 0xf6);
    const QColor slate(0x60, 0x70, 0x88);
    const QColor pink(0xdb, 0x2f, 0x88);

    if (is({"py"})) return {QStringLiteral("PY"), blue};
    if (is({"go"})) return {QStringLiteral("GO"), cyan};
    if (is({"rs"})) return {QStringLiteral("RS"), amber};
    if (is({"ts", "tsx"})) return {QStringLiteral("TS"), blue};
    if (is({"js", "mjs", "cjs", "jsx"})) return {QStringLiteral("JS"), amber};
    if (is({"java"})) return {QStringLiteral("JV"), red};
    if (is({"rb"})) return {QStringLiteral("RB"), red};
    if (is({"php"})) return {QStringLiteral("PHP"), purple};
    if (is({"cs"})) return {QStringLiteral("C#"), green};
    if (is({"cpp", "cc", "cxx", "c", "h", "hpp", "hh"})) return {QStringLiteral("<>"), blue};
    if (is({"css", "scss", "sass", "less"})) return {QStringLiteral("CSS"), blue};
    if (is({"html", "htm"})) return {QStringLiteral("<>"), amber};
    if (is({"md", "rst", "markdown"})) return {QStringLiteral("MD"), slate};
    if (is({"txt", "log"})) return {QStringLiteral("TXT"), slate};
    if (is({"csv", "tsv"})) return {QStringLiteral("CSV"), green};
    if (is({"pdf"})) return {QStringLiteral("PDF"), red};
    if (is({"epub"})) return {QStringLiteral("EPUB"), purple};
    if (is({"doc", "docx", "rtf"})) return {QStringLiteral("DOC"), blue};
    if (is({"xls", "xlsx"})) return {QStringLiteral("XLS"), green};
    if (is({"ppt", "pptx"})) return {QStringLiteral("PPT"), amber};
    if (is({"zip", "tar", "gz", "tgz", "7z", "rar"})) return {QStringLiteral("ZIP"), amber};
    if (is({"mp3", "wav", "flac", "aac", "ogg", "m4a"})) return {QStringLiteral("♪"), purple};
    if (is({"mp4", "mov", "avi", "mkv", "webm", "m4v"})) return {QStringLiteral("▶"), pink};
    if (is({"json", "yaml", "yml", "toml", "xml", "ini", "cfg", "conf", "cmake"}) ||
        name == QStringLiteral("CMakeLists.txt")) {
        return {QStringLiteral("⚙"), slate};
    }
    if (is({"sh", "bash", "zsh", "bat", "cmd", "ps1", "command"})) return {QStringLiteral("$"), green};
    return {QString(), QColor()};
}

QString normalizedDirectoryPath(const QString& path)
{
    QFileInfo info(path);
    const QString absolutePath = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    const QString canonicalPath = QDir(absolutePath).canonicalPath();
    return canonicalPath.isEmpty() ? QDir(absolutePath).absolutePath() : canonicalPath;
}

bool textInputWidgetHasFocus()
{
    for (QWidget* widget = QApplication::focusWidget(); widget; widget = widget->parentWidget()) {
        if (qobject_cast<QLineEdit*>(widget) || qobject_cast<QPlainTextEdit*>(widget)) {
            return true;
        }
        if (auto* textEdit = qobject_cast<QTextEdit*>(widget)) {
            return !textEdit->isReadOnly();
        }
    }
    return false;
}

struct StartupOptions {
    QString rootPath;
    bool mycelStorageEnabled = true;
    bool showVersion = false;
};

StartupOptions startupOptions(const QStringList& arguments)
{
    StartupOptions options;
    QString rootArgument;
    bool endOfOptions = false;

    for (int i = 1; i < arguments.size(); ++i) {
        const QString argument = arguments.at(i).trimmed();
        if (argument.isEmpty()) {
            continue;
        }
        if (!endOfOptions && argument == QStringLiteral("--")) {
            endOfOptions = true;
            continue;
        }
        if (!endOfOptions && (argument == QStringLiteral("--no-mycel") ||
                              argument == QStringLiteral("--no-metadata"))) {
            options.mycelStorageEnabled = false;
            continue;
        }
        if (!endOfOptions && (argument == QStringLiteral("--version") ||
                              argument == QStringLiteral("-v"))) {
            options.showVersion = true;
            continue;
        }
        if (rootArgument.isEmpty()) {
            rootArgument = argument;
        }
    }

    options.rootPath = normalizedDirectoryPath(rootArgument.isEmpty() ? QDir::currentPath() : rootArgument);
    return options;
}

bool hasMycelFolder(const QString& rootPath)
{
    return QFileInfo(QDir(rootPath).filePath(QStringLiteral(".mycel"))).isDir();
}

bool createMycelFolder(const QString& rootPath)
{
    return QDir(rootPath).mkpath(QStringLiteral(".mycel"));
}

QString historyPathKey(const QString& path)
{
    QString key = QDir::cleanPath(path);
#ifdef Q_OS_WIN
    key = key.toCaseFolded();
#endif
    return key;
}

QStringList recentRootPaths()
{
    constexpr int MaxRecentRoots = 20;
    QSettings settings;
    const QStringList savedPaths = settings.value(QStringLiteral("startup/recentRoots")).toStringList();
    QStringList paths;
    QSet<QString> seen;

    for (const QString& savedPath : savedPaths) {
        if (savedPath.trimmed().isEmpty()) {
            continue;
        }
        const QString path = normalizedDirectoryPath(savedPath);
        if (!QFileInfo(path).isDir()) {
            continue;
        }
        const QString key = historyPathKey(path);
        if (seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        paths.append(path);
        if (paths.size() >= MaxRecentRoots) {
            break;
        }
    }

    if (paths != savedPaths) {
        settings.setValue(QStringLiteral("startup/recentRoots"), paths);
    }
    return paths;
}

void rememberRootPath(const QString& rootPath)
{
    constexpr int MaxRecentRoots = 20;
    const QString path = normalizedDirectoryPath(rootPath);
    if (!QFileInfo(path).isDir()) {
        return;
    }

    QStringList paths = recentRootPaths();
    const QString key = historyPathKey(path);
    for (auto it = paths.begin(); it != paths.end();) {
        if (historyPathKey(*it) == key) {
            it = paths.erase(it);
        } else {
            ++it;
        }
    }
    paths.prepend(path);
    while (paths.size() > MaxRecentRoots) {
        paths.removeLast();
    }

    QSettings settings;
    settings.setValue(QStringLiteral("startup/recentRoots"), paths);
}

QString chooseRecentRootPath(QWidget* parent)
{
    const QStringList paths = recentRootPaths();
    if (paths.isEmpty()) {
        return {};
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("履歴から選択"));
    dialog.resize(640, 360);

    auto* layout = new QVBoxLayout(&dialog);
    auto* label = new QLabel(QStringLiteral("ルートフォルダを選択してください。"), &dialog);
    layout->addWidget(label);

    auto* list = new QListWidget(&dialog);
    list->addItems(paths);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    list->setWordWrap(false);
    if (list->count() > 0) {
        list->setCurrentRow(0);
    }
    layout->addWidget(list, 1);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    auto* cancelButton = new QPushButton(QStringLiteral("キャンセル"), &dialog);
    auto* okButton = new QPushButton(QStringLiteral("開く"), &dialog);
    okButton->setDefault(true);
    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(okButton);
    layout->addLayout(buttonRow);

    QObject::connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted || !list->currentItem()) {
        return {};
    }
    return normalizedDirectoryPath(list->currentItem()->text());
}

bool resolveStartupStorageMode(StartupOptions& options)
{
    if (!options.mycelStorageEnabled) {
        rememberRootPath(options.rootPath);
        return true;
    }

    while (!hasMycelFolder(options.rootPath)) {
        QMessageBox box;
        box.setWindowTitle(QStringLiteral("Mycel"));
        box.setIcon(QMessageBox::Question);
        box.setText(QStringLiteral("ルートフォルダに .mycel フォルダがありません。"));
        box.setInformativeText(
            QStringLiteral("Mycel の並び順や色設定を保存するために .mycel フォルダを作成しますか？"));
        QPushButton* createButton = box.addButton(QStringLiteral(".mycel を作成"), QMessageBox::AcceptRole);
        QPushButton* restrictedButton = box.addButton(QStringLiteral(".mycel なしで開く"), QMessageBox::ActionRole);
        QPushButton* historyButton = box.addButton(QStringLiteral("履歴から選択"), QMessageBox::ActionRole);
        QPushButton* chooseButton = box.addButton(QStringLiteral("別フォルダを選択"), QMessageBox::ActionRole);
        QPushButton* quitButton = box.addButton(QStringLiteral("終了"), QMessageBox::RejectRole);
        historyButton->setEnabled(!recentRootPaths().isEmpty());
        box.setDefaultButton(createButton);
        box.setEscapeButton(quitButton);
        box.exec();

        if (box.clickedButton() == createButton) {
            if (createMycelFolder(options.rootPath)) {
                rememberRootPath(options.rootPath);
                return true;
            }
            QMessageBox::warning(nullptr, QStringLiteral("Mycel"),
                                 QStringLiteral(".mycel フォルダを作成できませんでした。"));
            continue;
        }

        if (box.clickedButton() == historyButton) {
            const QString dir = chooseRecentRootPath(nullptr);
            if (!dir.isEmpty()) {
                options.rootPath = dir;
            }
            continue;
        }

        if (box.clickedButton() == chooseButton) {
            const QString dir = QFileDialog::getExistingDirectory(nullptr, QStringLiteral("ルートフォルダを選択"),
                                                                  options.rootPath);
            if (!dir.isEmpty()) {
                options.rootPath = normalizedDirectoryPath(dir);
            }
            continue;
        }

        if (box.clickedButton() == quitButton || box.clickedButton() != restrictedButton) {
            return false;
        }

        options.mycelStorageEnabled = false;
        rememberRootPath(options.rootPath);
        return true;
    }

    rememberRootPath(options.rootPath);
    return true;
}

class MainWindow;

class NodeItem final : public QGraphicsItem {
public:
    NodeItem(Node* node, MainWindow* window) : node_(node), window_(window)
    {
        layoutCenter_ = node_->center;
        setPos(node_->center);
        setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);
        setAcceptDrops(node_->isDir);
        setZValue(NodeLayerZ);
        createPreviewWidget();
    }

    Node* node() const { return node_; }
    QString path() const { return node_ ? node_->path : QString(); }
    QPointF layoutCenter() const { return layoutCenter_; }

    // In-place refresh after a metadata-only relayout: move to the recomputed center, create or
    // drop the inline preview widget to match previewOpen, and repaint. Reusing the live item
    // (instead of recreating the scene) keeps the selection and avoids widget churn.
    void syncFromNode()
    {
        prepareGeometryChange();
        layoutCenter_ = node_->center;
        setPos(node_->center);
        setOpacity(1.0);           // clear any leftover drag visuals
        setZValue(NodeLayerZ);
        const bool wantsPreviewWidget = node_->previewOpen && !node_->isDir;
        if (!wantsPreviewWidget && previewProxy_) {
            delete previewProxy_;
            previewProxy_ = nullptr;
        } else if (wantsPreviewWidget && !previewProxy_) {
            createPreviewWidget();
        } else if (previewProxy_) {
            syncPreviewWidgetGeometry();
        }
        update();
    }
    void setInternalDropHover(bool hover)
    {
        if (internalDropHover_ == hover) {
            return;
        }
        internalDropHover_ = hover;
        update();
    }
    void setLinkDropHover(bool hover)
    {
        if (linkDropHover_ == hover) {
            return;
        }
        linkDropHover_ = hover;
        update();
    }
    QRectF linkDropSceneRect() const
    {
        if (node_->isDir) {
            return {};
        }
        const QRectF box(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                         node_->size.width(), node_->size.height());
        return mapRectToScene(QRectF(box.right() - 28.0, box.top() - 3.0, 34.0, box.height() + 6.0));
    }
    bool containsLinkDropScenePoint(const QPointF& scenePos, qreal margin = 0.0) const
    {
        return linkDropSceneRect().adjusted(-margin, -margin, margin, margin).contains(scenePos);
    }
    bool canResizePreviewAtScene(const QPointF& scenePos) const
    {
        if (!node_->previewOpen || node_->isDir) {
            return false;
        }
        return previewResizeHandle().contains(mapFromScene(scenePos));
    }
    QString imageResizeDebugAtScene(const QPointF& scenePos) const
    {
        const QPointF local = mapFromScene(scenePos);
        const QRectF rect = previewRect();
        const QRectF handle = previewResizeHandle();
        return QStringLiteral("path=%1 previewOpen=%2 isDir=%3 isImage=%4 scene=(%5,%6) local=(%7,%8) rect=(%9,%10,%11,%12) inside=%13 handle=(%14,%15,%16,%17) insideHandle=%18")
            .arg(node_ ? node_->path : QStringLiteral("(null)"))
            .arg(node_ && node_->previewOpen ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(node_ && node_->isDir ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(node_ && isImagePreviewFile(QFileInfo(node_->path)) ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(scenePos.x(), 0, 'f', 1)
            .arg(scenePos.y(), 0, 'f', 1)
            .arg(local.x(), 0, 'f', 1)
            .arg(local.y(), 0, 'f', 1)
            .arg(rect.x(), 0, 'f', 1)
            .arg(rect.y(), 0, 'f', 1)
            .arg(rect.width(), 0, 'f', 1)
            .arg(rect.height(), 0, 'f', 1)
            .arg(rect.contains(local) ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(handle.x(), 0, 'f', 1)
            .arg(handle.y(), 0, 'f', 1)
            .arg(handle.width(), 0, 'f', 1)
            .arg(handle.height(), 0, 'f', 1)
            .arg(handle.contains(local) ? QStringLiteral("true") : QStringLiteral("false"));
    }
    qreal currentImageResizeScale() const
    {
        return imagePreviewScaleForSize(QFileInfo(node_->path), node_->previewSize, false);
    }
    QSizeF currentPreviewSize() const
    {
        return node_ ? node_->previewSize : QSizeF();
    }
    void showContextMenuAt(const QPoint& screenPos);
    void applyImagePreviewScale(qreal scale)
    {
        if (!node_ || node_->isDir || !isImagePreviewFile(QFileInfo(node_->path))) {
            return;
        }
        prepareGeometryChange();
        node_->previewSize = imagePreviewSizeForScale(QFileInfo(node_->path), scale);
        syncPreviewWidgetGeometry();
        update();
    }
    void applyPreviewSize(const QSizeF& size, bool preferHeight)
    {
        if (!node_ || node_->isDir || isImagePreviewFile(QFileInfo(node_->path))) {
            return;
        }
        prepareGeometryChange();
        node_->previewSize = clampedPreviewSizeForFile(QFileInfo(node_->path), size, preferHeight);
        syncPreviewWidgetGeometry();
        update();
    }
    void savePreviewSize(const QSizeF& size, bool preferHeight)
    {
        if (!node_ || node_->isDir || isImagePreviewFile(QFileInfo(node_->path))) {
            return;
        }
        resizePreview(size, preferHeight);
    }
    void saveImagePreviewScale(qreal scale)
    {
        if (!node_ || node_->isDir || !isImagePreviewFile(QFileInfo(node_->path))) {
            return;
        }
        resizeImagePreview(scale);
    }
    bool beginImagePreviewResizeAtScene(const QPointF& scenePos)
    {
        if (!canResizePreviewAtScene(scenePos)) {
            return false;
        }
        resizingPreview_ = true;
        resizePreferHeight_ = false;
        resizeStartScene_ = scenePos;
        resizeStartSize_ = node_->previewSize;
        resizeStartScale_ = imagePreviewScaleForSize(QFileInfo(node_->path), node_->previewSize, false);
        resizeCurrentScale_ = resizeStartScale_;
        setSelected(true);
        update();
        return true;
    }
    void updateImagePreviewResizeAtScene(const QPointF& scenePos)
    {
        if (!resizingPreview_ || node_->isDir) {
            return;
        }
        prepareGeometryChange();
        const QPointF delta = scenePos - resizeStartScene_;
        const QFileInfo info(node_->path);
        const bool preferHeight = std::abs(delta.y()) > std::abs(delta.x());
        const QSizeF targetSize = preferHeight
                                      ? QSizeF(resizeStartSize_.width(),
                                               resizeStartSize_.height() + delta.y())
                                      : QSizeF(resizeStartSize_.width() + delta.x(),
                                               resizeStartSize_.height());
        resizeCurrentScale_ = imagePreviewScaleForSize(info, targetSize, preferHeight);
        node_->previewSize = imagePreviewSizeForScale(info, resizeCurrentScale_);
        resizeCurrentScale_ = imagePreviewScaleForSize(info, node_->previewSize, false);
        syncPreviewWidgetGeometry();
        update();
    }
    void finishImagePreviewResize()
    {
        if (!resizingPreview_) {
            return;
        }
        resizingPreview_ = false;
        resizeImagePreview(resizeCurrentScale_);
    }
    QRectF labelSceneRect() const
    {
        const QRectF box(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                         node_->size.width(), node_->size.height());
        const qreal leftPadding = node_->isDir ? 66.0 : 40.0;
        return mapRectToScene(QRectF(box.left() + leftPadding - 3.0, box.top() + 4.0,
                                     box.width() - leftPadding - 8.0, box.height() - 8.0));
    }
    void refreshPreviewWidget(const QSizeF& previewSize)
    {
        if (!node_->previewOpen || node_->isDir) {
            return;
        }
        if (node_->previewSize != previewSize) {
            prepareGeometryChange();
            node_->previewSize = previewSize;
        }
        delete previewProxy_;
        previewProxy_ = nullptr;
        createPreviewWidget();
        update();
    }

    QRectF boundingRect() const override
    {
        QRectF rect(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                    node_->size.width(), node_->size.height());
        if (node_->collapsed && node_->hiddenChildren > 0) {
            rect.setRight(rect.right() + 104.0);
        }
        if (node_->previewOpen && !node_->isDir) {
            rect = rect.united(previewFrameRect());
        }
        // The selection outline and drop-hover outlines are stroked a few pixels OUTSIDE the
        // node box (see paint(): box.adjusted(-5,-5,5,5) with a 3px pen is the widest). Grow the
        // bounding rect to enclose them, otherwise deselecting only invalidates the inner box and
        // leaves the outline's outer edge painted on the canvas as a stale frame.
        return rect.adjusted(-8.0, -8.0, 8.0, 8.0);
    }

    QPainterPath shape() const override
    {
        QPainterPath path;
        const QRectF box(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                         node_->size.width(), node_->size.height());
        path.addRoundedRect(box, 8.0, 8.0);
        if (node_->previewOpen && !node_->isDir) {
            path.addRect(previewFrameRect());
        }
        if (node_->collapsed && node_->hiddenChildren > 0) {
            path.addRect(QRectF(box.right(), box.top(), 104.0, box.height()));
        }
        return path;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        const ThemeColors colors = currentThemeColors();
        painter->setRenderHint(QPainter::Antialiasing, !fastCanvasRendering());
        const QRectF box(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                         node_->size.width(), node_->size.height());
        if (!node_->isDir && node_->previewOpen) {
            paintPreviewFrame(painter);
            return;
        }

        if (hasUserFill()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(windowFill());
            painter->drawRoundedRect(box.adjusted(-4.0, -3.0, 4.0, 3.0), 8.0, 8.0);
        }
        if (isSelected()) {
            painter->setPen(QPen(colors.nodeSelectedBorder, 2.0));
            painter->setBrush(hasUserFill() ? QColor(colors.nodeSelectedFill.red(), colors.nodeSelectedFill.green(), colors.nodeSelectedFill.blue(), 190)
                                            : colors.nodeSelectedFill);
            painter->drawRoundedRect(box.adjusted(-4.0, -3.0, 4.0, 3.0), 8.0, 8.0);
        }

        if (node_->isDir && (externalDropHover_ || internalDropHover_)) {
            painter->setPen(QPen(colors.highlight, 3.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(QColor(colors.highlight.red(), colors.highlight.green(), colors.highlight.blue(), 34));
            painter->drawRoundedRect(box.adjusted(-5.0, -5.0, 5.0, 5.0), 15.0, 15.0);
        }
        if (!node_->isDir && linkDropHover_) {
            const QRectF zone(box.right() - 25.0, box.top() + 3.0, 30.0, box.height() - 6.0);
            painter->setPen(QPen(colors.linkAccent, 2.4, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(QColor(colors.linkAccent.red(), colors.linkAccent.green(), colors.linkAccent.blue(), 42));
            painter->drawRoundedRect(zone, 8.0, 8.0);
            painter->setPen(QPen(colors.linkAccent.darker(currentAppTheme() == AppTheme::Dark ? 90 : 130), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawLine(QPointF(zone.center().x() - 5.0, zone.center().y()),
                              QPointF(zone.center().x() + 5.0, zone.center().y()));
        }

        if (node_->isDir) {
            paintFolder(painter, QPointF(box.left() + 18.0, -17.0), node_->depth == 0 ? QColor("#46a4ff") : QColor("#ffc94d"));
        } else {
            paintFile(painter, QPointF(box.left() + 6.0, -14.0), QFileInfo(node_->path));
        }

        // A sub-root (child folder with its own .mycel) gets a Mycel badge on its folder icon.
        if (node_->isSubRoot) {
            static const QPixmap mycelBadge = QPixmap(QStringLiteral(":/icons/mycel.png"))
                                                  .scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            if (!mycelBadge.isNull()) {
                painter->drawPixmap(QPointF(box.left() + 40.0, 1.0), mycelBadge);
            }
        }

        QFont font = painter->font();
        font.setPointSize(node_->depth == 0 ? 17 : (node_->isDir ? 12 : 11));
        painter->setFont(font);
        painter->setPen(colors.nodeText);
        painter->drawText(QRectF(box.left() + (node_->isDir ? 66.0 : 40.0), box.top(),
                                 box.width() - (node_->isDir ? 76.0 : 42.0), box.height()),
                          Qt::AlignVCenter | Qt::AlignLeft, shortLabel(node_->name));

        if (node_->collapsed && node_->hiddenChildren > 0) {
            const QRectF badge(box.right() + 8.0, -13.0, 88.0, 26.0);
            painter->setPen(QPen(colors.badgeBorder, 1.1));
            painter->setBrush(colors.badgeBackground);
            painter->drawRoundedRect(badge, 10.0, 10.0);
            QFont badgeFont = painter->font();
            badgeFont.setPointSize(10);
            badgeFont.setBold(true);
            painter->setFont(badgeFont);
            painter->setPen(colors.badgeText);
            painter->drawText(badge, Qt::AlignCenter, QStringLiteral("配下 %1 件").arg(node_->hiddenChildren));
        } else if (node_->hiddenChildren > 0) {
            painter->setPen(colors.badgeText);
            painter->drawText(QRectF(box.right() - 50.0, box.top(), 44.0, box.height()),
                              Qt::AlignCenter, QStringLiteral("+%1").arg(node_->hiddenChildren));
        }

        if (node_->previewOpen) {
            paintInlinePreview(painter, previewRect());
        }
    }

    // Drag gesture driven by BoardView (single owner). The view reliably receives
    // press/move/release, but the item's implicit mouse grab can be lost mid-drag, so
    // the item no longer overrides the mouse events. It just exposes these driver hooks.
    void beginDrag(const QPointF& scenePos, Qt::KeyboardModifiers modifiers);
    void updateDrag(const QPointF& scenePos);
    void finishDrag(Qt::MouseButton button, const QPointF& scenePos);
    // Right-click selects this node (or keeps it if already part of the selection) so the
    // context menu acts on it and the selection frame stays visible.
    void selectForContextMenu();
    // Default action (double-click): toggle a folder's collapse or a file's inline preview.
    // Driven by BoardView, since the view owns the press that would otherwise let
    // QGraphicsView deliver the double-click here.
    void activate();

protected:
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
    void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
    void dropEvent(QGraphicsSceneDragDropEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    QColor windowColor() const;
    QColor windowFill() const;
    bool hasUserFill() const;
    QStringList windowInlinePreviewLines() const;
    QString windowMarkdownPreviewText() const;
    bool windowIsDocumentThumbnail(const QFileInfo& info) const;
    QPixmap windowDocumentThumbnail(const QFileInfo& info) const;
    void updateDragAtScene(const QPointF& scenePos);
    void finishDragAtScene(Qt::MouseButton button, const QPointF& scenePos);
    void resizePreview(const QSizeF& size, bool preferHeight);
    void resizeImagePreview(qreal scale);
    void createPreviewWidget();
    void syncPreviewWidgetGeometry();

    static constexpr qreal PreviewHeaderHeight = 34.0;

    QRectF previewFrameRect() const
    {
        return previewHeaderRect().united(previewRect());
    }

    QRectF previewHeaderRect() const
    {
        const QRectF box(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                         node_->size.width(), node_->size.height());
        return QRectF(box.left(), box.top(), node_->size.width(), PreviewHeaderHeight);
    }

    QRectF previewRect() const
    {
        const QRectF header = previewHeaderRect();
        return QRectF(header.left(), header.bottom(),
                      node_->previewSize.width(), node_->previewSize.height());
    }

    QRectF previewResizeHandle() const
    {
        const QRectF rect = previewRect();
        const qreal grip = isImagePreviewFile(QFileInfo(node_->path)) ? 36.0 : 28.0;
        return QRectF(rect.right() - grip, rect.bottom() - grip, grip, grip);
    }

    void paintFolder(QPainter* painter, const QPointF& at, const QColor& fill)
    {
        QPainterPath path;
        path.moveTo(at.x(), at.y() + 9.0);
        path.quadTo(at.x(), at.y() + 4.0, at.x() + 5.0, at.y() + 4.0);
        path.lineTo(at.x() + 16.0, at.y() + 4.0);
        path.lineTo(at.x() + 22.0, at.y() + 10.0);
        path.lineTo(at.x() + 36.0, at.y() + 10.0);
        path.quadTo(at.x() + 40.0, at.y() + 10.0, at.x() + 40.0, at.y() + 14.0);
        path.lineTo(at.x() + 40.0, at.y() + 31.0);
        path.quadTo(at.x() + 40.0, at.y() + 36.0, at.x() + 35.0, at.y() + 36.0);
        path.lineTo(at.x() + 5.0, at.y() + 36.0);
        path.quadTo(at.x(), at.y() + 36.0, at.x(), at.y() + 31.0);
        path.closeSubpath();

        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawPath(path);
        painter->setPen(QPen(QColor("#f6b93f"), 1.8));
        painter->drawLine(QPointF(at.x() + 3.0, at.y() + 14.0), QPointF(at.x() + 37.0, at.y() + 14.0));
    }

    void paintFile(QPainter* painter, const QPointF& at, const QFileInfo& info)
    {
        const ThemeColors colors = currentThemeColors();
        QPainterPath page;
        page.moveTo(at);
        page.lineTo(at.x() + 16.0, at.y());
        page.lineTo(at.x() + 26.0, at.y() + 10.0);
        page.lineTo(at.x() + 26.0, at.y() + 32.0);
        page.lineTo(at.x(), at.y() + 32.0);
        page.closeSubpath();

        painter->setPen(QPen(colors.fileStroke, 1.3));
        painter->setBrush(colors.filePage);
        painter->drawPath(page);
        painter->drawLine(QPointF(at.x() + 16.0, at.y()), QPointF(at.x() + 16.0, at.y() + 10.0));
        painter->drawLine(QPointF(at.x() + 16.0, at.y() + 10.0), QPointF(at.x() + 26.0, at.y() + 10.0));

        painter->setPen(QPen(colors.fileInk, 1.1));
        const bool image = isImagePreviewFile(info);
        if (image) {
            painter->drawRect(QRectF(at.x() + 5.0, at.y() + 15.0, 15.0, 10.0));
            QPainterPath mountain;
            mountain.moveTo(at.x() + 6.5, at.y() + 24.0);
            mountain.lineTo(at.x() + 11.0, at.y() + 19.5);
            mountain.lineTo(at.x() + 14.0, at.y() + 22.0);
            mountain.lineTo(at.x() + 17.5, at.y() + 18.0);
            mountain.lineTo(at.x() + 20.0, at.y() + 24.0);
            painter->drawPath(mountain);
            return;
        }

        const FileKindStyle kind = fileKindStyleFor(info);
        if (!kind.badge.isEmpty()) {
            QFont badgeFont = painter->font();
            badgeFont.setPointSize(kind.badge.size() > 2 ? 5 : 7);
            badgeFont.setBold(true);
            painter->setFont(badgeFont);
            painter->setPen(QPen(kind.accent.isValid() ? kind.accent : colors.fileInk, 1.1));
            painter->drawText(QRectF(at.x() + 3.0, at.y() + 14.0, 20.0, 13.0), Qt::AlignCenter, kind.badge);
            return;
        }

        painter->drawLine(QPointF(at.x() + 5.0, at.y() + 17.0), QPointF(at.x() + 19.0, at.y() + 17.0));
        painter->drawLine(QPointF(at.x() + 5.0, at.y() + 22.0), QPointF(at.x() + 17.0, at.y() + 22.0));
    }

    void paintInlinePreview(QPainter* painter, const QRectF& rect)
    {
        const ThemeColors colors = currentThemeColors();
        QColor border = hasUserFill() ? colors.inlinePreviewBorder : colors.inlinePreviewBorder;
        border.setAlpha(hasUserFill() ? 180 : 255);
        painter->setPen(QPen(border, 1.1));
        painter->setBrush(hasUserFill() ? windowFill() : colors.inlinePreviewBackground);
        painter->drawRect(rect);

        painter->setPen(QPen(colors.inlinePreviewBorder, 1.2));
        const QPointF corner(rect.right() - 12.0, rect.bottom() - 4.0);
        painter->drawLine(corner, QPointF(rect.right() - 4.0, rect.bottom() - 12.0));
        painter->drawLine(QPointF(rect.right() - 8.0, rect.bottom() - 4.0),
                          QPointF(rect.right() - 4.0, rect.bottom() - 8.0));
    }

    void paintPreviewFrame(QPainter* painter)
    {
        const ThemeColors colors = currentThemeColors();
        const QRectF header = previewHeaderRect();
        const QRectF body = previewRect();
        const bool selected = isSelected();
        const QColor border = selected ? colors.nodeSelectedBorder : (hasUserFill() ? windowColor() : colors.nodeStroke);
        const QColor bodyFill = selected ? colors.previewPanel : colors.inlinePreviewBackground;
        const QColor headerFill = selected ? colors.nodeSelectedFill : (hasUserFill() ? windowFill() : colors.nodeFill);
        constexpr qreal radius = 4.0;

        painter->setPen(QPen(border, selected ? 2.0 : 1.4));
        painter->setBrush(bodyFill);
        painter->drawRoundedRect(body, radius, radius);

        painter->setPen(Qt::NoPen);
        painter->setBrush(headerFill);
        painter->drawRoundedRect(header.adjusted(1.0, 1.0, -1.0, -1.0), radius, radius);

        painter->setPen(QPen(border, selected ? 2.0 : 1.4));
        painter->drawRoundedRect(header, radius, radius);

        painter->save();
        painter->translate(QPointF(header.left() + 12.0, header.top() + 6.0));
        painter->scale(0.68, 0.68);
        paintFile(painter, QPointF(0.0, 0.0), QFileInfo(node_->path));
        painter->restore();

        QFont font = painter->font();
        font.setPointSize(node_->depth == 0 ? 14 : 11);
        font.setBold(true);
        painter->setFont(font);
        painter->setPen(colors.nodeText);
        painter->drawText(QRectF(header.left() + 40.0, header.top(),
                                 header.width() - 48.0, header.height()),
                          Qt::AlignVCenter | Qt::AlignLeft, shortLabel(node_->name));

        const QFileInfo info(node_->path);
        QPixmap previewPixmap;
        if (isImagePreviewFile(info)) {
            previewPixmap = cachedImagePixmapForFile(info);
        } else if (windowIsDocumentThumbnail(info)) {
            previewPixmap = windowDocumentThumbnail(info);  // PDF first-page thumbnail
        }
        if (!previewPixmap.isNull()) {
            const QRectF imageArea = body.adjusted(6.0, 6.0, -6.0, -6.0);  // symmetric → centered
            if (imageArea.width() > 1.0 && imageArea.height() > 1.0) {
                painter->save();
                painter->setRenderHint(QPainter::SmoothPixmapTransform, !fastCanvasRendering());
                const QSizeF scaled = previewPixmap.size().scaled(imageArea.size().toSize(), Qt::KeepAspectRatio);
                const QRectF target(QPointF(imageArea.left() + (imageArea.width() - scaled.width()) / 2.0,
                                            imageArea.top() + (imageArea.height() - scaled.height()) / 2.0),
                                    scaled);
                painter->drawPixmap(target, previewPixmap, QRectF(QPointF(0.0, 0.0), previewPixmap.size()));
                painter->restore();
            }
        }

        painter->setPen(QPen(colors.inlinePreviewBorder, 1.2));
        const QPointF corner(body.right() - 12.0, body.bottom() - 4.0);
        painter->drawLine(corner, QPointF(body.right() - 4.0, body.bottom() - 12.0));
        painter->drawLine(QPointF(body.right() - 8.0, body.bottom() - 4.0),
                          QPointF(body.right() - 4.0, body.bottom() - 8.0));
    }

    Node* node_;
    MainWindow* window_;
    QPointF layoutCenter_;
    QPointF dragStart_;
    QPointF pressStartScene_;
    Qt::KeyboardModifiers pressModifiers_ = Qt::NoModifier;
    bool dragMoveLogged_ = false;
    bool resizingPreview_ = false;
    bool resizePreferHeight_ = false;
    qreal resizeStartScale_ = 1.0;
    qreal resizeCurrentScale_ = 1.0;
    bool externalDropHover_ = false;
    bool internalDropHover_ = false;
    bool linkDropHover_ = false;
    QPointF resizeStartScene_;
    QSizeF resizeStartSize_;
    QGraphicsProxyWidget* previewProxy_ = nullptr;
};

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

class MindMapScene final : public QGraphicsScene {
protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override
    {
        const ThemeColors colors = currentThemeColors();
        painter->fillRect(rect, colors.canvasBackground);
        QColor gridColor = currentAppTheme() == AppTheme::Dark ? QColor(69, 78, 88, 150) : QColor(216, 213, 205, 170);
        painter->setPen(QPen(gridColor, 1.0));
        const int grid = 28;
        const qreal left = std::floor(rect.left() / grid) * grid;
        const qreal top = std::floor(rect.top() / grid) * grid;
        QVector<QPointF> points;
        points.reserve(static_cast<int>((rect.width() / grid + 2.0) * (rect.height() / grid + 2.0)));
        for (qreal x = left; x < rect.right(); x += grid) {
            for (qreal y = top; y < rect.bottom(); y += grid) {
                points.append(QPointF(x, y));
            }
        }
        painter->drawPoints(points.constData(), points.size());
    }
};

// Owns the single in-progress node-drag session. BoardView routes its press/move/release
// here so there is one place that knows which node is being dragged; the per-node motion
// and drop logic still lives on NodeItem (beginDrag/updateDrag/finishDrag).
class DragController {
public:
    bool active() const { return node_ != nullptr; }

    void begin(NodeItem* node, const QPointF& scenePos, Qt::KeyboardModifiers modifiers)
    {
        node_ = node;
        if (node_) {
            node_->beginDrag(scenePos, modifiers);
        }
    }

    void update(const QPointF& scenePos)
    {
        if (node_) {
            node_->updateDrag(scenePos);
        }
    }

    void finish(Qt::MouseButton button, const QPointF& scenePos)
    {
        NodeItem* node = node_;
        node_ = nullptr;  // cleared first: finishDrag may rebuild and delete the item
        if (node) {
            node->finishDrag(button, scenePos);
        }
    }

    // Drop the session without finishing it — used when a rebuild is about to delete the
    // dragged item, so the pointer never dangles.
    void cancel() { node_ = nullptr; }

private:
    NodeItem* node_ = nullptr;
};

class BoardView final : public QGraphicsView {
public:
    explicit BoardView(QWidget* parent = nullptr) : QGraphicsView(parent)
    {
        setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        setDragMode(QGraphicsView::NoDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorUnderMouse);
        setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
        setCacheMode(QGraphicsView::CacheBackground);
        setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
        setFocusPolicy(Qt::StrongFocus);
        setAcceptDrops(true);
        viewport()->setAcceptDrops(true);
        fastRenderTimer_.setSingleShot(true);
        fastRenderTimer_.setInterval(120);
        connect(&fastRenderTimer_, &QTimer::timeout, this, [this] { endFastRendering(); });
    }

    void setCheatSheetHandler(std::function<void()> handler)
    {
        cheatSheetHandler_ = std::move(handler);
    }

    void setKeyHandler(std::function<bool(QKeyEvent*)> handler)
    {
        keyHandler_ = std::move(handler);
    }

    void setViewChangedHandler(std::function<void()> handler)
    {
        viewChangedHandler_ = std::move(handler);
    }

    void setDebugHandler(std::function<void(const QString&)> handler)
    {
        debugHandler_ = std::move(handler);
    }

    void cancelTransientNodeInteraction()
    {
        imageResizePath_.clear();
        // A rebuild destroys every NodeItem, so any in-progress drag pointer would
        // dangle. Drop it here (renderCurrentTree calls this before scene_.clear()).
        dragController_.cancel();
    }

    bool isDraggingNode() const { return dragController_.active(); }

protected:
    void wheelEvent(QWheelEvent* event) override
    {
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
        if (!(event->modifiers() & Qt::ControlModifier)) {
            const QPoint pixelDelta = event->pixelDelta();
            if (!pixelDelta.isNull()) {
                beginFastRendering();
                horizontalScrollBar()->setValue(horizontalScrollBar()->value() - pixelDelta.x());
                verticalScrollBar()->setValue(verticalScrollBar()->value() - pixelDelta.y());
                notifyViewChanged();
                event->accept();
                return;
            }
        }
#endif
        const QPoint delta = event->angleDelta();
        if (!delta.isNull()) {
            beginFastRendering();
            const int amount = delta.y();
            zoomAt(event->position(), amount > 0 ? 1.12 : 0.89);
            event->accept();
            return;
        }
        QGraphicsView::wheelEvent(event);
    }

    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (!textInputWidgetHasFocus() && isBoardNavigationKey(keyEvent)) {
                if (keyHandler_ && keyHandler_(keyEvent)) {
                    notifyDebug(QStringLiteral("board event key handled by node navigation: %1").arg(debugKeyName(keyEvent)));
                    event->accept();
                    return true;
                }
                if (scrollWithArrowKey(keyEvent)) {
                    event->accept();
                    return true;
                }
            }
        }
        if (event->type() == QEvent::NativeGesture) {
            auto* gesture = static_cast<QNativeGestureEvent*>(event);
            if (gesture->gestureType() == Qt::ZoomNativeGesture) {
                const qreal factor = 1.0 + gesture->value();
                if (factor > 0.0) {
                    beginFastRendering();
                    zoomAt(gesture->position(), factor);
                }
                event->accept();
                return true;
            }
        }
        return QGraphicsView::event(event);
    }

    void contextMenuEvent(QContextMenuEvent* event) override
    {
        if (NodeItem* nodeItem = nodeItemAt(event->pos())) {
            nodeItem->showContextMenuAt(event->globalPos());
            event->accept();
            return;
        }
        QGraphicsView::contextMenuEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            const bool directItemHit = itemAt(event->pos()) != nullptr;
            notifyDebug(QStringLiteral("view mouse press itemAt: %1")
                            .arg(directItemHit ? QStringLiteral("item") : QStringLiteral("none")));
            if (NodeItem* nodeItem = nodeItemAt(event->pos())) {
                notifyDebug(QStringLiteral("view mouse press node hit: %1 direct=%2")
                                .arg(nodeItem->path(),
                                     directItemHit ? QStringLiteral("true") : QStringLiteral("false")));
                notifyDebug(QStringLiteral("image resize candidate: %1")
                                .arg(nodeItem->imageResizeDebugAtScene(mapToScene(event->pos()))));
                const QPointF scenePos = mapToScene(event->pos());
                if (nodeItem->canResizePreviewAtScene(scenePos)) {
                    imageResizePath_ = nodeItem->path();
                    imageResizeStartScene_ = scenePos;
                    imageResizeStartSize_ = nodeItem->currentPreviewSize();
                    imageResizeStartScale_ = nodeItem->currentImageResizeScale();
                    imageResizeCurrentScale_ = imageResizeStartScale_;
                    notifyDebug(QStringLiteral("image resize begin scale=%1 size=(%2,%3)")
                                    .arg(imageResizeCurrentScale_, 0, 'f', 4)
                                    .arg(nodeItem->currentPreviewSize().width(), 0, 'f', 1)
                                    .arg(nodeItem->currentPreviewSize().height(), 0, 'f', 1));
                    event->accept();
                    return;
                }
                // The view owns the node drag end to end, so the release is always delivered
                // here even if the item's implicit grab is lost. Start a drag when the press
                // is on the node body — that includes the case where itemAt() missed entirely
                // (a frequent quirk that nodeItemAt resolves by geometry). Only pass through
                // when itemAt() reports a different item, i.e. a child preview proxy widget,
                // so the preview stays interactive. Alt+drag is reserved for panning.
                QGraphicsItem* hitItem = itemAt(event->pos());
                const bool onChildWidget = hitItem != nullptr && hitItem != nodeItem;
                if (!(event->modifiers() & Qt::AltModifier) && !onChildWidget) {
                    dragController_.begin(nodeItem, scenePos, event->modifiers());
                    event->accept();
                    return;
                }
            } else {
                notifyDebug(QStringLiteral("image resize candidate: no NodeItem"));
            }
        }
        const bool rightButtonCanvasPan = event->button() == Qt::RightButton &&
                                          itemAt(event->pos()) == nullptr &&
                                          nodeItemAt(event->pos()) == nullptr;
        if ((event->button() == Qt::MiddleButton) ||
            rightButtonCanvasPan ||
            (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier))) {
            panning_ = true;
            panningButton_ = event->button();
            lastPanPoint_ = event->pos();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton && itemAt(event->pos()) == nullptr) {
            rubberBandSelecting_ = true;
            rubberBandStart_ = event->pos();
            setDragMode(QGraphicsView::RubberBandDrag);
            QGraphicsView::mousePressEvent(event);
            return;
        }
        // Right-click on a node: establish/keep its selection and consume the press so the scene's
        // default press handling can't clear the selection (the context menu still fires on
        // right-button release). Without this the highlight frame vanishes on right-click.
        if (event->button() == Qt::RightButton) {
            if (NodeItem* nodeItem = nodeItemAt(event->pos())) {
                nodeItem->selectForContextMenu();
                event->accept();
                return;
            }
        }
        QGraphicsView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!imageResizePath_.isEmpty()) {
            NodeItem* resizeItem = nodeItemForPath(imageResizePath_);
            if (!resizeItem) {
                notifyDebug(QStringLiteral("image resize move canceled: item missing"));
                imageResizePath_.clear();
                event->accept();
                return;
            }
            const QPointF delta = mapToScene(event->pos()) - imageResizeStartScene_;
            const QSizeF targetSize = previewResizeTargetSize(QFileInfo(imageResizePath_),
                                                              imageResizeStartSize_, delta);
            const bool preferHeight = std::abs(delta.y()) > std::abs(delta.x());
            if (isImagePreviewFile(QFileInfo(imageResizePath_))) {
                imageResizeCurrentScale_ = imagePreviewScaleForSize(QFileInfo(imageResizePath_),
                                                                    targetSize,
                                                                    preferHeight);
                resizeItem->applyImagePreviewScale(imageResizeCurrentScale_);
                imageResizeCurrentScale_ = resizeItem->currentImageResizeScale();
            } else {
                resizeItem->applyPreviewSize(targetSize, preferHeight);
            }
            notifyDebug(QStringLiteral("image resize move scale=%1 size=(%2,%3)")
                            .arg(imageResizeCurrentScale_, 0, 'f', 4)
                            .arg(resizeItem->currentPreviewSize().width(), 0, 'f', 1)
                            .arg(resizeItem->currentPreviewSize().height(), 0, 'f', 1));
            event->accept();
            return;
        }
        if (dragController_.active() && (event->buttons() & Qt::LeftButton)) {
            dragController_.update(mapToScene(event->pos()));
            event->accept();
            return;
        }
        if (panning_) {
            beginFastRendering();
            const QPoint delta = event->pos() - lastPanPoint_;
            lastPanPoint_ = event->pos();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            event->accept();
            return;
        }
        QGraphicsView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        notifyDebug(QStringLiteral("view mouse release button=%1 imageResize=%2")
                        .arg(static_cast<int>(event->button()))
                        .arg(imageResizePath_.isEmpty() ? QStringLiteral("(none)") : imageResizePath_));
        if (event->button() == Qt::LeftButton && !imageResizePath_.isEmpty()) {
            if (NodeItem* resizeItem = nodeItemForPath(imageResizePath_)) {
                if (isImagePreviewFile(QFileInfo(imageResizePath_))) {
                    resizeItem->saveImagePreviewScale(imageResizeCurrentScale_);
                } else {
                    const QPointF delta = mapToScene(event->pos()) - imageResizeStartScene_;
                    const bool preferHeight = std::abs(delta.y()) > std::abs(delta.x());
                    const QSizeF targetSize = previewResizeTargetSize(QFileInfo(imageResizePath_),
                                                                      imageResizeStartSize_, delta);
                    resizeItem->savePreviewSize(targetSize, preferHeight);
                }
            } else {
                notifyDebug(QStringLiteral("image resize finish canceled: item missing"));
            }
            imageResizePath_.clear();
            notifyDebug(QStringLiteral("image resize finish"));
            event->accept();
            return;
        }
        if (dragController_.active() && event->button() == Qt::LeftButton) {
            notifyDebug(QStringLiteral("view finalize node drag"));
            dragController_.finish(Qt::LeftButton, mapToScene(event->pos()));
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton && rubberBandSelecting_) {
            QGraphicsView::mouseReleaseEvent(event);
            rememberRubberBandRect(event->pos());
            applyRubberBandSelection(event->modifiers());
            setDragMode(QGraphicsView::NoDrag);
            rubberBandSelecting_ = false;
            return;
        }
        if (event->button() == panningButton_ && panning_) {
            panning_ = false;
            panningButton_ = Qt::NoButton;
            unsetCursor();
            notifyViewChanged();
            event->accept();
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        // The view owns the press, so QGraphicsView no longer delivers double-clicks to the
        // node. Drive the node's default action here. Pass through when the press is on a
        // child preview proxy so double-clicking preview text keeps its normal behaviour.
        if (event->button() == Qt::LeftButton) {
            if (NodeItem* nodeItem = nodeItemAt(event->pos())) {
                QGraphicsItem* hitItem = itemAt(event->pos());
                const bool onChildWidget = hitItem != nullptr && hitItem != nodeItem;
                if (!onChildWidget) {
                    nodeItem->activate();
                    event->accept();
                    return;
                }
            }
        }
        QGraphicsView::mouseDoubleClickEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (textInputWidgetHasFocus()) {
            QGraphicsView::keyPressEvent(event);
            return;
        }
        if (event->key() == Qt::Key_Plus) {
            zoomAt(viewport()->rect().center(), 1.12);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Minus) {
            zoomAt(viewport()->rect().center(), 0.89);
            event->accept();
            return;
        }
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
            lastRubberBandSceneRect_.isValid()) {
            zoomToRect(lastRubberBandSceneRect_);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Question && cheatSheetHandler_) {
            cheatSheetHandler_();
            event->accept();
            return;
        }
        if (keyHandler_ && keyHandler_(event)) {
            notifyDebug(QStringLiteral("board keyPress handled by node navigation: %1").arg(debugKeyName(event)));
            event->accept();
            return;
        }
        if (scrollWithArrowKey(event)) {
            event->accept();
            return;
        }
        QGraphicsView::keyPressEvent(event);
    }

private:
    NodeItem* nodeItemAt(const QPoint& pos) const
    {
        for (QGraphicsItem* item = itemAt(pos); item; item = item->parentItem()) {
            if (auto* nodeItem = dynamic_cast<NodeItem*>(item)) {
                return nodeItem;
            }
        }
        // QGraphicsView::itemAt() can miss a node (notably when a preview proxy widget is
        // present), which would make a file-name click look like an empty-canvas click. Fall
        // back to a direct geometry test against every visible node.
        if (!scene()) {
            return nullptr;
        }
        const QPointF scenePos = mapToScene(pos);
        for (QGraphicsItem* item : scene()->items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem || !nodeItem->isVisible()) {
                continue;
            }
            const QPointF localPos = nodeItem->mapFromScene(scenePos);
            if (nodeItem->contains(localPos) ||
                nodeItem->boundingRect().adjusted(-2.0, -2.0, 2.0, 2.0).contains(localPos)) {
                return nodeItem;
            }
        }
        return nullptr;
    }

    NodeItem* nodeItemForPath(const QString& path) const
    {
        if (!scene()) {
            return nullptr;
        }
        for (QGraphicsItem* item : scene()->items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->path() == path) {
                return nodeItem;
            }
        }
        return nullptr;
    }

    void rememberRubberBandRect(const QPoint& endPos)
    {
        const QRect viewportRect = QRect(rubberBandStart_, endPos).normalized();
        if (viewportRect.width() < 8 || viewportRect.height() < 8) {
            lastRubberBandSceneRect_ = QRectF();
            return;
        }

        lastRubberBandSceneRect_ = mapToScene(viewportRect).boundingRect();
    }

    void applyRubberBandSelection(Qt::KeyboardModifiers modifiers)
    {
        QGraphicsScene* currentScene = scene();
        if (!currentScene) {
            return;
        }

        if (!lastRubberBandSceneRect_.isValid()) {
            if (!(modifiers & Qt::ControlModifier)) {
                currentScene->clearSelection();
            }
            return;
        }

        if (!(modifiers & Qt::ControlModifier)) {
            currentScene->clearSelection();
        }

        for (QGraphicsItem* item :
             currentScene->items(lastRubberBandSceneRect_, Qt::IntersectsItemBoundingRect)) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem) {
                nodeItem->setSelected(true);
            }
        }
    }

    void zoomToRect(const QRectF& sceneRect)
    {
        if (!sceneRect.isValid() || sceneRect.width() <= 1.0 || sceneRect.height() <= 1.0) {
            return;
        }

        resetTransform();
        fitInView(sceneRect.adjusted(-40.0, -40.0, 40.0, 40.0), Qt::KeepAspectRatio);
        notifyViewChanged();
    }

    void zoomAt(const QPointF& viewportPos, qreal factor)
    {
        const qreal currentScale = transform().m11();
        const qreal targetScale = std::clamp(currentScale * factor, 0.15, 4.5);
        if (std::abs(targetScale - currentScale) < 0.0001) {
            return;
        }

        const QPointF sceneBefore = mapToScene(viewportPos.toPoint());
        const qreal appliedFactor = targetScale / currentScale;
        scale(appliedFactor, appliedFactor);
        const QPointF sceneAfter = mapToScene(viewportPos.toPoint());
        const QPointF delta = sceneAfter - sceneBefore;
        translate(delta.x(), delta.y());
        notifyViewChanged();
    }

    void notifyViewChanged()
    {
        if (viewChangedHandler_) {
            viewChangedHandler_();
        }
    }

    void beginFastRendering()
    {
        if (!g_fastCanvasRendering) {
            g_fastCanvasRendering = true;
            setRenderHints(QPainter::TextAntialiasing);
            viewport()->update();
        }
        fastRenderTimer_.start();
    }

    void endFastRendering()
    {
        if (!g_fastCanvasRendering) {
            return;
        }
        g_fastCanvasRendering = false;
        setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        viewport()->update();
    }

    void notifyDebug(const QString& message)
    {
        if (debugHandler_) {
            debugHandler_(message);
        }
    }

    bool scrollWithArrowKey(QKeyEvent* event)
    {
        if (!event || event->modifiers() != Qt::NoModifier || !isArrowKey(event->key())) {
            return false;
        }

        constexpr int scrollStep = 56;
        const int oldH = horizontalScrollBar()->value();
        const int oldV = verticalScrollBar()->value();
        switch (event->key()) {
        case Qt::Key_Up:
            beginFastRendering();
            verticalScrollBar()->setValue(verticalScrollBar()->value() - scrollStep);
            break;
        case Qt::Key_Down:
            beginFastRendering();
            verticalScrollBar()->setValue(verticalScrollBar()->value() + scrollStep);
            break;
        case Qt::Key_Left:
            beginFastRendering();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - scrollStep);
            break;
        case Qt::Key_Right:
            beginFastRendering();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() + scrollStep);
            break;
        default:
            return false;
        }
        const int newH = horizontalScrollBar()->value();
        const int newV = verticalScrollBar()->value();
        notifyDebug(QStringLiteral("arrow scroll %1: h %2 -> %3 / %4, v %5 -> %6 / %7%8")
                        .arg(debugKeyName(event))
                        .arg(oldH)
                        .arg(newH)
                        .arg(horizontalScrollBar()->maximum())
                        .arg(oldV)
                        .arg(newV)
                        .arg(verticalScrollBar()->maximum())
                        .arg(oldH == newH && oldV == newV ? QStringLiteral(" (unchanged)") : QString()));
        notifyViewChanged();
        return true;
    }

    static QString debugKeyName(QKeyEvent* event)
    {
        if (!event) {
            return QStringLiteral("(none)");
        }
        QString key = QKeySequence(event->key()).toString(QKeySequence::NativeText);
        if (key.isEmpty()) {
            key = QStringLiteral("key=%1").arg(event->key());
        }
        return key;
    }

    static bool isArrowKey(int key)
    {
        return key == Qt::Key_Up || key == Qt::Key_Down ||
               key == Qt::Key_Left || key == Qt::Key_Right;
    }

    static bool isBoardNavigationKey(QKeyEvent* event)
    {
        if (!event) {
            return false;
        }
        const int key = event->key();
        return isArrowKey(key) || key == Qt::Key_Tab || key == Qt::Key_Backtab ||
               key == Qt::Key_Return || key == Qt::Key_Enter;
    }

    bool panning_ = false;
    DragController dragController_;  // owns the in-progress node-drag session
    QString imageResizePath_;
    QPointF imageResizeStartScene_;
    QSizeF imageResizeStartSize_;
    qreal imageResizeStartScale_ = 1.0;
    qreal imageResizeCurrentScale_ = 1.0;
    bool rubberBandSelecting_ = false;
    Qt::MouseButton panningButton_ = Qt::NoButton;
    QPoint lastPanPoint_;
    QPoint rubberBandStart_;
    QRectF lastRubberBandSceneRect_;
    QTimer fastRenderTimer_;
    std::function<void()> cheatSheetHandler_;
    std::function<bool(QKeyEvent*)> keyHandler_;
    std::function<void()> viewChangedHandler_;
    std::function<void(const QString&)> debugHandler_;
};

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

class PreviewText final : public QTextEdit {
public:
    explicit PreviewText(QWidget* parent = nullptr) : QTextEdit(parent)
    {
        TextFontSettings::registerPreview(this);
    }

    ~PreviewText() override
    {
        TextFontSettings::unregisterPreview(this);
    }

protected:
    bool event(QEvent* event) override
    {
        if (handleTextZoomGesture(event, accumulatedZoom_, TextFontSettings::Surface::Preview)) {
            return true;
        }
        return QTextEdit::event(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (handleTextZoomKey(event, TextFontSettings::Surface::Preview)) {
            return;
        }
        QTextEdit::keyPressEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (event->modifiers() & Qt::ControlModifier) {
            const QPoint delta = event->angleDelta();
            const QPoint pixelDelta = event->pixelDelta();
            const int amount = !delta.isNull() ? delta.y() : pixelDelta.y();
            if (amount != 0) {
                TextFontSettings::changeConfiguredPointSize(TextFontSettings::Surface::Preview, amount > 0 ? 1 : -1);
            }
            event->accept();
            return;
        }
        QTextEdit::wheelEvent(event);
    }

private:
    qreal accumulatedZoom_ = 0.0;
};

class YouTubePreview final : public QWidget {
public:
    explicit YouTubePreview(const QString& embedUrl, const QString& thumbnailPath, QWidget* parent = nullptr) : QWidget(parent)
    {
        setProperty("mycelInlinePreview", true);
        setAutoFillBackground(false);
        watchUrl_ = youtubeWatchUrlFromEmbedUrl(embedUrl);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        thumbnailLabel_ = new QLabel(this);
        thumbnailLabel_->setProperty("mycelInlinePreview", true);
        thumbnailLabel_->setAlignment(Qt::AlignCenter);
        thumbnailLabel_->setMinimumSize(1, 1);
        thumbnailLabel_->setStyleSheet(QStringLiteral("background: #111827; color: #e5e7eb;"));
        thumbnailLabel_->setCursor(Qt::PointingHandCursor);
        layout->addWidget(thumbnailLabel_, 1);

        auto* footer = new QWidget(this);
        footer->setProperty("mycelInlinePreview", true);
        auto* footerLayout = new QHBoxLayout(footer);
        footerLayout->setContentsMargins(4, 0, 4, 2);
        footerLayout->addStretch(1);
        auto* openButton = new QPushButton(QStringLiteral("YouTubeで開く"), footer);
        openButton->setCursor(Qt::PointingHandCursor);
        footerLayout->addWidget(openButton);
        layout->addWidget(footer, 0);

        connect(openButton, &QPushButton::clicked, this, [this] {
            QDesktopServices::openUrl(QUrl(watchUrl_));
        });
        thumbnailLabel_->installEventFilter(this);
        originalThumbnail_.load(thumbnailPath);
        if (originalThumbnail_.isNull()) {
            thumbnailLabel_->setText(QStringLiteral("サムネイルがありません"));
        } else {
            updateThumbnailPixmap();
        }
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == thumbnailLabel_ && event->type() == QEvent::MouseButtonRelease) {
            QDesktopServices::openUrl(QUrl(watchUrl_));
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        updateThumbnailPixmap();
    }

private:
    void updateThumbnailPixmap()
    {
        if (!thumbnailLabel_ || originalThumbnail_.isNull()) {
            return;
        }
        const QSize target = thumbnailLabel_->size();
        if (target.isEmpty()) {
            return;
        }
        thumbnailLabel_->setPixmap(originalThumbnail_.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    QLabel* thumbnailLabel_ = nullptr;
    QString watchUrl_;
    QPixmap originalThumbnail_;
};

class AspectImagePreview final : public QLabel {
public:
    explicit AspectImagePreview(QWidget* parent = nullptr) : QLabel(parent)
    {
        setAlignment(Qt::AlignCenter);
        setMinimumSize(1, 1);
        setAutoFillBackground(false);
    }

    void setPreviewPixmap(const QPixmap& pixmap)
    {
        pixmap_ = pixmap;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if (pixmap_.isNull()) {
            QLabel::paintEvent(event);
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QSize targetSize = pixmap_.size().scaled(size(), Qt::KeepAspectRatio);
        const QPoint topLeft((width() - targetSize.width()) / 2,
                             (height() - targetSize.height()) / 2);
        painter.drawPixmap(QRect(topLeft, targetSize), pixmap_);
    }

private:
    QPixmap pixmap_;
};

class InlineVideoPreview final : public QWidget {
public:
    explicit InlineVideoPreview(const QString& path, QWidget* parent = nullptr) : QWidget(parent)
    {
        setProperty("mycelInlinePreview", true);
        setAutoFillBackground(false);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        videoWidget_ = new QVideoWidget(this);
        videoWidget_->setMinimumSize(1, 1);
        layout->addWidget(videoWidget_, 1);

        auto* controls = new QWidget(this);
        controls->setProperty("mycelInlinePreview", true);
        auto* controlsLayout = new QHBoxLayout(controls);
        controlsLayout->setContentsMargins(4, 0, 4, 2);
        controlsLayout->setSpacing(6);

        playButton_ = new QPushButton(QStringLiteral("再生"), controls);
        playButton_->setFixedWidth(54);
        positionSlider_ = new QSlider(Qt::Horizontal, controls);
        positionSlider_->setRange(0, 0);
        controlsLayout->addWidget(playButton_);
        controlsLayout->addWidget(positionSlider_, 1);
        layout->addWidget(controls, 0);

        audioOutput_ = new QAudioOutput(this);
        player_ = new QMediaPlayer(this);
        player_->setAudioOutput(audioOutput_);
        player_->setVideoOutput(videoWidget_);
        player_->setSource(QUrl::fromLocalFile(path));

        connect(playButton_, &QPushButton::clicked, this, [this] {
            if (player_->playbackState() == QMediaPlayer::PlayingState) {
                player_->pause();
            } else {
                player_->play();
            }
        });
        connect(player_, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
            playButton_->setText(state == QMediaPlayer::PlayingState ? QStringLiteral("停止") : QStringLiteral("再生"));
        });
        connect(player_, &QMediaPlayer::durationChanged, this, [this](qint64 duration) {
            positionSlider_->setRange(0, static_cast<int>(std::min<qint64>(duration, std::numeric_limits<int>::max())));
        });
        connect(player_, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
            if (!sliderPressed_) {
                positionSlider_->setValue(static_cast<int>(std::min<qint64>(position, std::numeric_limits<int>::max())));
            }
        });
        connect(positionSlider_, &QSlider::sliderPressed, this, [this] { sliderPressed_ = true; });
        connect(positionSlider_, &QSlider::sliderReleased, this, [this] {
            sliderPressed_ = false;
            player_->setPosition(positionSlider_->value());
        });
    }

    ~InlineVideoPreview() override
    {
        if (player_) {
            player_->stop();
            player_->setSource(QUrl());
        }
    }

private:
    QVideoWidget* videoWidget_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QSlider* positionSlider_ = nullptr;
    QAudioOutput* audioOutput_ = nullptr;
    QMediaPlayer* player_ = nullptr;
    bool sliderPressed_ = false;
};

class TextEditor final : public QPlainTextEdit {
public:
    explicit TextEditor(QWidget* parent = nullptr) : QPlainTextEdit(parent)
    {
        TextFontSettings::registerEditor(this);
    }

    ~TextEditor() override
    {
        TextFontSettings::unregisterEditor(this);
    }

protected:
    bool event(QEvent* event) override
    {
        if (handleTextZoomGesture(event, accumulatedZoom_, TextFontSettings::Surface::Editor)) {
            return true;
        }
        return QPlainTextEdit::event(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (handleTextZoomKey(event, TextFontSettings::Surface::Editor)) {
            return;
        }
        QPlainTextEdit::keyPressEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (event->modifiers() & Qt::ControlModifier) {
            const QPoint delta = event->angleDelta();
            const QPoint pixelDelta = event->pixelDelta();
            const int amount = !delta.isNull() ? delta.y() : pixelDelta.y();
            if (amount != 0) {
                TextFontSettings::changeConfiguredPointSize(TextFontSettings::Surface::Editor, amount > 0 ? 1 : -1);
            }
            event->accept();
            return;
        }
        QPlainTextEdit::wheelEvent(event);
    }

private:
    qreal accumulatedZoom_ = 0.0;
};

class TextEditorDialog final : public QDialog {
public:
    explicit TextEditorDialog(const QString& filePath, QWidget* parent = nullptr)
        : QDialog(parent), filePath_(filePath)
    {
        setWindowTitle(QStringLiteral("Mycel Editor - %1").arg(QFileInfo(filePath_).fileName()));
        resize(860, 620);

        auto* layout = new QVBoxLayout(this);
        pathLabel_ = new QLabel(QDir::toNativeSeparators(filePath_), this);
        pathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(pathLabel_);

        editor_ = new TextEditor(this);
        layout->addWidget(editor_, 1);

        auto* buttonRow = new QHBoxLayout();
        statusLabel_ = new QLabel(this);
        buttonRow->addWidget(statusLabel_, 1);

        saveButton_ = new QPushButton(QStringLiteral("保存"), this);
        closeButton_ = new QPushButton(QStringLiteral("閉じる"), this);
        buttonRow->addWidget(saveButton_);
        buttonRow->addWidget(closeButton_);
        layout->addLayout(buttonRow);

        if (!loadFile()) {
            editor_->setReadOnly(true);
            saveButton_->setEnabled(false);
        }

        connect(editor_, &QPlainTextEdit::textChanged, this, [this] {
            if (loading_) {
                return;
            }
            modified_ = true;
            updateStatus();
        });
        connect(saveButton_, &QPushButton::clicked, this, [this] { saveFile(); });
        connect(closeButton_, &QPushButton::clicked, this, &QWidget::close);

        auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
        connect(saveShortcut, &QShortcut::activated, this, [this] { saveFile(); });

        updateStatus();
    }

    bool wasSaved() const
    {
        return saved_;
    }

protected:
    void reject() override
    {
        close();
    }

    void closeEvent(QCloseEvent* event) override
    {
        if (!modified_) {
            event->accept();
            return;
        }

        const QMessageBox::StandardButton result = QMessageBox::question(
            this,
            QStringLiteral("Mycel"),
            QStringLiteral("変更を保存しますか？"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if (result == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if (result == QMessageBox::Save && !saveFile()) {
            event->ignore();
            return;
        }
        event->accept();
    }

private:
    bool loadFile()
    {
        QFile file(filePath_);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("ファイルを開けませんでした。"));
            return false;
        }

        loading_ = true;
        editor_->setPlainText(QString::fromUtf8(file.readAll()));
        loading_ = false;
        modified_ = false;
        loadedLastModified_ = QFileInfo(filePath_).lastModified();
        return true;
    }

    bool saveFile()
    {
        const QFileInfo info(filePath_);
        if (loadedLastModified_.isValid() && info.exists() && info.lastModified() > loadedLastModified_) {
            const QMessageBox::StandardButton result = QMessageBox::question(
                this,
                QStringLiteral("Mycel"),
                QStringLiteral("ファイルが外部で変更されています。上書きしますか？"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (result != QMessageBox::Yes) {
                return false;
            }
        }

        QFile file(filePath_);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("ファイルを保存できませんでした。"));
            return false;
        }

        file.write(editor_->toPlainText().toUtf8());
        file.close();
        loadedLastModified_ = QFileInfo(filePath_).lastModified();
        modified_ = false;
        saved_ = true;
        updateStatus();
        return true;
    }

    void updateStatus()
    {
        statusLabel_->setText(modified_ ? QStringLiteral("未保存") : QStringLiteral("保存済み"));
        saveButton_->setEnabled(!editor_->isReadOnly() && modified_);
        setWindowModified(modified_);
    }

    QString filePath_;
    QLabel* pathLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    TextEditor* editor_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    QPushButton* closeButton_ = nullptr;
    QDateTime loadedLastModified_;
    bool loading_ = false;
    bool modified_ = false;
    bool saved_ = false;
};

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
        fileSystemRefreshTimer_.setSingleShot(true);
        fileSystemRefreshTimer_.setInterval(500);
        connect(fileSystemWatcher_, &QFileSystemWatcher::directoryChanged,
                this, [this](const QString& path) { scheduleFileSystemRefresh(path); });
        connect(fileSystemWatcher_, &QFileSystemWatcher::fileChanged,
                this, [this](const QString& path) { scheduleFileSystemRefresh(path); });
        connect(&fileSystemRefreshTimer_, &QTimer::timeout, this, [this] { refreshFromFileSystemChange(); });

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

        loadOrderFile();
        loadColorFile();
        loadPreviewFile();
        loadLinkFile();
        if (!loadCollapsedFile()) {
            applyLargeTreeStartupCollapse();
        }
        applyTheme(uiTheme_, false, false);
        restoreWindowStateFromSettingsFile();
        if (mycelStorageEnabled_) {
            cleanHistoryTrash();  // discard any trash left behind by a previous session
        }
        rebuild(true);
        QTimer::singleShot(0, this, [this] { syncEditorPaneVisibility(); });
        qApp->installEventFilter(this);
    }

    ~MainWindow() override
    {
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
        saveViewState();
        saveCollapsedFile();
        rootPath_ = normalized;
        rememberRootPath(rootPath_);
        collapsedPaths_.clear();
        previewPaths_.clear();
        previewSizes_.clear();
        previewImageScales_.clear();
        loadOrderFile();
        loadColorFile();
        loadPreviewFile();
        loadLinkFile();
        if (!loadCollapsedFile()) {
            applyLargeTreeStartupCollapse();
        }
        // Keep the current window position/size when switching roots — do NOT apply the target
        // root's saved window geometry (that is only restored on initial startup).
        rebuild(true);
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

    bool openCenteredTextEditor(const QString& path)
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

        pauseFileSystemWatcher();
        const FileOperationService::MoveResult result = FileOperationService::moveInto(
            {{sourcePath, sourceIsDir, sourceIsRoot}}, targetDirPath);
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
            // source already lives in the target directory: nothing to do
            clearDragPreview();
            rebuild(false);
            return;
        }

        const FileOperationService::MovedEntry& entry = result.moved.front();
        recordDebugEvent(QStringLiteral("move node metadata: %1 -> %2")
                             .arg(relativeKeyForPath(entry.oldPath), relativeKeyForPath(entry.newPath)));
        applyMovedMetadata(entry, targetDirPath, mycelStorageEnabled_ && !entry.isDir);
        saveColorFile();
        savePreviewFile();
        saveLinkFile();
        saveCollapsedFile();
        if (mycelStorageEnabled_ && !entry.isDir) {
            saveOrderFile();
        }
        rebuild(false);
        recordHistory(QStringLiteral("移動"), {{entry.oldPath, entry.newPath}},
                      {{entry.newPath, entry.oldPath}}, historyBefore, historySelection);
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
        for (NodeItem* item : dragItems) {
            Node* node = item ? item->node() : nullptr;
            if (!node || node == root_.get()) {
                continue;
            }
            requests.push_back({node->path, node->isDir, false});
            recordDebugEvent(QStringLiteral("move drag request: %1 isDir=%2")
                                 .arg(relativeKeyForPath(node->path),
                                      node->isDir ? QStringLiteral("true") : QStringLiteral("false")));
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

        recordDebugEvent(QStringLiteral("move drag saving metadata"));
        saveColorFile();
        savePreviewFile();
        saveOrderFile();
        saveLinkFile();
        saveCollapsedFile();
        recordDebugEvent(QStringLiteral("move drag rebuild"));
        rebuild(false);

        std::vector<std::pair<QString, QString>> redoMoves;
        std::vector<std::pair<QString, QString>> undoMoves;
        for (const FileOperationService::MovedEntry& entry : result.moved) {
            redoMoves.push_back({entry.oldPath, entry.newPath});
            undoMoves.push_back({entry.newPath, entry.oldPath});
        }
        if (!redoMoves.empty()) {
            recordHistory(QStringLiteral("移動"), std::move(redoMoves), std::move(undoMoves),
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

        return canImportExternalUrls(mimeData, targetDir) || mimeData->hasImage() ||
               mimeData->hasHtml() || mimeData->hasText() || pasteableBinaryFormat(mimeData).has_value();
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
        if (mimeData->hasText()) {
            if (const auto embedUrl = youtubeEmbedUrlFromText(mimeData->text())) {
                const QString videoId = youtubeVideoIdFromUrl(QUrl(*embedUrl));
                const QString preferredName = videoId.isEmpty()
                                                  ? QStringLiteral("YouTube.url")
                                                  : QStringLiteral("YouTube %1.url").arg(videoId);
                destinationPath = availableImportPath(targetDir->path, preferredName, false);
                saved = writeBytes(destinationPath, (*embedUrl + QLatin1Char('\n')).toUtf8());
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

        for (const FileLink& link : fileLinks_) {
            if (link.from == from->path && link.to == to->path) {
                relayout();  // already linked: just restore the layout after the drag
                return;
            }
        }

        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();

        // A link target is placed beside exactly one source, so re-linking the same file
        // replaces its previous connection instead of leaving a stale line behind.
        fileLinks_.erase(std::remove_if(fileLinks_.begin(), fileLinks_.end(),
                                        [to](const FileLink& link) { return link.to == to->path; }),
                         fileLinks_.end());

        fileLinks_.push_back({from->path, to->path});
        saveLinkFile();
        relayout();
        recordHistory(QStringLiteral("関連付け"), {}, {}, historyBefore, historySelection);
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
        rebuild(false);
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
        if (!embedUrl) {
            return true;
        }

        const QString cachePath = youtubeThumbnailCachePathForEmbedUrl(*embedUrl);
        if (!cachePath.isEmpty() && QFileInfo::exists(cachePath)) {
            return true;
        }

        fetchYouTubeThumbnailForInlinePreview(path, *embedUrl);
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
    struct MetadataSnapshot {
        QSet<QString> collapsedPaths;
        std::map<QString, QColor> userColors;
        QSet<QString> previewPaths;
        std::map<QString, QSizeF> previewSizes;
        std::map<QString, qreal> previewImageScales;
        std::map<QString, QStringList> fileOrders;
        std::vector<FileLink> fileLinks;
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
                                previewImageScales_, fileOrders_, fileLinks_};
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
    }

    void saveAllMetadata()
    {
        saveColorFile();
        saveOrderFile();
        savePreviewFile();
        saveLinkFile();
        saveCollapsedFile();
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

    void refreshAll()
    {
        cancelQueuedInlinePreviewToggle();
        loadOrderFile();
        loadColorFile();
        loadPreviewFile();
        loadLinkFile();
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

    QStringList collectWatchedDirectories() const
    {
        QStringList directories;
        const QFileInfo rootInfo(rootPath_);
        if (!rootInfo.isDir()) {
            return directories;
        }

        directories.append(rootInfo.absoluteFilePath());
        QDirIterator iterator(rootPath_,
                              QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString path = QFileInfo(iterator.next()).absoluteFilePath();
            if (!isMetadataPath(path)) {
                directories.append(path);
            }
        }
        directories.removeDuplicates();
        return directories;
    }

    QStringList collectWatchedFiles() const
    {
        QStringList files;
        if (!QFileInfo(rootPath_).isDir()) {
            return files;
        }

        QDirIterator iterator(rootPath_,
                              QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString path = QFileInfo(iterator.next()).absoluteFilePath();
            if (!isMetadataPath(path)) {
                files.append(path);
            }
        }
        files.removeDuplicates();
        return files;
    }

    void resetFileSystemWatcher()
    {
        if (!fileSystemWatcher_ || inlineRenameActivitySuspended_) {
            return;
        }

        const QStringList currentFiles = fileSystemWatcher_->files();
        if (!currentFiles.isEmpty()) {
            fileSystemWatcher_->removePaths(currentFiles);
        }
        const QStringList currentDirectories = fileSystemWatcher_->directories();
        if (!currentDirectories.isEmpty()) {
            fileSystemWatcher_->removePaths(currentDirectories);
        }

        const QStringList directories = collectWatchedDirectories();
        if (!directories.isEmpty()) {
            fileSystemWatcher_->addPaths(directories);
        }
        const QStringList files = collectWatchedFiles();
        if (!files.isEmpty()) {
            fileSystemWatcher_->addPaths(files);
        }
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

    enum class SubtreeRefresh { Changed, Unchanged, NotFound };

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

        const QStringList refreshDirectories = pendingRefreshDirectories();
        pendingFileSystemPaths_.clear();
        cancelQueuedInlinePreviewToggle();
        if (refreshDirectories.isEmpty()) {
            resetFileSystemWatcher();
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
            // mtime touch). Skip the re-render so selection and previews do not churn.
            resetFileSystemWatcher();
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
    void appendCreatedItemToOrder(const QString& directoryPath, const QString& newName, bool,
                                  const QString& afterName = QString())
    {
        if (!mycelStorageEnabled_) {
            return;
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
        const QJsonObject rootObject = currentViewStateObject();

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

    void rebuild(bool fitAfterRebuild)
    {
        saveSideEditorNow();
        finishInlineRename(false);
        root_ = scanTree(rootPath_, 0, -1, collapsedPaths_, previewPaths_, previewSizes_, fileOrders_, rootPath_,
                         mycelStorageEnabled_);
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
        if (generation != viewRestoreGeneration_ || pendingViewRestoreReapplies_ <= 0 || !view_) {
            return;  // a newer restore or a root switch superseded this re-apply
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
    QString queuedPreviewPath_;
    QString queuedCollapsePath_;
    QTimer previewClickTimer_;
    QFileSystemWatcher* fileSystemWatcher_ = nullptr;
    QTimer fileSystemRefreshTimer_;
    QSet<QString> pendingFileSystemPaths_;
    std::map<QString, QSizeF> previewSizes_;
    std::map<QString, qreal> previewImageScales_;
    std::map<QString, QStringList> fileOrders_;
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
                logFile.open(QIODevice::WriteOnly | QIODevice::Append);
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
