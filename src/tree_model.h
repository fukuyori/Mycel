#pragma once

// Data model (Node/FileLink), theming, tree scanning/layout, preview-file-type detection,
// and startup-option parsing -- everything main.cpp needed before it gets into the
// QGraphicsItem/QWidget classes and MainWindow itself. Extracted verbatim out of main.cpp;
// included from inside its anonymous namespace (see main.cpp), so these declarations keep
// the same internal linkage they always had.
#include <QtCore/QDir>
#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QDirIterator>
#include <QtCore/QStorageInfo>
#include <QtCore/QHash>
#include <QtCore/QIODevice>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QCryptographicHash>
#include <QtCore/QXmlStreamReader>
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QRegularExpression>
#include <QtCore/QBuffer>
#include <QtCore/QSaveFile>
#include <QtCore/QSettings>
#include <QtCore/QThread>
#include <QtCore/QSet>
#include <QtCore/QSizeF>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QFontMetricsF>
#include <QtGui/QIcon>
#include <QtGui/QImage>
#include <QtGui/QImageReader>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPen>
#include <QtGui/QPixmap>
#include <QtGui/QPixmapCache>
#include <QtGui/QTextDocument>
#include <QtGui/QTextOption>
#if MYCEL_HAS_PDF
#include <QtPdf/QPdfDocument>
#endif
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

#include <algorithm>
#include <atomic>
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

constexpr int MaxDepth = 5;
constexpr int MaxChildren = 90;
constexpr int LargeTreeAutoCollapseFileThreshold = 500;
constexpr qreal XStep = 260.0;
constexpr qreal ParentChildGap = 80.0;
constexpr qreal YStep = 72.0;
constexpr qreal FreeCanvasMargin = 12000.0;
// Files larger than this are skipped for rename-detection hashing: cloud-synced roots can hold
// large placeholder files whose content is not resident locally, and hashing them would force
// a full download just to compute an identity we rarely need for big files.
constexpr qint64 RenameHashMaxFileSize = 10 * 1024 * 1024;
// How often the background sweep re-checks every directory for external renames the live
// QFileSystemWatcher notifications missed (some cloud-sync clients/network drives do not fire
// reliable change notifications). This is a safety net, not the primary detection path, so a
// low frequency is fine -- the per-file hash cache keeps each sweep cheap.
constexpr int BackgroundReconcileIntervalMs = 120000;
// Delay before the deferred startup scan begins. The initial scan used to run inside the
// MainWindow constructor, so on network drives or large roots the window did not appear until
// every directory had been walked; this small delay lets the shown window paint its loading
// state before the (still GUI-thread) visible-tree scan starts.
constexpr int DeferredStartupDelayMs = 50;

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

// A link target is laid out beside exactly one source (layoutFileLinks positions each target
// node's center once per source that claims it, so a target claimed by two sources has its
// position overwritten by whichever is processed last, leaving the other source's connector
// pointing at the wrong place). addFileLink() enforces "one incoming link per target" at
// creation time, but that invariant is not automatically preserved when links are loaded from
// disk (loadLinkFile) or rewritten after an external rename (rekeyPathMetadataAfterRename) --
// callers on those paths should run the result through this. Keeps, for each target, only the
// last occurrence in `links`' order (matching the "re-linking replaces the old link" semantics
// addFileLink already uses), preserving the relative order of the survivors.
void dedupeFileLinksKeepingLastPerTarget(std::vector<FileLink>& links)
{
    QSet<QString> keptTargets;
    std::vector<FileLink> kept;
    kept.reserve(links.size());
    for (auto it = links.rbegin(); it != links.rend(); ++it) {
        if (keptTargets.contains(it->to)) {
            continue;
        }
        keptTargets.insert(it->to);
        kept.push_back(*it);
    }
    std::reverse(kept.begin(), kept.end());
    links = std::move(kept);
}

// Cached content hash for one file, keyed by its root-relative path. Recomputed only when
// size or mtime no longer match, so unchanged (typically large, rarely-edited) files are not
// re-read on every scan.
struct FileHashCacheEntry {
    qint64 size = 0;
    qint64 mtimeMs = 0;
    QByteArray hash;
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
std::optional<QUrl> urlForShortcutFile(const QFileInfo& info);

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

    if (urlForShortcutFile(info)) {
        return QSizeF(520.0, 300.0);
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

// True when `path` lives on a network filesystem. Native file watching there is somewhere
// between expensive and broken: macOS FSEvents does not deliver events for SMB/NFS mounts,
// and Qt on Windows falls back to polling every watched path every second. For such roots the
// watcher is disabled entirely and the periodic background sweep takes over change detection.
bool isNetworkFileSystemPath(const QString& path)
{
#ifdef Q_OS_WIN
    const QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    if (native.startsWith(QStringLiteral("\\\\"))) {
        return true;  // UNC path
    }
    const QString driveRoot = native.left(3);  // e.g. "Z:\\"
    return GetDriveTypeW(reinterpret_cast<const wchar_t*>(driveRoot.utf16())) == DRIVE_REMOTE;
#else
    const QByteArray type = QStorageInfo(path).fileSystemType().toLower();
    static constexpr const char* networkTypes[] = {"smb",  "cifs",  "nfs", "afpfs",
                                                   "webdav", "davfs", "sshfs", "9p"};
    for (const char* networkType : networkTypes) {
        if (type.contains(networkType)) {
            return true;
        }
    }
    return false;
#endif
}

// True when a file's content is not resident on disk (cloud-sync placeholder, e.g. OneDrive
// Files On-Demand or Dropbox Smart Sync). Reading such a file forces a full download, so
// rename-detection hashing must skip it. Non-Windows platforms report false: their sync clients
// typically keep full local copies.
bool isCloudPlaceholderFile(const QFileInfo& info)
{
#ifdef Q_OS_WIN
    const DWORD attributes = GetFileAttributesW(reinterpret_cast<const wchar_t*>(info.absoluteFilePath().utf16()));
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    constexpr DWORD kRecallOnDataAccess = 0x00400000; // FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
    return (attributes & (kRecallOnDataAccess | FILE_ATTRIBUTE_OFFLINE)) != 0;
#else
    Q_UNUSED(info);
    return false;
#endif
}

// Content hash used to recognize a file that was renamed outside Mycel (e.g. by a cloud-sync
// client or another program). Returns nullopt for files too large to hash cheaply, cloud
// placeholders, or on read failure.
std::optional<QByteArray> computeFileContentHash(const QFileInfo& info)
{
    if (!info.isFile() || info.size() > RenameHashMaxFileSize || isCloudPlaceholderFile(info)) {
        return std::nullopt;
    }
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(&file)) {
        return std::nullopt;
    }
    return hash.result();
}

// Explains why computeFileContentHash() returned nullopt for `info`, for diagnostic logging
// only (reconcileRenamedFiles calls this so a failed rename match leaves a traceable reason
// in the debug pane instead of just silently not matching).
QString describeHashSkipReason(const QFileInfo& info)
{
    if (info.size() > RenameHashMaxFileSize) {
        return QStringLiteral("too large: %1 bytes").arg(info.size());
    }
    if (isCloudPlaceholderFile(info)) {
        return QStringLiteral("cloud placeholder, not downloaded locally");
    }
    return QStringLiteral("could not open/read the file");
}

// One recursive listing of every real directory and file under the root. This is the single
// walk shared by every whole-tree consumer -- QFileSystemWatcher registration, the rename
// reconcile, and hash seeding -- which used to each walk the tree themselves (up to six
// traversals per refresh; each stat is a network round-trip on remote drives). Only the
// root's own .mycel subtree is excluded; hidden and system entries are included. The root
// directory itself leads the directory list.
struct TreeWalkResult {
    QStringList directories;
    QStringList files;
    QList<QFileInfo> fileInfos;  // one entry per element of `files`, same order (cached stats)
};

// `cancelled` (optional) aborts the walk early with a partial result; callers passing it must
// discard that partial result themselves.
TreeWalkResult walkRealTree(const QString& rootPath, const std::atomic_bool* cancelled = nullptr)
{
    TreeWalkResult result;
    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.isDir()) {
        return result;
    }

    const QString metadataRoot = QDir::cleanPath(QDir(rootPath).filePath(QStringLiteral(".mycel")));
    const auto isMetadataPath = [&metadataRoot](const QString& path) {
        return path == metadataRoot || path.startsWith(metadataRoot + QLatin1Char('/'));
    };

    result.directories.append(rootInfo.absoluteFilePath());
    QDirIterator iterator(rootPath,
                          QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        if (cancelled && cancelled->load()) {
            return result;
        }
        iterator.next();
        const QFileInfo info = iterator.fileInfo();
        const QString path = QDir::cleanPath(info.absoluteFilePath());
        if (isMetadataPath(path)) {
            continue;
        }
        if (info.isDir()) {
            result.directories.append(path);
        } else {
            result.files.append(path);
            result.fileInfos.append(info);
        }
    }
    return result;
}

// Everything the startup scan thread produces from its single whole-tree walk: the directory
// and file lists the QFileSystemWatcher registration and the rename reconcile need, plus the
// hash-cache entries that were missing or stale (seeded off-thread because it reads file
// contents, which on network drives means downloading them).
struct StartupScanResult {
    QStringList directories;
    QStringList files;
    std::map<QString, FileHashCacheEntry> refreshedHashes;
};

// Runs on the startup scan thread. Pure function of the filesystem: touches no MainWindow
// state, works on a copy of the hash cache, and bails out (returning a partial result that the
// caller discards) as soon as `cancelled` is set so window close never waits on a slow walk.
StartupScanResult runStartupScan(const QString& rootPath, bool mycelStorageEnabled,
                                 const std::map<QString, FileHashCacheEntry>& knownHashes,
                                 const std::atomic_bool& cancelled)
{
    StartupScanResult result;
    TreeWalkResult walk = walkRealTree(rootPath, &cancelled);
    result.directories = std::move(walk.directories);
    result.files = std::move(walk.files);

    if (!mycelStorageEnabled || cancelled.load()) {
        return result;
    }

    // Seed/refresh content hashes for rename detection (same skip rules as
    // updateHashCacheEntryForFile: size cap, cloud placeholders, unchanged size+mtime).
    const QDir root(rootPath);
    for (const QFileInfo& info : walk.fileInfos) {
        if (cancelled.load()) {
            return result;
        }
        if (info.size() > RenameHashMaxFileSize || isCloudPlaceholderFile(info)) {
            continue;
        }
        const QString key = root.relativeFilePath(info.absoluteFilePath());
        const qint64 mtimeMs = info.lastModified().toMSecsSinceEpoch();
        const auto existing = knownHashes.find(key);
        if (existing != knownHashes.end() && existing->second.size == info.size() &&
            existing->second.mtimeMs == mtimeMs) {
            continue;
        }
        if (const std::optional<QByteArray> hash = computeFileContentHash(info)) {
            result.refreshedHashes[key] = FileHashCacheEntry{info.size(), mtimeMs, *hash};
        }
    }
    return result;
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

// Shared ordering rule for directory entries: names present in `order` sort by their position
// there; anything else falls back to directories-first, case-insensitive alphabetical. Used by
// both scanTree (freshly stat'd QFileInfoList) and reorderChildrenInPlace (cached Node children)
// so a drag-drop reorder and a fresh disk scan always agree on the resulting order.
bool lessByFileOrder(const QString& aName, bool aIsDir, const QString& bName, bool bIsDir,
                      const QStringList& order)
{
    const int ai = order.indexOf(aName);
    const int bi = order.indexOf(bName);
    if (ai != -1 || bi != -1) {
        if (ai == -1) {
            return false;
        }
        if (bi == -1) {
            return true;
        }
        return ai < bi;
    }
    if (aIsDir != bIsDir) {
        return aIsDir;
    }
    return QString::compare(aName, bName, Qt::CaseInsensitive) < 0;
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
            return lessByFileOrder(a.fileName(), a.isDir(), b.fileName(), b.isDir(), names);
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

// Reorders `parent`'s already-scanned children to match `order`, applying the same rule scanTree
// uses on a fresh disk listing. A drag-drop reorder changes only fileOrders_ metadata -- no file
// is added, removed, or renamed -- so the visible children can be resorted in place instead of
// re-stat'ing the whole tree from disk (see relayout()'s handling of other metadata-only changes).
void reorderChildrenInPlace(Node& parent, const QStringList& order)
{
    std::stable_sort(parent.children.begin(), parent.children.end(),
                      [&order](const std::unique_ptr<Node>& a, const std::unique_ptr<Node>& b) {
                          return lessByFileOrder(a->name, a->isDir, b->name, b->isDir, order);
                      });
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
    // needs the whole subtree height (mirrors layoutTree's yCursor advance); a file needs one row --
    // plus, via nodeRowSpan, whatever its OWN outgoing links need (a target can itself be a link
    // source, chaining further targets off it; without this a chained file target's slot in its
    // parent stack was sized for just its own row, so its chain spilled into the next sibling).
    static qreal linkedTargetSpan(const Node& node, const QSet<QString>& linkedTargets,
                                  Node& root, const std::vector<FileLink>& links)
    {
        if (!node.isDir) {
            return nodeRowSpan(node, root, links);
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
        // Targets already placed by an earlier source in this call, so later sources' columns
        // steer clear of them too.
        QSet<QString> positionedTargets;
        // A source that is itself another link's target (a chain, e.g. A -> B -> C) must not be
        // positioned until B's own center has been finalized by A's pass -- linkSourceAnchor(*from)
        // below would otherwise read B's default-constructed (0,0) center. positionSource recurses
        // into a target immediately after placing it, so a chain is always walked root-first
        // regardless of map order (plain path order would otherwise sort e.g. "NewFile 4.txt"
        // before "NewFile.txt" and process the chain backwards).
        QSet<QString> processedSources;

        std::function<void(const QString&)> positionSource = [&](const QString& sourcePath) {
            if (processedSources.contains(sourcePath)) {
                return;  // already positioned, or a link cycle looping back here
            }
            processedSources.insert(sourcePath);
            const auto found = targetsBySource.find(sourcePath);
            if (found == targetsBySource.end()) {
                return;
            }
            Node* from = findNodeByPath(root, sourcePath);
            if (!from) {
                return;
            }
            const std::vector<Node*>& targets = found->second;

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
            // preview of the node above or below the source. Skip this source's own targets
            // (not yet positioned) and any other link target not yet placed by this call, but
            // do check against targets already placed by an earlier source — otherwise two
            // sources' fans can claim overlapping columns and one paints over the other.
            qreal columnLeft = anchor.x();
            std::function<void(const Node&)> clearOverlaps = [&](const Node& node) {
                if (linkedTargets.contains(node.path) && !positionedTargets.contains(node.path)) {
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
                positionedTargets.insert(to->path);
                positionSource(to->path);  // `to` may itself be a chained source; its center is
                                            // final now, so lay out its own fan (a no-op if it
                                            // has no outgoing links).
            }
        };

        // Start from chain roots (sources that are not themselves another link's target); a
        // source that IS a target only gets positioned via positionSource's recursion, once its
        // own center has been finalized by its positioning source.
        for (auto& [sourcePath, targets] : targetsBySource) {
            Q_UNUSED(targets);
            if (!linkedTargets.contains(sourcePath)) {
                positionSource(sourcePath);
            }
        }
        // Safety net: anything left over is a source whose own incoming link didn't resolve to a
        // valid target above (e.g. a dangling/self link) -- position it directly so its fan still
        // lays out instead of silently vanishing, just anchored at whatever center it already has.
        for (auto& [sourcePath, targets] : targetsBySource) {
            Q_UNUSED(targets);
            positionSource(sourcePath);
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

QString urlShortcutFileName(const QUrl& url)
{
    QString stem = url.host().trimmed();
    const QStringList pathParts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (!pathParts.isEmpty()) {
        const QString lastPart = QUrl::fromPercentEncoding(pathParts.last().toUtf8()).trimmed();
        if (!lastPart.isEmpty()) {
            stem += stem.isEmpty() ? lastPart : QStringLiteral(" ") + lastPart;
        }
    }
    if (stem.isEmpty()) {
        stem = QStringLiteral("URL");
    }

    stem.replace(QRegularExpression(QStringLiteral(R"([<>:"/\\|?*\x00-\x1f])")), QStringLiteral(" "));
    stem = stem.simplified();
    while (stem.endsWith(QLatin1Char('.')) || stem.endsWith(QLatin1Char(' '))) {
        stem.chop(1);
    }
    if (stem.isEmpty()) {
        stem = QStringLiteral("URL");
    }
    if (stem.size() > 80) {
        stem = stem.left(80).trimmed();
    }
    return stem + QStringLiteral(".url");
}

QByteArray urlShortcutBytes(const QUrl& url)
{
    const QString text = QStringLiteral("[InternetShortcut]\nURL=%1\n").arg(url.toString());
    return text.toUtf8();
}

std::optional<QUrl> urlForShortcutFile(const QFileInfo& info)
{
    if (!info.exists() || !info.isFile() ||
        info.suffix().compare(QStringLiteral("url"), Qt::CaseInsensitive) != 0 ||
        info.size() > 64 * 1024) {
        return std::nullopt;
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    return firstUrlFromText(QString::fromUtf8(file.read(64 * 1024)));
}

QString htmlAttributeDecoded(QString value)
{
    value = value.trimmed();
    if (!value.contains(QLatin1Char('&'))) {
        return value;
    }

    QTextDocument doc;
    doc.setHtml(value);
    return doc.toPlainText().trimmed();
}

QString htmlTagAttributeValue(const QString& tag, const QString& attributeName)
{
    static const QRegularExpression attributePattern(
        QStringLiteral("([A-Za-z_:][-A-Za-z0-9_:.]*)\\s*=\\s*(\"([^\"]*)\"|'([^']*)'|([^\\s\"'=<>`]+))"),
        QRegularExpression::CaseInsensitiveOption);

    const QString wanted = attributeName.toLower();
    QRegularExpressionMatchIterator it = attributePattern.globalMatch(tag);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (match.captured(1).toLower() != wanted) {
            continue;
        }
        QString value = match.captured(3);
        if (value.isEmpty()) {
            value = match.captured(4);
        }
        if (value.isEmpty()) {
            value = match.captured(5);
        }
        return htmlAttributeDecoded(value);
    }
    return {};
}

std::optional<QUrl> resolvedHttpImageUrl(const QString& value, const QUrl& baseUrl)
{
    QString candidate = value.trimmed();
    if (candidate.isEmpty() || candidate.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
        return std::nullopt;
    }

    const QUrl resolved = baseUrl.resolved(QUrl(candidate));
    const QString scheme = resolved.scheme().toLower();
    if (!resolved.isValid() || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
        return std::nullopt;
    }
    return resolved;
}

std::optional<QUrl> resolvedHttpImageUrlFromSrcset(const QString& srcset, const QUrl& baseUrl)
{
    const QStringList candidates = srcset.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString candidate : candidates) {
        candidate = candidate.trimmed();
        const int whitespace = candidate.indexOf(QRegularExpression(QStringLiteral("\\s")));
        if (whitespace > 0) {
            candidate = candidate.left(whitespace);
        }
        if (auto url = resolvedHttpImageUrl(candidate, baseUrl)) {
            return url;
        }
    }
    return std::nullopt;
}

std::optional<QUrl> resolvedHttpImageUrlFromJsonishAttribute(QString value, const QUrl& baseUrl)
{
    value = value.trimmed();
    if (value.isEmpty()) {
        return std::nullopt;
    }

    static const QRegularExpression urlPattern(QStringLiteral(R"((https?:\\?/\\?/[^"\\]+))"),
                                               QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = urlPattern.globalMatch(value);
    while (it.hasNext()) {
        QString candidate = it.next().captured(1);
        candidate.replace(QStringLiteral("\\/"), QStringLiteral("/"));
        candidate.replace(QStringLiteral("\\u0026"), QStringLiteral("&"));
        if (auto url = resolvedHttpImageUrl(candidate, baseUrl)) {
            return url;
        }
    }
    return std::nullopt;
}

std::optional<QUrl> imageUrlFromImgTag(const QString& tag, const QUrl& baseUrl)
{
    for (const QString& attribute : {QStringLiteral("data-old-hires"), QStringLiteral("src"),
                                     QStringLiteral("data-src"), QStringLiteral("data-original"),
                                     QStringLiteral("data-lazy-src")}) {
        if (auto url = resolvedHttpImageUrl(htmlTagAttributeValue(tag, attribute), baseUrl)) {
            return url;
        }
    }
    if (auto url = resolvedHttpImageUrlFromSrcset(htmlTagAttributeValue(tag, QStringLiteral("srcset")), baseUrl)) {
        return url;
    }
    if (auto url = resolvedHttpImageUrlFromJsonishAttribute(
            htmlTagAttributeValue(tag, QStringLiteral("data-a-dynamic-image")), baseUrl)) {
        return url;
    }
    return std::nullopt;
}

int htmlDimensionAttribute(const QString& tag, const QString& attributeName)
{
    const QString value = htmlTagAttributeValue(tag, attributeName);
    if (value.contains(QLatin1Char('%'))) {
        return 0;
    }
    static const QRegularExpression numberPattern(QStringLiteral("^\\s*(\\d{1,5})"));
    const QRegularExpressionMatch match = numberPattern.match(value);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

bool imageRankTextContainsAny(const QString& text, const QStringList& words)
{
    for (const QString& word : words) {
        if (text.contains(word)) {
            return true;
        }
    }
    return false;
}

int scoreImageCandidate(const QString& tag, const QUrl& imageUrl, const QUrl& baseUrl, int index)
{
    const QString id = htmlTagAttributeValue(tag, QStringLiteral("id"));
    const QString classes = htmlTagAttributeValue(tag, QStringLiteral("class"));
    const QString alt = htmlTagAttributeValue(tag, QStringLiteral("alt"));
    const QString src = imageUrl.toString();
    const QString rankText = QStringLiteral("%1 %2 %3 %4")
                                 .arg(id, classes, alt, src)
                                 .toLower();

    int score = 100 - std::min(index, 30);
    if (imageUrl.host().compare(baseUrl.host(), Qt::CaseInsensitive) == 0) {
        score += 6;
    }

    const QString path = imageUrl.path().toLower();
    if (path.endsWith(QStringLiteral(".jpg")) || path.endsWith(QStringLiteral(".jpeg")) ||
        path.endsWith(QStringLiteral(".png")) || path.endsWith(QStringLiteral(".webp"))) {
        score += 8;
    } else if (path.endsWith(QStringLiteral(".svg")) || path.endsWith(QStringLiteral(".gif"))) {
        score -= 70;
    }

    const int width = htmlDimensionAttribute(tag, QStringLiteral("width"));
    const int height = htmlDimensionAttribute(tag, QStringLiteral("height"));
    if (width > 0 && height > 0) {
        if (width <= 2 || height <= 2) {
            score -= 200;
        } else if (width < 80 || height < 80) {
            score -= 80;
        } else {
            const qint64 area = static_cast<qint64>(width) * static_cast<qint64>(height);
            if (area >= 480000) {
                score += 45;
            } else if (area >= 120000) {
                score += 30;
            } else if (area >= 40000) {
                score += 15;
            }
            const double ratio = static_cast<double>(width) / static_cast<double>(height);
            score += (ratio >= 0.45 && ratio <= 3.8) ? 12 : -30;
        }
    }

    if (imageRankTextContainsAny(rankText, {QStringLiteral("hero"), QStringLiteral("main"),
                                            QStringLiteral("visual"), QStringLiteral("cover"),
                                            QStringLiteral("eyecatch"), QStringLiteral("article"),
                                            QStringLiteral("photo"), QStringLiteral("picture"),
                                            QStringLiteral("product"), QStringLiteral("media"),
                                            QStringLiteral("content")})) {
        score += 30;
    }
    if (alt.size() >= 4) {
        score += 6;
    }
    if (imageRankTextContainsAny(rankText, {QStringLiteral("logo"), QStringLiteral("icon"),
                                            QStringLiteral("favicon"), QStringLiteral("sprite"),
                                            QStringLiteral("pixel"), QStringLiteral("tracking"),
                                            QStringLiteral("beacon"), QStringLiteral("spacer"),
                                            QStringLiteral("transparent"), QStringLiteral("blank")})) {
        score -= 120;
    }
    if (imageRankTextContainsAny(rankText, {QStringLiteral("avatar"), QStringLiteral("profile"),
                                            QStringLiteral("badge"), QStringLiteral("button"),
                                            QStringLiteral("emoji"), QStringLiteral("ad-"),
                                            QStringLiteral("ads"), QStringLiteral("banner")})) {
        score -= 55;
    }
    return score;
}

std::optional<QUrl> rankedImageUrlFromHtml(const QString& html, const QUrl& baseUrl,
                                           const QRegularExpression& imgPattern)
{
    std::optional<QUrl> bestUrl;
    int bestScore = std::numeric_limits<int>::min();
    int index = 0;

    QRegularExpressionMatchIterator imgIt = imgPattern.globalMatch(html);
    while (imgIt.hasNext() && index < 40) {
        const QString tag = imgIt.next().captured(0);
        const auto url = imageUrlFromImgTag(tag, baseUrl);
        if (!url) {
            ++index;
            continue;
        }

        const int score = scoreImageCandidate(tag, *url, baseUrl, index);
        if (!bestUrl || score > bestScore) {
            bestUrl = url;
            bestScore = score;
        }
        ++index;
    }

    return bestScore > -20 ? bestUrl : std::nullopt;
}

std::optional<QUrl> firstImageUrlFromHtml(const QString& html, const QUrl& baseUrl)
{
    static const QRegularExpression metaPattern(QStringLiteral("<meta\\b[^>]*>"),
                                                QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression imgPattern(QStringLiteral("<img\\b[^>]*>"),
                                               QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatchIterator priorityImgIt = imgPattern.globalMatch(html);
    const QString host = baseUrl.host().toLower();
    const bool xPost = host == QStringLiteral("x.com") || host.endsWith(QStringLiteral(".x.com")) ||
                       host == QStringLiteral("twitter.com") || host.endsWith(QStringLiteral(".twitter.com"));
    while (priorityImgIt.hasNext()) {
        const QString tag = priorityImgIt.next().captured(0);
        const QString id = htmlTagAttributeValue(tag, QStringLiteral("id")).toLower();
        const QString alt = htmlTagAttributeValue(tag, QStringLiteral("alt")).trimmed();
        if (id != QStringLiteral("landingimage") &&
            alt != QStringLiteral("見出し画像") &&
            !(xPost && alt == QStringLiteral("画像"))) {
            continue;
        }
        if (auto url = imageUrlFromImgTag(tag, baseUrl)) {
            return url;
        }
    }

    QRegularExpressionMatchIterator metaIt = metaPattern.globalMatch(html);
    while (metaIt.hasNext()) {
        const QString tag = metaIt.next().captured(0);
        const QString property = htmlTagAttributeValue(tag, QStringLiteral("property")).toLower();
        const QString name = htmlTagAttributeValue(tag, QStringLiteral("name")).toLower();
        if (property != QStringLiteral("og:image") &&
            property != QStringLiteral("og:image:url") &&
            name != QStringLiteral("twitter:image") &&
            name != QStringLiteral("twitter:image:src")) {
            continue;
        }
        if (auto url = resolvedHttpImageUrl(htmlTagAttributeValue(tag, QStringLiteral("content")), baseUrl)) {
            return url;
        }
    }

    return rankedImageUrlFromHtml(html, baseUrl, imgPattern);
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
