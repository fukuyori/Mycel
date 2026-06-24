#include <QtCore/QDir>
#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QEvent>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDirIterator>
#include <QtCore/QIODevice>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMimeData>
#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QSettings>
#include <QtCore/QSet>
#include <QtCore/QSizeF>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtCore/QVector>
#include <QtGui/QTransform>
#include <QtGui/QBrush>
#include <QtGui/QActionGroup>
#include <QtGui/QColor>
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
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPen>
#include <QtGui/QPixmap>
#include <QtGui/QShortcut>
#include <QtGui/QTextCursor>
#include <QtGui/QTextDocument>
#include <QtGui/QTextOption>
#include <QtGui/QWheelEvent>
#include <QtGui/QAction>
#include <QtGui/QCloseEvent>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimediaWidgets/QVideoWidget>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGraphicsItem>
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
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
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

namespace {

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
    int depth = 0;
    int branch = -1;
    int hiddenChildren = 0;
    bool previewOpen = false;
    QPointF center;
    QSizeF size;
    QSizeF previewSize;
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

QColor neutralStroke()
{
    return QColor("#7f8a90");
}

QColor neutralFill()
{
    return QColor("#f7fafb");
}

QColor softFillFromColor(QColor color)
{
    color.setAlpha(42);
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

QSizeF automaticPreviewSize(const QFileInfo& info)
{
    constexpr qreal width = 460.0;
    constexpr qreal minHeight = 46.0;
    constexpr qreal maxHeight = 320.0;

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

    if (isImagePreviewFile(info) || info.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0 ||
        !isTextPreviewFile(info) || info.size() > 1024 * 1024) {
        return QSizeF(width, minHeight);
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QSizeF(width, minHeight);
    }

    QTextStream stream(&file);
    QStringList lines;
    while (lines.size() < 7 && !stream.atEnd()) {
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

std::unique_ptr<Node> scanTree(const QString& path, int depth, int branch,
                               const QSet<QString>& collapsedPaths, const QSet<QString>& previewPaths,
                               const std::map<QString, QSizeF>& previewSizes,
                               const std::map<QString, QStringList>& fileOrders,
                               const QString& rootPath)
{
    QFileInfo info(path);
    auto node = std::make_unique<Node>();
    node->path = info.absoluteFilePath();
    node->parentPath = info.absoluteDir().absolutePath();
    node->name = info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName();
    node->isDir = info.isDir();
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

    if (!node->isDir || depth >= MaxDepth) {
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
                                          collapsedPaths, previewPaths, previewSizes, fileOrders, rootPath));
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

void layoutTree(Node& node, qreal leftX, qreal& yCursor)
{
    node.center.setX(leftX + node.size.width() / 2.0);
    if (node.children.empty()) {
        node.center.setY(yCursor);
        yCursor += YStep + (node.previewOpen ? node.previewSize.height() + 28.0 : 0.0);
        return;
    }

    const qreal childLeftX = leftX + std::max(XStep, node.size.width() + ParentChildGap);
    for (auto& child : node.children) {
        layoutTree(*child, childLeftX, yCursor);
    }
    node.center.setY((node.children.front()->center.y() + node.children.back()->center.y()) / 2.0);
}

void translateTree(Node& node, const QPointF& delta)
{
    node.center += delta;
    for (auto& child : node.children) {
        translateTree(*child, delta);
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

qreal nodeVerticalSpan(const Node& node)
{
    return YStep + (node.previewOpen ? node.previewSize.height() + 28.0 : 0.0);
}

qreal nodeContentRightX(const Node& node)
{
    qreal right = node.center.x() + node.size.width() / 2.0;
    if (node.previewOpen) {
        const qreal previewRight = node.center.x() - node.size.width() / 2.0 + 40.0 + node.previewSize.width();
        right = std::max(right, previewRight);
    }
    return right;
}

void layoutFileLinks(Node& root, const std::vector<FileLink>& links)
{
    const int passes = std::clamp(static_cast<int>(links.size()), 1, 8);
    for (int pass = 0; pass < passes; ++pass) {
        std::map<QString, qreal> nextLinkedY;
        for (const FileLink& link : links) {
            Node* from = findNodeByPath(root, link.from);
            Node* to = findNodeByPath(root, link.to);
            if (!from || !to || from->isDir || to->isDir || from == to) {
                continue;
            }

            auto [it, inserted] = nextLinkedY.emplace(from->path, from->center.y());
            const qreal linkedY = it->second;
            const qreal linkedLeftX = nodeContentRightX(*from) + ParentChildGap;
            to->center = QPointF(linkedLeftX + to->size.width() / 2.0, linkedY);
            it->second = linkedY + nodeVerticalSpan(*to);
        }
    }
}

void visitNodes(Node& node, const std::function<void(Node&)>& fn)
{
    fn(node);
    for (auto& child : node.children) {
        visitNodes(*child, fn);
    }
}

bool isDescendantPath(const QString& ancestor, const QString& candidate)
{
    QFileInfo ancestorInfo(ancestor);
    QFileInfo candidateInfo(candidate);
    QString base = ancestorInfo.canonicalFilePath();
    QString child = candidateInfo.canonicalFilePath();
    if (base.isEmpty()) {
        base = ancestorInfo.absoluteFilePath();
    }
    if (child.isEmpty()) {
        child = candidateInfo.absoluteFilePath();
    }
    base = QDir::cleanPath(base);
    child = QDir::cleanPath(child);
    return !base.isEmpty() && !child.isEmpty() && child != base && child.startsWith(base + QLatin1Char('/'));
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
        QStringLiteral("csv"), QStringLiteral("sh")
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
    QString html = QStringLiteral(
        "<html><head><style>"
        "body{font-family:Meiryo,Segoe UI,sans-serif;color:#243036;background:#fff;}"
        "table{border-collapse:collapse;width:100%;font-size:13px;}"
        "td,th{border:1px solid #d1dce3;padding:4px 6px;vertical-align:top;}"
        "tr:nth-child(even){background:#f7fafb;}"
        "</style></head><body><table>");
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

QString fileKindBadge(const QFileInfo& info)
{
    const QString suffix = info.suffix().toLower();
    const QString name = info.fileName();
    if (suffix == QStringLiteral("cpp") || suffix == QStringLiteral("cc") ||
        suffix == QStringLiteral("cxx") || suffix == QStringLiteral("c") ||
        suffix == QStringLiteral("h") || suffix == QStringLiteral("hpp")) {
        return QStringLiteral("<>");
    }
    if (suffix == QStringLiteral("md") || suffix == QStringLiteral("rst")) {
        return QStringLiteral("MD");
    }
    if (suffix == QStringLiteral("txt")) {
        return QStringLiteral("TXT");
    }
    if (suffix == QStringLiteral("pdf")) {
        return QStringLiteral("PDF");
    }
    if (suffix == QStringLiteral("json") || suffix == QStringLiteral("yaml") ||
        suffix == QStringLiteral("yml") || suffix == QStringLiteral("toml") ||
        suffix == QStringLiteral("xml") || suffix == QStringLiteral("cmake") ||
        name == QStringLiteral("CMakeLists.txt")) {
        return QStringLiteral("⚙");
    }
    if (suffix == QStringLiteral("sh") || suffix == QStringLiteral("bat") ||
        suffix == QStringLiteral("command")) {
        return QStringLiteral("$");
    }
    return {};
}

QString normalizedDirectoryPath(const QString& path)
{
    QFileInfo info(path);
    const QString absolutePath = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    const QString canonicalPath = QDir(absolutePath).canonicalPath();
    return canonicalPath.isEmpty() ? QDir(absolutePath).absolutePath() : canonicalPath;
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
        setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable |
                 QGraphicsItem::ItemSendsGeometryChanges);
        setAcceptDrops(node_->isDir);
        setZValue(10.0);
        createPreviewWidget();
    }

    Node* node() const { return node_; }
    QPointF layoutCenter() const { return layoutCenter_; }
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
    bool containsLinkDropScenePoint(const QPointF& scenePos) const
    {
        return linkDropSceneRect().contains(scenePos);
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
        if (node_->previewOpen) {
            rect = rect.united(QRectF(rect.left() + 40.0, rect.bottom() + 8.0,
                                      node_->previewSize.width(), node_->previewSize.height()));
        }
        return rect;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF box(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                         node_->size.width(), node_->size.height());
        if (hasUserFill()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(windowFill());
            painter->drawRoundedRect(box.adjusted(-4.0, -3.0, 4.0, 3.0), 8.0, 8.0);
        }
        if (isSelected()) {
            painter->setPen(QPen(QColor("#0b63ce"), 2.0));
            painter->setBrush(hasUserFill() ? QColor(219, 234, 254, 190) : QColor("#dbeafe"));
            painter->drawRoundedRect(box.adjusted(-4.0, -3.0, 4.0, 3.0), 8.0, 8.0);
        }

        if (node_->isDir && (externalDropHover_ || internalDropHover_)) {
            painter->setPen(QPen(QColor("#1e8cff"), 3.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(QColor(30, 140, 255, 30));
            painter->drawRoundedRect(box.adjusted(-5.0, -5.0, 5.0, 5.0), 15.0, 15.0);
        }
        if (!node_->isDir && linkDropHover_) {
            const QRectF zone(box.right() - 25.0, box.top() + 3.0, 30.0, box.height() - 6.0);
            painter->setPen(QPen(QColor("#0f9f6e"), 2.4, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(QColor(16, 185, 129, 38));
            painter->drawRoundedRect(zone, 8.0, 8.0);
            painter->setPen(QPen(QColor("#047857"), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawLine(QPointF(zone.center().x() - 5.0, zone.center().y()),
                              QPointF(zone.center().x() + 5.0, zone.center().y()));
        }

        if (node_->isDir) {
            paintFolder(painter, QPointF(box.left() + 18.0, -17.0), node_->depth == 0 ? QColor("#46a4ff") : QColor("#ffc94d"));
        } else {
            paintFile(painter, QPointF(box.left() + 6.0, -14.0), QFileInfo(node_->path));
        }

        QFont font = painter->font();
        font.setPointSize(node_->depth == 0 ? 17 : (node_->isDir ? 12 : 11));
        painter->setFont(font);
        painter->setPen(QColor("#172321"));
        painter->drawText(QRectF(box.left() + (node_->isDir ? 66.0 : 40.0), box.top(),
                                 box.width() - (node_->isDir ? 76.0 : 42.0), box.height()),
                          Qt::AlignVCenter | Qt::AlignLeft, shortLabel(node_->name));

        if (node_->collapsed && node_->hiddenChildren > 0) {
            const QRectF badge(box.right() + 8.0, -13.0, 88.0, 26.0);
            painter->setPen(QPen(QColor("#9b7a32"), 1.1));
            painter->setBrush(QColor("#fff6dd"));
            painter->drawRoundedRect(badge, 10.0, 10.0);
            QFont badgeFont = painter->font();
            badgeFont.setPointSize(10);
            badgeFont.setBold(true);
            painter->setFont(badgeFont);
            painter->setPen(QColor("#6f561f"));
            painter->drawText(badge, Qt::AlignCenter, QStringLiteral("配下 %1 件").arg(node_->hiddenChildren));
        } else if (node_->hiddenChildren > 0) {
            painter->setPen(QColor("#86602d"));
            painter->drawText(QRectF(box.right() - 50.0, box.top(), 44.0, box.height()),
                              Qt::AlignCenter, QStringLiteral("+%1").arg(node_->hiddenChildren));
        }

        if (node_->previewOpen) {
            paintInlinePreview(painter, QRectF(box.left() + 40.0, box.bottom() + 8.0,
                                               node_->previewSize.width(), node_->previewSize.height()));
        }
    }

protected:
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (node_->previewOpen && previewResizeHandle().contains(event->pos())) {
            resizingPreview_ = true;
            resizeStartScene_ = event->scenePos();
            resizeStartSize_ = node_->previewSize;
            setFlag(QGraphicsItem::ItemIsMovable, false);
            event->accept();
            return;
        }
        dragStart_ = pos();
        if (event->button() == Qt::LeftButton) {
            setOpacity(0.72);
            setZValue(100.0);
        }
        QGraphicsItem::mousePressEvent(event);
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
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
    void resizePreview(const QSizeF& size);
    void createPreviewWidget();
    void syncPreviewWidgetGeometry();

    QRectF previewRect() const
    {
        const QRectF box(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                         node_->size.width(), node_->size.height());
        return QRectF(box.left() + 40.0, box.bottom() + 8.0,
                      node_->previewSize.width(), node_->previewSize.height());
    }

    QRectF previewResizeHandle() const
    {
        const QRectF rect = previewRect();
        return QRectF(rect.right() - 16.0, rect.bottom() - 16.0, 16.0, 16.0);
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
        QPainterPath page;
        page.moveTo(at);
        page.lineTo(at.x() + 16.0, at.y());
        page.lineTo(at.x() + 26.0, at.y() + 10.0);
        page.lineTo(at.x() + 26.0, at.y() + 32.0);
        page.lineTo(at.x(), at.y() + 32.0);
        page.closeSubpath();

        painter->setPen(QPen(QColor("#879198"), 1.3));
        painter->setBrush(QColor("#ffffff"));
        painter->drawPath(page);
        painter->drawLine(QPointF(at.x() + 16.0, at.y()), QPointF(at.x() + 16.0, at.y() + 10.0));
        painter->drawLine(QPointF(at.x() + 16.0, at.y() + 10.0), QPointF(at.x() + 26.0, at.y() + 10.0));

        painter->setPen(QPen(QColor("#59666d"), 1.1));
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

        const QString badge = fileKindBadge(info);
        if (!badge.isEmpty()) {
            QFont badgeFont = painter->font();
            badgeFont.setPointSize(badge.size() > 2 ? 5 : 7);
            badgeFont.setBold(true);
            painter->setFont(badgeFont);
            painter->drawText(QRectF(at.x() + 3.0, at.y() + 14.0, 20.0, 13.0), Qt::AlignCenter, badge);
            return;
        }

        painter->drawLine(QPointF(at.x() + 5.0, at.y() + 17.0), QPointF(at.x() + 19.0, at.y() + 17.0));
        painter->drawLine(QPointF(at.x() + 5.0, at.y() + 22.0), QPointF(at.x() + 17.0, at.y() + 22.0));
    }

    void paintInlinePreview(QPainter* painter, const QRectF& rect)
    {
        painter->setPen(QPen(hasUserFill() ? QColor(154, 162, 168, 160) : QColor("#c4ccd1"), 1.1));
        painter->setBrush(hasUserFill() ? windowFill() : QColor("#ffffff"));
        painter->drawRoundedRect(rect, 8.0, 8.0);

        painter->setPen(QPen(QColor("#9aa5aa"), 1.2));
        const QPointF corner(rect.right() - 12.0, rect.bottom() - 4.0);
        painter->drawLine(corner, QPointF(rect.right() - 4.0, rect.bottom() - 12.0));
        painter->drawLine(QPointF(rect.right() - 8.0, rect.bottom() - 4.0),
                          QPointF(rect.right() - 4.0, rect.bottom() - 8.0));
    }

    Node* node_;
    MainWindow* window_;
    QPointF layoutCenter_;
    QPointF dragStart_;
    bool resizingPreview_ = false;
    bool externalDropHover_ = false;
    bool internalDropHover_ = false;
    bool linkDropHover_ = false;
    QPointF resizeStartScene_;
    QSizeF resizeStartSize_;
    QGraphicsProxyWidget* previewProxy_ = nullptr;
};

class MindMapScene final : public QGraphicsScene {
public:
    void setRoot(Node* root) { root_ = root; }
    void setUserColors(const std::map<QString, QColor>* userColors) { userColors_ = userColors; }
    void setFileLinks(const std::vector<FileLink>* links) { links_ = links; }

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override
    {
        painter->fillRect(rect, QColor("#fdfcf8"));
        painter->setPen(QPen(QColor(216, 213, 205, 170), 1.0));
        const int grid = 28;
        const qreal left = std::floor(rect.left() / grid) * grid;
        const qreal top = std::floor(rect.top() / grid) * grid;
        for (qreal x = left; x < rect.right(); x += grid) {
            for (qreal y = top; y < rect.bottom(); y += grid) {
                painter->drawPoint(QPointF(x, y));
            }
        }
        if (root_) {
            painter->setRenderHint(QPainter::Antialiasing);
            drawEdges(painter, *root_);
            drawFileLinks(painter);
        }
    }

private:
    Node* findNodeByPath(Node& node, const QString& path) const
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

    void drawFileLinks(QPainter* painter)
    {
        if (!root_ || !links_) {
            return;
        }

        for (const FileLink& link : *links_) {
            Node* from = findNodeByPath(*root_, link.from);
            Node* to = findNodeByPath(*root_, link.to);
            if (!from || !to || from->isDir || to->isDir) {
                continue;
            }

            const QPointF start(from->center.x() + from->size.width() / 2.0 + 6.0, from->center.y());
            const QPointF end(to->center.x() - to->size.width() / 2.0 - 6.0, to->center.y());
            const qreal distance = std::max<qreal>(72.0, end.x() - start.x());
            QPainterPath path(start);
            path.cubicTo(QPointF(start.x() + distance * 0.45, start.y()),
                         QPointF(end.x() - distance * 0.45, end.y()),
                         end);
            QColor color = neutralStroke();
            color.setAlpha(125);
            painter->setPen(QPen(color, 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(path);
        }
    }

    bool hasIncomingFileLink(const Node& node) const
    {
        if (!links_ || node.isDir) {
            return false;
        }
        for (const FileLink& link : *links_) {
            if (link.to == node.path) {
                return true;
            }
        }
        return false;
    }

    void drawEdges(QPainter* painter, const Node& node)
    {
        if (node.children.empty()) {
            return;
        }

        const QPointF start(node.center.x() + node.size.width() / 2.0, node.center.y());

        for (const auto& child : node.children) {
            if (hasIncomingFileLink(*child)) {
                drawEdges(painter, *child);
                continue;
            }
            const QPointF end(child->center.x() - child->size.width() / 2.0, child->center.y());
            const qreal distance = std::max<qreal>(96.0, end.x() - start.x());
            const qreal splitX = start.x() + std::clamp(distance * 0.46, 58.0, 128.0);
            const QPointF split(splitX, end.y());

            QPainterPath path(start);
            path.cubicTo(QPointF(start.x() + distance * 0.16, start.y()),
                         QPointF(split.x() - 34.0, split.y()),
                         split);
            path.cubicTo(QPointF(split.x() + 30.0, split.y()),
                         QPointF(end.x() - 38.0, end.y()),
                         end);
            QColor color = neutralStroke();
            color.setAlpha(125);
            const qreal width = child->isDir ? (node.depth == 0 ? 2.6 : 2.1) : 1.1;
            painter->setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(path);
            drawEdges(painter, *child);
        }
    }

    Node* root_ = nullptr;
    const std::map<QString, QColor>* userColors_ = nullptr;
    const std::vector<FileLink>* links_ = nullptr;
};

class BoardView final : public QGraphicsView {
public:
    explicit BoardView(QWidget* parent = nullptr) : QGraphicsView(parent)
    {
        setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
        setDragMode(QGraphicsView::NoDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorUnderMouse);
        setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
        setFocusPolicy(Qt::StrongFocus);
        setAcceptDrops(true);
        viewport()->setAcceptDrops(true);
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

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        const QPoint delta = event->angleDelta();
        if (!delta.isNull()) {
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
            if ((keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) &&
                keyHandler_ && keyHandler_(keyEvent)) {
                event->accept();
                return true;
            }
        }
        if (event->type() == QEvent::NativeGesture) {
            auto* gesture = static_cast<QNativeGestureEvent*>(event);
            if (gesture->gestureType() == Qt::ZoomNativeGesture) {
                const qreal factor = 1.0 + gesture->value();
                if (factor > 0.0) {
                    zoomAt(gesture->position(), factor);
                }
                event->accept();
                return true;
            }
        }
        return QGraphicsView::event(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        const bool rightButtonCanvasPan = event->button() == Qt::RightButton && itemAt(event->pos()) == nullptr;
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
        QGraphicsView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (panning_) {
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

    void keyPressEvent(QKeyEvent* event) override
    {
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
            event->accept();
            return;
        }
        QGraphicsView::keyPressEvent(event);
    }

private:
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
        if (!currentScene || !lastRubberBandSceneRect_.isValid()) {
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

    bool panning_ = false;
    bool rubberBandSelecting_ = false;
    Qt::MouseButton panningButton_ = Qt::NoButton;
    QPoint lastPanPoint_;
    QPoint rubberBandStart_;
    QRectF lastRubberBandSceneRect_;
    std::function<void()> cheatSheetHandler_;
    std::function<bool(QKeyEvent*)> keyHandler_;
    std::function<void()> viewChangedHandler_;
};

class TextEditor final : public QPlainTextEdit {
public:
    explicit TextEditor(QWidget* parent = nullptr) : QPlainTextEdit(parent)
    {
        instances().push_back(this);
        applyConfiguredFont();
    }

    ~TextEditor() override
    {
        auto& editors = instances();
        editors.erase(std::remove(editors.begin(), editors.end(), this), editors.end());
    }

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        if (event->modifiers() & Qt::ControlModifier) {
            const QPoint delta = event->angleDelta();
            const QPoint pixelDelta = event->pixelDelta();
            const int amount = !delta.isNull() ? delta.y() : pixelDelta.y();
            if (amount != 0) {
                setConfiguredPointSize(configuredPointSize() + (amount > 0 ? 1 : -1));
            }
            event->accept();
            return;
        }
        QPlainTextEdit::wheelEvent(event);
    }

private:
    static constexpr int DefaultPointSize = 10;
    static constexpr int MinPointSize = 7;
    static constexpr int MaxPointSize = 32;

    static std::vector<TextEditor*>& instances()
    {
        static std::vector<TextEditor*> editors;
        return editors;
    }

    static int configuredPointSize()
    {
        QSettings settings;
        return std::clamp(settings.value(QStringLiteral("editor/fontPointSize"), DefaultPointSize).toInt(),
                          MinPointSize, MaxPointSize);
    }

    static QFont editorFont(int pointSize)
    {
        QFont font(QStringLiteral("Consolas"));
        font.setStyleHint(QFont::Monospace);
        font.setPointSize(pointSize);
        return font;
    }

    static void setConfiguredPointSize(int pointSize)
    {
        const int clampedPointSize = std::clamp(pointSize, MinPointSize, MaxPointSize);
        QSettings settings;
        settings.setValue(QStringLiteral("editor/fontPointSize"), clampedPointSize);
        settings.sync();
        const QFont font = editorFont(clampedPointSize);
        for (TextEditor* editor : instances()) {
            if (editor) {
                editor->setFont(font);
            }
        }
    }

    void applyConfiguredFont()
    {
        setFont(editorFont(configuredPointSize()));
    }
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

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QString rootPath, bool mycelStorageEnabled, QWidget* parent = nullptr)
        : QMainWindow(parent), rootPath_(std::move(rootPath)), mycelStorageEnabled_(mycelStorageEnabled)
    {
        resize(1280, 820);

        editorSplitter_ = new QSplitter(Qt::Horizontal, this);

        view_ = new BoardView(editorSplitter_);
        view_->setScene(&scene_);
        view_->setCheatSheetHandler([this] { showCheatSheet(); });
        view_->setKeyHandler([this](QKeyEvent* event) { return handleBoardShortcut(event); });
        view_->setViewChangedHandler([this] { scheduleViewStateSave(); });

        sideEditorPanel_ = new QWidget(editorSplitter_);
        auto* editorLayout = new QVBoxLayout(sideEditorPanel_);
        editorLayout->setContentsMargins(10, 8, 10, 8);
        editorLayout->setSpacing(6);
        sideEditorPathLabel_ = new QLabel(sideEditorPanel_);
        sideEditorPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        sideEditorPathLabel_->setWordWrap(true);
        sideEditorStatusLabel_ = new QLabel(sideEditorPanel_);
        sidePreviewStack_ = new QStackedWidget(sideEditorPanel_);
        sidePreviewText_ = new QTextEdit(sidePreviewStack_);
        sidePreviewText_->setReadOnly(true);
        sidePreviewText_->setUndoRedoEnabled(false);
        sidePreviewText_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        sidePreviewText_->setLineWrapMode(QTextEdit::WidgetWidth);
        sidePreviewImage_ = new QLabel(sidePreviewStack_);
        sidePreviewImage_->setAlignment(Qt::AlignCenter);
        sidePreviewImage_->setScaledContents(true);
        sidePreviewVideo_ = new QVideoWidget(sidePreviewStack_);
        sideEditor_ = new TextEditor(sideEditorPanel_);
        sideEditor_->setReadOnly(true);
        sideEditor_->setPlaceholderText({});
        sidePreviewStack_->addWidget(sidePreviewText_);
        sidePreviewStack_->addWidget(sidePreviewImage_);
        sidePreviewStack_->addWidget(sidePreviewVideo_);
        sidePreviewStack_->addWidget(sideEditor_);
        sidePreviewAudioOutput_ = new QAudioOutput(this);
        sidePreviewAudioOutput_->setMuted(true);
        sidePreviewPlayer_ = new QMediaPlayer(this);
        sidePreviewPlayer_->setAudioOutput(sidePreviewAudioOutput_);
        sidePreviewPlayer_->setVideoOutput(sidePreviewVideo_);
        editorLayout->addWidget(sideEditorPathLabel_);
        editorLayout->addWidget(sideEditorStatusLabel_);
        editorLayout->addWidget(sidePreviewStack_, 1);

        editorSplitter_->addWidget(view_);
        editorSplitter_->addWidget(sideEditorPanel_);
        setCentralWidget(editorSplitter_);

        auto* toolbar = addToolBar(QStringLiteral("Mycel"));
        QAction* openAction = toolbar->addAction(QStringLiteral("Open"));
        QAction* refreshAction = toolbar->addAction(QStringLiteral("Refresh"));
        refreshAction->setShortcut(QKeySequence(Qt::Key_F5));
        QAction* fitAction = toolbar->addAction(QStringLiteral("Fit"));
        fitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
        QAction* exportAction = toolbar->addAction(QStringLiteral("Export"));
        QAction* importAction = toolbar->addAction(QStringLiteral("Import"));
        QAction* openSelectedPreviewsAction = toolbar->addAction(QStringLiteral("Open Previews"));
        QAction* closeSelectedPreviewsAction = toolbar->addAction(QStringLiteral("Close Previews"));
        editorPaneAction_ = toolbar->addAction(QStringLiteral("Preview"));
        editorPaneAction_->setCheckable(true);
        editorPaneAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
        editorPaneAction_->setShortcutContext(Qt::ApplicationShortcut);
        editorPaneAction_->setChecked(QSettings().value(QStringLiteral("editor/paneVisible"), true).toBool());
        sideEditorPanel_->setVisible(editorPaneAction_->isChecked());
        auto* editorPositionButton = new QToolButton(this);
        editorPositionButton->setText(QStringLiteral("Preview Place"));
        editorPositionButton->setPopupMode(QToolButton::InstantPopup);
        auto* editorPositionMenu = new QMenu(editorPositionButton);
        editorPositionButton->setMenu(editorPositionMenu);
        auto* editorPositionGroup = new QActionGroup(this);
        editorPositionGroup->setExclusive(true);
        QAction* editorLeftAction = editorPositionMenu->addAction(QStringLiteral("Left"));
        QAction* editorRightAction = editorPositionMenu->addAction(QStringLiteral("Right"));
        QAction* editorBottomAction = editorPositionMenu->addAction(QStringLiteral("Bottom"));
        for (QAction* action : {editorLeftAction, editorRightAction, editorBottomAction}) {
            action->setCheckable(true);
            editorPositionGroup->addAction(action);
        }
        editorLeftAction->setData(QStringLiteral("left"));
        editorRightAction->setData(QStringLiteral("right"));
        editorBottomAction->setData(QStringLiteral("bottom"));
        toolbar->addWidget(editorPositionButton);
        applyEditorPanePosition(QSettings().value(QStringLiteral("editor/panePosition"), QStringLiteral("right")).toString(),
                                false);
        for (QAction* action : editorPositionGroup->actions()) {
            if (action->data().toString() == editorPanePosition_) {
                action->setChecked(true);
                break;
            }
        }
        QAction* renameSelectedAction = new QAction(this);
        renameSelectedAction->setShortcut(QKeySequence(Qt::Key_F2));
        renameSelectedAction->setShortcutContext(Qt::ApplicationShortcut);
        addAction(renameSelectedAction);
        QAction* maximizeAction = new QAction(this);
        maximizeAction->setShortcut(QKeySequence(Qt::Key_F11));
        maximizeAction->setShortcutContext(Qt::ApplicationShortcut);
        addAction(maximizeAction);
        QAction* quitAction = new QAction(this);
        quitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
        quitAction->setShortcutContext(Qt::ApplicationShortcut);
        addAction(quitAction);

        connect(openAction, &QAction::triggered, this, [this] {
            const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Open directory"), rootPath_);
            if (!dir.isEmpty()) {
                saveViewState();
                saveCollapsedFile();
                rootPath_ = normalizedDirectoryPath(dir);
                rememberRootPath(rootPath_);
                collapsedPaths_.clear();
                previewPaths_.clear();
                previewSizes_.clear();
                loadOrderFile();
                loadColorFile();
                loadPreviewFile();
                loadLinkFile();
                if (!loadCollapsedFile()) {
                    applyLargeTreeStartupCollapse();
                }
                restoreWindowStateFromSettingsFile();
                rebuild(true);
                QTimer::singleShot(0, this, [this] { syncEditorPaneVisibility(); });
            }
        });
        connect(refreshAction, &QAction::triggered, this, [this] { refreshAll(); });
        connect(fitAction, &QAction::triggered, this, [this] { fitToMap(); });
        connect(exportAction, &QAction::triggered, this, [this] { exportArchive(); });
        connect(importAction, &QAction::triggered, this, [this] { importArchive(); });
        connect(openSelectedPreviewsAction, &QAction::triggered, this, [this] { setSelectedFilePreviews(true); });
        connect(closeSelectedPreviewsAction, &QAction::triggered, this, [this] { setSelectedFilePreviews(false); });
        connect(editorPositionGroup, &QActionGroup::triggered, this, [this](QAction* action) {
            applyEditorPanePosition(action->data().toString(), true);
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
        connect(&scene_, &QGraphicsScene::selectionChanged, this, [this] { updateSideEditorForSelection(); });
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
        restoreWindowStateFromSettingsFile();
        rebuild(true);
        QTimer::singleShot(0, this, [this] { syncEditorPaneVisibility(); });
    }

    bool mycelStorageEnabled() const
    {
        return mycelStorageEnabled_;
    }

    Node* rootNode() const
    {
        return root_.get();
    }

    void createFolder(Node* parent)
    {
        if (!parent || !parent->isDir) {
            return;
        }

        const QString parentPath = parent->path;
        QDir dir(parent->path);
        QString name = QStringLiteral("NewFolder");
        QString path = dir.filePath(name);
        for (int number = 2; QFileInfo::exists(path); ++number) {
            name = QStringLiteral("NewFolder %1").arg(number);
            path = dir.filePath(name);
        }

        if (!dir.mkdir(name)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("フォルダを作成できませんでした。"));
            return;
        }
        appendCreatedItemToOrder(parent->path, name, true);
        saveOrderFile();
        rebuild(false);
        restoreFolderSelection(parentPath);
    }

    void createFile(Node* parent, const QString& selectionPathAfterCreate = {})
    {
        if (!parent || !parent->isDir) {
            return;
        }

        const QString parentPath = parent->path;
        const QString restorePath = selectionPathAfterCreate.isEmpty() ? parentPath : selectionPathAfterCreate;
        QDir dir(parent->path);
        QString name = QStringLiteral("NewFile.txt");
        QString path = dir.filePath(name);
        for (int number = 2; QFileInfo::exists(path); ++number) {
            name = QStringLiteral("NewFile %1.txt").arg(number);
            path = dir.filePath(name);
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("ファイルを作成できませんでした。"));
            return;
        }
        appendCreatedItemToOrder(parent->path, name, false);
        saveOrderFile();
        rebuild(false);
        selectNodePath(restorePath);
    }

    void createFileInSelectedFolder()
    {
        Node* node = singleSelectedNode();
        if (!node) {
            return;
        }
        if (node->isDir) {
            createFile(node);
            return;
        }

        Node* parent = findVisibleNodeByPath(root_.get(), node->parentPath);
        if (!parent || !parent->isDir) {
            return;
        }
        createFile(parent, node->path);
    }

    void createFolderInSelectedFolder()
    {
        Node* node = singleSelectedNode();
        if (!node || !node->isDir) {
            return;
        }
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

    bool pasteCopiedItems()
    {
        if (copiedPaths_.isEmpty()) {
            return false;
        }

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

        if (!failed.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 QStringLiteral("コピーできなかった項目があります。\n%1").arg(failed.join(QLatin1Char('\n'))));
        }
        return true;
    }

    bool focusEditorForSelectedFile()
    {
        Node* node = singleSelectedNode();
        if (!canEditTextFile(node)) {
            return false;
        }

        if (editorPaneAction_ && !editorPaneAction_->isChecked()) {
            editorPaneAction_->setChecked(true);
        } else {
            sideEditorPanel_->setVisible(true);
            updateSideEditorForSelection();
        }
        loadSideEditorFile(node->path);
        sideEditorStatusLabel_->setText(QStringLiteral("編集中"));
        sideEditor_->setFocus(Qt::ShortcutFocusReason);
        sideEditor_->moveCursor(QTextCursor::End);
        return true;
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
            loadSidePreviewFile(focusPath);
            selectNodePath(focusPath);
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
        bool hasSelectedFile = false;
        bool allPreviewsOpen = true;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem || !nodeItem->node() || nodeItem->node()->isDir) {
                continue;
            }
            hasSelectedFile = true;
            if (!previewPaths_.contains(nodeItem->node()->path)) {
                allPreviewsOpen = false;
            }
        }

        if (!hasSelectedFile) {
            return false;
        }
        setSelectedFilePreviews(!allPreviewsOpen);
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
            restoreSelection(selectedPaths);
        } else {
            view_->setFocus(Qt::ShortcutFocusReason);
        }
        return true;
    }

    bool toggleSelectedPreviewOrCollapse()
    {
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
        if (renameEdit_ || textInputHasFocus()) {
            return false;
        }

        const Qt::KeyboardModifiers modifiers = event->modifiers();
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
            return moveSelectionVertically(true);
        }
        if (event->key() == Qt::Key_Down && modifiers == Qt::NoModifier) {
            return moveSelectionVertically(false);
        }
        if (event->key() == Qt::Key_Left && modifiers == Qt::NoModifier) {
            return moveSelectionToParent();
        }
        if (event->key() == Qt::Key_Right && modifiers == Qt::NoModifier) {
            return moveSelectionToFirstChild();
        }
        if (event->key() == Qt::Key_N && modifiers == Qt::NoModifier) {
            createFileInSelectedFolder();
            return true;
        }
        if (event->key() == Qt::Key_N && modifiers == Qt::ShiftModifier) {
            createFolderInSelectedFolder();
            return true;
        }
        if (event->key() == Qt::Key_D && modifiers == Qt::NoModifier) {
            if (selectedDeletableItemCount() <= 0) {
                return false;
            }
            deleteSelectedItems();
            return true;
        }
        if (event->key() == Qt::Key_C && modifiers == Qt::ControlModifier) {
            return copySelectedItems();
        }
        if (event->key() == Qt::Key_V && modifiers == Qt::ControlModifier) {
            return pasteCopiedItems();
        }
        return false;
    }

    bool textInputHasFocus() const
    {
        for (QWidget* widget = QApplication::focusWidget(); widget; widget = widget->parentWidget()) {
            if (qobject_cast<QLineEdit*>(widget) ||
                qobject_cast<QPlainTextEdit*>(widget) ||
                qobject_cast<QTextEdit*>(widget)) {
                return true;
            }
        }
        return false;
    }

    void openNode(Node* node)
    {
        if (!node) {
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(node->path));
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

    void editTextFile(Node* node)
    {
        if (!canEditTextFile(node)) {
            QMessageBox::information(this, QStringLiteral("Mycel"), QStringLiteral("このファイルは内蔵エディタで編集できません。"));
            return;
        }

        TextEditorDialog dialog(node->path, this);
        dialog.exec();
        if (dialog.wasSaved()) {
            rebuild(false);
        }
    }

    void updateSideEditorForSelection()
    {
        if (sideEditorEditing_ && !saveSideEditorNow()) {
            return;
        }
        sideEditorEditing_ = false;
        Node* node = singleSelectedNode();
        if (node && !node->isDir) {
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
        sidePreviewImage_->clear();
        sidePreviewStack_->setCurrentWidget(sidePreviewText_);
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
        sideEditorLoading_ = true;
        sideEditor_->setReadOnly(false);
        sideEditor_->setPlainText(QString::fromUtf8(file.readAll()));
        sideEditor_->moveCursor(QTextCursor::Start);
        sideEditorLoading_ = false;
        sidePreviewStack_->setCurrentWidget(sideEditor_);
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

        const QFileInfo info(path);
        if (!info.exists() || !info.isFile()) {
            showSidePreviewText(QStringLiteral("ファイルを開けませんでした。"), QStringLiteral("読み込み失敗"));
            return;
        }
        if (info.size() > 32 * 1024 * 1024 && !isVideoPreviewFile(info)) {
            showSidePreviewText(QStringLiteral("ファイルが大きいためプレビューしません。"), QStringLiteral("プレビュー不可"));
            return;
        }

        if (isImagePreviewFile(info)) {
            QPixmap pixmap(info.absoluteFilePath());
            if (pixmap.isNull()) {
                showSidePreviewText(QStringLiteral("画像を読み込めませんでした。"), QStringLiteral("画像プレビュー失敗"));
                return;
            }
            sidePreviewImage_->setPixmap(pixmap);
            sidePreviewStack_->setCurrentWidget(sidePreviewImage_);
            sideEditorStatusLabel_->setText(QStringLiteral("画像プレビュー"));
            return;
        }

        if (isVideoPreviewFile(info)) {
            sidePreviewPlayer_->setSource(QUrl::fromLocalFile(info.absoluteFilePath()));
            sidePreviewStack_->setCurrentWidget(sidePreviewVideo_);
            sidePreviewPlayer_->play();
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
            sideEditorStatusLabel_->setText(QStringLiteral("Markdown プレビュー"));
        } else if (isHtmlPreviewFile(info)) {
            sidePreviewText_->setHtml(text);
            sideEditorStatusLabel_->setText(QStringLiteral("HTML プレビュー"));
        } else if (isCsvPreviewFile(info)) {
            sidePreviewText_->setHtml(csvToHtmlTable(text));
            sideEditorStatusLabel_->setText(QStringLiteral("CSV プレビュー"));
        } else {
            sidePreviewText_->setPlainText(filterPreviewMetadataLines(text));
            sideEditorStatusLabel_->setText(QStringLiteral("テキストプレビュー"));
        }
        sidePreviewStack_->setCurrentWidget(sidePreviewText_);
    }

    void showSidePreviewText(const QString& text, const QString& status)
    {
        stopSidePreviewMedia();
        sidePreviewText_->clear();
        sidePreviewText_->setPlainText(text);
        sidePreviewStack_->setCurrentWidget(sidePreviewText_);
        sideEditorStatusLabel_->setText(status);
    }

    void stopSidePreviewMedia()
    {
        if (sidePreviewPlayer_) {
            sidePreviewPlayer_->stop();
            sidePreviewPlayer_->setSource(QUrl());
        }
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
        while (lines.size() < 7 && !stream.atEnd()) {
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
            previewPaths_.insert(node->path);
        } else {
            previewPaths_.remove(node->path);
        }
        savePreviewFile();
        rebuild(false);
        selectNodePath(node->path);
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
            previewPaths_.remove(path);
        } else {
            previewPaths_.insert(path);
        }
        savePreviewFile();
        rebuild(false);
        selectNodePath(path);
    }

    void setPreviewSize(Node* node, const QSizeF& size)
    {
        if (!node || node->isDir) {
            return;
        }
        previewSizes_[node->path] = QSizeF(std::clamp(size.width(), 260.0, 900.0),
                                           std::clamp(size.height(), 110.0, 520.0));
        savePreviewFile();
        rebuild(false);
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
        if (collapsedPaths_.contains(path)) {
            collapsedPaths_.remove(path);
        } else {
            collapsedPaths_.insert(path);
        }
        saveCollapsedFile();
        rebuild(false);
        selectNodePath(path);
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
        userColors_[node->path] = color;
        saveColorFile();
        rebuild(false);
    }

    void clearNodeColor(Node* node)
    {
        if (!node || !mycelStorageEnabled_) {
            return;
        }
        userColors_.erase(node->path);
        saveColorFile();
        rebuild(false);
    }

    void beginInlineRename(Node* node)
    {
        if (!node || node == root_.get()) {
            return;
        }

        finishInlineRename(false);

        NodeItem* target = nullptr;
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node() == node) {
                target = nodeItem;
                break;
            }
        }
        if (!target) {
            return;
        }

        QFileInfo info(node->path);
        renamingPath_ = node->path;
        renameEdit_ = new QLineEdit(info.fileName());
        renameEdit_->setFrame(false);
        renameEdit_->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #ffffff; color: #172321; border: 1px solid #1e8cff; "
            "border-radius: 5px; padding: 2px 5px; selection-background-color: #1e8cff; "
            "selection-color: #ffffff; }"));
        renameEdit_->installEventFilter(this);

        renameProxy_ = scene_.addWidget(renameEdit_);
        renameProxy_->setZValue(300.0);
        const QRectF rect = target->labelSceneRect().adjusted(-2.0, -2.0, 8.0, 2.0);
        renameProxy_->setPos(rect.topLeft());
        renameProxy_->resize(rect.size());

        const int dot = info.fileName().lastIndexOf(QLatin1Char('.'));
        if (!node->isDir && dot > 0) {
            renameEdit_->setSelection(0, dot);
        } else {
            renameEdit_->selectAll();
        }
        renameEdit_->setFocus(Qt::OtherFocusReason);

        connect(renameEdit_, &QLineEdit::editingFinished, this, [this] {
            finishInlineRename(true);
        });
    }

    void renameFile(Node* node)
    {
        beginInlineRename(node);
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
                "編集モード内 Ctrl + S : 保存し、1行目の Subject: に合わせてファイル名を変更して Subject 行を削除\n"
                "編集モード内 Esc : 保存してプレビューへ戻る\n"
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
        if (QFileInfo::exists(destination)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("同名のファイルまたはフォルダが既にあります。"));
            return false;
        }
        const bool wasDir = info.isDir();
        if (!QFile::rename(path, destination)) {
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
        return true;
    }

    void deleteFile(Node* node)
    {
        if (!node || node->isDir) {
            return;
        }
        const QString message = QStringLiteral("このファイルを削除しますか？\n%1").arg(node->path);
        if (QMessageBox::question(this, QStringLiteral("Mycel"), message,
                                  QMessageBox::Yes | QMessageBox::Cancel,
                                  QMessageBox::Cancel) != QMessageBox::Yes) {
            return;
        }
        if (!QFile::remove(node->path)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("ファイルを削除できませんでした。"));
            return;
        }
        removeDeletedPathMetadata(node->path, false);
        saveColorFile();
        saveOrderFile();
        savePreviewFile();
        saveLinkFile();
        saveCollapsedFile();
        rebuild(false);
    }

    void deleteFolder(Node* node)
    {
        if (!node || !node->isDir || node == root_.get()) {
            return;
        }
        const QString message = QStringLiteral("このフォルダと配下の項目を削除しますか？\n%1").arg(node->path);
        if (QMessageBox::question(this, QStringLiteral("Mycel"), message,
                                  QMessageBox::Yes | QMessageBox::Cancel,
                                  QMessageBox::Cancel) != QMessageBox::Yes) {
            return;
        }

        const QString folderPath = node->path;
        QDir dir(folderPath);
        if (!dir.removeRecursively()) {
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
    }

    void moveNode(Node* source, Node* targetDir)
    {
        if (!source || !targetDir || !targetDir->isDir || source == root_.get()) {
            rebuild(false);
            return;
        }
        if (source->path == targetDir->path || isDescendantPath(source->path, targetDir->path)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("自分自身または配下へは移動できません。"));
            rebuild(false);
            return;
        }

        const QString destination = QDir(targetDir->path).filePath(QFileInfo(source->path).fileName());
        if (QFileInfo::exists(destination)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("移動先に同名の項目があります。"));
            rebuild(false);
            return;
        }

        std::error_code ec;
        std::filesystem::rename(std::filesystem::path(source->path.toStdString()),
                                std::filesystem::path(destination.toStdString()), ec);
        if (ec) {
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 QStringLiteral("移動できませんでした。\n%1").arg(QString::fromStdString(ec.message())));
            rebuild(false);
            return;
        }

        rekeyPathMetadataAfterRename(source->path, destination, source->isDir);
        saveColorFile();
        savePreviewFile();
        saveLinkFile();
        saveCollapsedFile();
        if (mycelStorageEnabled_ && !source->isDir) {
            QStringList& sourceOrder = fileOrders_[orderKeyForDirectory(QFileInfo(source->path).absolutePath())];
            sourceOrder.removeAll(QFileInfo(source->path).fileName());
            QStringList& targetOrder = fileOrders_[orderKeyForDirectory(targetDir->path)];
            if (!targetOrder.contains(QFileInfo(source->path).fileName())) {
                targetOrder.append(QFileInfo(source->path).fileName());
            }
            saveOrderFile();
        }
        rebuild(false);
    }

    void moveDragItemsToFolder(NodeItem* sourceItem, Node* targetDir)
    {
        std::vector<NodeItem*> dragItems = selectedTopLevelDragItems(sourceItem);
        if (dragItems.size() <= 1) {
            moveNode(sourceItem ? sourceItem->node() : nullptr, targetDir);
            return;
        }
        if (!targetDir || !targetDir->isDir) {
            clearDragPreview();
            return;
        }

        struct MoveEntry {
            Node* node;
            QString oldPath;
            QString destination;
            QString sourceParentPath;
            QString name;
            bool isDir;
            bool needsMove;
        };

        std::vector<MoveEntry> entries;
        QStringList destinationNames;
        for (NodeItem* item : dragItems) {
            Node* node = item ? item->node() : nullptr;
            if (!node || node == root_.get()) {
                continue;
            }
            if (node->path == targetDir->path || (node->isDir && isDescendantPath(node->path, targetDir->path))) {
                QMessageBox::warning(this, QStringLiteral("Mycel"),
                                     QStringLiteral("自分自身または配下へは移動できません。"));
                rebuild(false);
                return;
            }

            QFileInfo info(node->path);
            const QString destination = QDir(targetDir->path).filePath(info.fileName());
            destinationNames.append(info.fileName());
            entries.push_back({node, node->path, destination, info.absolutePath(), info.fileName(),
                               node->isDir, QDir::cleanPath(destination) != QDir::cleanPath(node->path)});
        }

        if (entries.empty()) {
            clearDragPreview();
            return;
        }

        destinationNames.sort(Qt::CaseInsensitive);
        for (int i = 1; i < destinationNames.size(); ++i) {
            if (destinationNames[i].compare(destinationNames[i - 1], Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(this, QStringLiteral("Mycel"),
                                     QStringLiteral("移動先で同名になる項目があります。"));
                rebuild(false);
                return;
            }
        }

        for (const MoveEntry& entry : entries) {
            if (!entry.needsMove) {
                continue;
            }
            if (QFileInfo::exists(entry.destination)) {
                QMessageBox::warning(this, QStringLiteral("Mycel"),
                                     QStringLiteral("移動先に同名の項目があります。\n%1").arg(entry.destination));
                rebuild(false);
                return;
            }
        }

        QStringList failed;
        for (const MoveEntry& entry : entries) {
            if (!entry.needsMove) {
                continue;
            }

            std::error_code ec;
            std::filesystem::rename(std::filesystem::path(entry.oldPath.toStdString()),
                                    std::filesystem::path(entry.destination.toStdString()), ec);
            if (ec) {
                failed.append(QStringLiteral("%1\n%2").arg(entry.oldPath, QString::fromStdString(ec.message())));
                continue;
            }
            rekeyPathMetadataAfterRename(entry.oldPath, entry.destination, entry.isDir);
            if (mycelStorageEnabled_) {
                QStringList& sourceOrder = fileOrders_[orderKeyForDirectory(entry.sourceParentPath)];
                sourceOrder.removeAll(entry.name);
                QStringList& targetOrder = fileOrders_[orderKeyForDirectory(targetDir->path)];
                if (!targetOrder.contains(entry.name)) {
                    targetOrder.append(entry.name);
                }
            }
        }

        saveColorFile();
        savePreviewFile();
        saveOrderFile();
        saveLinkFile();
        saveCollapsedFile();
        rebuild(false);

        if (!failed.isEmpty()) {
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
        }

        if (!importedNames.isEmpty()) {
            saveOrderFile();
            rebuild(false);
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

        QString destinationPath;
        bool saved = false;
        if (mimeData->hasImage()) {
            const QImage image = qvariant_cast<QImage>(mimeData->imageData());
            if (!image.isNull()) {
                destinationPath = availableImportPath(targetDir->path, QStringLiteral("貼り付け画像.png"), false);
                saved = image.save(destinationPath, "PNG");
            }
        } else if (mimeData->hasHtml()) {
            destinationPath = availableImportPath(targetDir->path, QStringLiteral("貼り付け.html"), false);
            saved = writeBytes(destinationPath, mimeData->html().toUtf8());
        } else if (mimeData->hasText()) {
            destinationPath = availableImportPath(targetDir->path, QStringLiteral("貼り付け.txt"), false);
            saved = writeBytes(destinationPath, mimeData->text().toUtf8());
        } else if (const auto binaryFormat = pasteableBinaryFormat(mimeData)) {
            const QString extension = extensionForMimeFormat(*binaryFormat);
            destinationPath = availableImportPath(targetDir->path, QStringLiteral("貼り付け.%1").arg(extension), false);
            saved = writeBytes(destinationPath, mimeData->data(*binaryFormat));
        }

        if (!saved || destinationPath.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("クリップボードの内容を貼り付けできませんでした。"));
            return;
        }

        const QString fileName = QFileInfo(destinationPath).fileName();
        appendCreatedItemToOrder(targetDir->path, fileName, false);
        saveOrderFile();
        rebuild(false);
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

    NodeItem* folderItemForDrop(const NodeItem* source) const
    {
        if (!source) {
            return nullptr;
        }

        const std::vector<NodeItem*> dragItems = selectedTopLevelDragItems(source);
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

        const QRectF dropRect = source->sceneBoundingRect().adjusted(-10.0, -10.0, 10.0, 10.0);
        NodeItem* best = nullptr;
        qreal bestOverlap = 0.0;
        for (QGraphicsItem* item : scene_.items(dropRect, Qt::IntersectsItemBoundingRect)) {
            auto* candidate = dynamic_cast<NodeItem*>(item);
            if (!candidate || candidate == source || !candidate->node()->isDir) {
                continue;
            }
            if (isMovingItem(candidate)) {
                continue;
            }

            const QRectF overlap = dropRect.intersected(candidate->sceneBoundingRect());
            const qreal area = overlap.width() * overlap.height();
            if (area > bestOverlap) {
                bestOverlap = area;
                best = candidate;
            }
        }
        return best;
    }

    NodeItem* linkTargetItemForDrop(const NodeItem* source, const QPointF& scenePos) const
    {
        if (!mycelStorageEnabled_ || !source || !source->node() || source->node()->isDir ||
            isMultiDragSelection(source)) {
            return nullptr;
        }

        for (QGraphicsItem* item : scene_.items(scenePos)) {
            auto* candidate = dynamic_cast<NodeItem*>(item);
            if (!candidate || candidate == source || !candidate->node() || candidate->node()->isDir) {
                continue;
            }
            if (candidate->containsLinkDropScenePoint(scenePos)) {
                return candidate;
            }
        }
        return nullptr;
    }

    NodeItem* updateInternalDropHover(NodeItem* source)
    {
        NodeItem* target = folderItemForDrop(source);
        if (target == dragHoverFolder_) {
            return target;
        }

        clearInternalDropHover();
        dragHoverFolder_ = target;
        if (dragHoverFolder_) {
            dragHoverFolder_->setInternalDropHover(true);
        }
        return dragHoverFolder_;
    }

    NodeItem* updateLinkDropHover(NodeItem* source, const QPointF& scenePos)
    {
        NodeItem* target = linkTargetItemForDrop(source, scenePos);
        if (target == dragHoverLinkTarget_) {
            return target;
        }

        clearLinkDropHover();
        dragHoverLinkTarget_ = target;
        if (dragHoverLinkTarget_) {
            dragHoverLinkTarget_->setLinkDropHover(true);
        }
        return dragHoverLinkTarget_;
    }

    void clearInternalDropHover()
    {
        if (!dragHoverFolder_) {
            return;
        }
        dragHoverFolder_->setInternalDropHover(false);
        dragHoverFolder_ = nullptr;
    }

    void clearLinkDropHover()
    {
        if (!dragHoverLinkTarget_) {
            return;
        }
        dragHoverLinkTarget_->setLinkDropHover(false);
        dragHoverLinkTarget_ = nullptr;
    }

    void addFileLink(Node* from, Node* to)
    {
        if (!mycelStorageEnabled_) {
            QMessageBox::information(this, QStringLiteral("Mycel"),
                                     QStringLiteral(".mycel なしの制限モードでは関連付けできません。"));
            return;
        }
        if (!from || !to || from->isDir || to->isDir || from->path == to->path) {
            return;
        }

        for (const FileLink& link : fileLinks_) {
            if (link.from == from->path && link.to == to->path) {
                rebuild(false);
                return;
            }
        }

        fileLinks_.push_back({from->path, to->path});
        saveLinkFile();
        rebuild(false);
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

    void removeIncomingFileLinks(Node* node)
    {
        if (!mycelStorageEnabled_ || !node || node->isDir) {
            return;
        }

        const auto oldSize = fileLinks_.size();
        fileLinks_.erase(std::remove_if(fileLinks_.begin(), fileLinks_.end(), [node](const FileLink& link) {
                             return link.to == node->path;
                         }),
                         fileLinks_.end());
        if (fileLinks_.size() == oldSize) {
            return;
        }

        saveLinkFile();
        rebuild(false);
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
        bool changed = false;
        QStringList selectedPaths;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem || !nodeItem->node()) {
                continue;
            }

            selectedPaths.append(nodeItem->node()->path);
            if (nodeItem->node()->isDir) {
                continue;
            }

            const QString path = nodeItem->node()->path;
            if (open) {
                if (!previewPaths_.contains(path)) {
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
            rebuild(false);
            restoreSelection(selectedPaths);
        }
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

        QStringList failed;
        for (const QString& path : filePaths) {
            if (!QFile::remove(path)) {
                failed.append(path);
                continue;
            }
            removeDeletedPathMetadata(path, false);
        }

        for (const QString& path : folderPaths) {
            QDir dir(path);
            if (!dir.removeRecursively()) {
                failed.append(path);
                continue;
            }
            removeDeletedPathMetadata(path, true);
        }

        saveColorFile();
        saveOrderFile();
        savePreviewFile();
        saveLinkFile();
        saveCollapsedFile();
        rebuild(false);

        if (!failed.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 QStringLiteral("削除できなかった項目があります。\n%1").arg(failed.join(QLatin1Char('\n'))));
        }
    }

    bool hasSelectedItems() const
    {
        return !scene_.selectedItems().isEmpty();
    }

    void restoreFolderSelection(const QString& folderPath)
    {
        selectNodePath(folderPath);
    }

    bool selectNodePath(const QString& path)
    {
        scene_.clearSelection();
        bool selected = false;
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node() && nodeItem->node()->path == path) {
                nodeItem->setSelected(true);
                selected = true;
                break;
            }
        }
        view_->setFocus(Qt::ShortcutFocusReason);
        return selected;
    }

    bool restoreSelection(const QStringList& paths)
    {
        scene_.clearSelection();
        bool selected = false;
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && nodeItem->node() && paths.contains(nodeItem->node()->path)) {
                nodeItem->setSelected(true);
                selected = true;
            }
        }
        view_->setFocus(Qt::ShortcutFocusReason);
        return selected;
    }

    bool moveSelectionWithTab(bool reverse)
    {
        if (!root_) {
            return false;
        }

        Node* current = singleSelectedNode();
        if (!current) {
            if (!root_->children.empty()) {
                return selectNodePath(reverse ? root_->children.back()->path : root_->children.front()->path);
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
                    return selectNodePath(parent->children[index - 1]->path);
                }
                if (parent != root_.get()) {
                    return selectNodePath(parent->path);
                }
                view_->setFocus(Qt::ShortcutFocusReason);
                return true;
            }
            if (index + 1 < parent->children.size()) {
                return selectNodePath(parent->children[index + 1]->path);
            }
            if (!current->children.empty()) {
                return selectNodePath(current->children.front()->path);
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
            if (!root_->children.empty()) {
                return selectNodePath(upward ? root_->children.back()->path : root_->children.front()->path);
            }
            view_->setFocus(Qt::ShortcutFocusReason);
            return true;
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
                return selectNodePath(parent->children[index - 1]->path);
            }
            if (!upward && index + 1 < parent->children.size()) {
                return selectNodePath(parent->children[index + 1]->path);
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
        if (!root_ || !current || current == root_.get()) {
            view_->setFocus(Qt::ShortcutFocusReason);
            return root_ != nullptr;
        }

        Node* parent = findVisibleNodeByPath(root_.get(), current->parentPath);
        if (!parent) {
            view_->setFocus(Qt::ShortcutFocusReason);
            return true;
        }
        return selectNodePath(parent->path);
    }

    bool moveSelectionToFirstChild()
    {
        Node* current = singleSelectedNode();
        if (!root_ || !current || !current->isDir) {
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
            return selectNodePath(folder->children.front()->path);
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
        if (watched == renameEdit_ && event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                finishInlineRename(false);
                return true;
            }
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                finishInlineRename(true);
                return true;
            }
        }
        return QMainWindow::eventFilter(watched, event);
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
            previewSizes[path] = QSizeF(std::clamp(width, 260.0, 900.0),
                                        std::clamp(height, 110.0, 520.0));
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

    void appendCreatedItemToOrder(const QString& directoryPath, const QString& newName, bool)
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
        order.append(newName);
        fileOrders_[key] = order;
    }

    QString availableImportPath(const QString& targetDirectoryPath, const QString& preferredName, bool isDirectory) const
    {
        QFileInfo preferredInfo(preferredName);
        const QString completeSuffix = preferredInfo.completeSuffix();
        const QString baseName = isDirectory || completeSuffix.isEmpty()
                                     ? preferredName
                                     : preferredName.left(preferredName.size() - completeSuffix.size() - 1);
        const QString suffix = isDirectory || completeSuffix.isEmpty() ? QString() : QStringLiteral(".") + completeSuffix;

        QDir target(targetDirectoryPath);
        QString candidate = target.filePath(preferredName);
        for (int number = 2; QFileInfo::exists(candidate); ++number) {
            candidate = target.filePath(QStringLiteral("%1 %2%3").arg(baseName).arg(number).arg(suffix));
        }
        return candidate;
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
        QDir source(sourcePath);
        if (!source.exists()) {
            return false;
        }

        QDir parent(QFileInfo(destinationPath).absolutePath());
        if (!parent.mkpath(QFileInfo(destinationPath).fileName())) {
            return false;
        }

        const QFileInfoList entries = source.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot |
                                                           QDir::Hidden | QDir::System);
        for (const QFileInfo& entry : entries) {
            const QString destinationChild = QDir(destinationPath).filePath(entry.fileName());
            if (entry.isDir()) {
                if (!copyDirectoryRecursively(entry.absoluteFilePath(), destinationChild)) {
                    return false;
                }
            } else {
                if (!QFile::copy(entry.absoluteFilePath(), destinationChild)) {
                    return false;
                }
            }
        }
        return true;
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

        QFile file(orderFilePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("並び順を保存できませんでした。"));
            return;
        }
        file.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
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

        QFile file(colorFilePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("色設定を保存できませんでした。"));
            return;
        }
        file.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
    }

    void loadPreviewFile()
    {
        previewPaths_.clear();
        previewSizes_.clear();
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
                previewPaths_.insert(absolutePath);
            }

            const qreal width = preview.value(QStringLiteral("width")).toDouble(0.0);
            const qreal height = preview.value(QStringLiteral("height")).toDouble(0.0);
            if (width > 0.0 && height > 0.0) {
                previewSizes_[absolutePath] = QSizeF(std::clamp(width, 260.0, 900.0),
                                                     std::clamp(height, 110.0, 520.0));
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

        QJsonObject previews;
        for (const QString& path : paths.values()) {
            QJsonObject preview;
            preview.insert(QStringLiteral("open"), previewPaths_.contains(path));
            const auto size = previewSizes_.find(path);
            if (size != previewSizes_.end()) {
                preview.insert(QStringLiteral("width"), size->second.width());
                preview.insert(QStringLiteral("height"), size->second.height());
            }
            previews.insert(relativeKeyForPath(path), preview);
        }

        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("previews"), previews);

        QFile file(previewFilePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("プレビュー状態を保存できませんでした。"));
            return;
        }
        file.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
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

        QFile file(collapsedFilePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("折りたたみ状態を保存できませんでした。"));
            return;
        }
        file.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
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

        QFile file(linkFilePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("関連設定を保存できませんでした。"));
            return;
        }
        file.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
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

    bool readViewStateObject(const QJsonObject& rootObject, QTransform& transform, int& horizontal, int& vertical) const
    {
        const QJsonObject transformObject = rootObject.value(QStringLiteral("transform")).toObject();
        const QJsonObject scrollObject = rootObject.value(QStringLiteral("scroll")).toObject();

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

    bool loadViewState(QTransform& transform, int& horizontal, int& vertical) const
    {
        const std::optional<QJsonObject> rootObject = loadViewStateObject();
        if (!rootObject) {
            return false;
        }

        return readViewStateObject(*rootObject, transform, horizontal, vertical);
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

        QFile file(viewStateFilePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return;
        }
        file.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
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
        if (restoringViewState_ || !mycelStorageEnabled_) {
            return;
        }
        viewStateSaveTimer_.start();
    }

    void rebuild(bool fitAfterRebuild)
    {
        saveSideEditorNow();
        finishInlineRename(false);
        setWindowTitle(windowTitleForRoot(rootPath_, mycelStorageEnabled_));

        const QTransform previousTransform = view_->transform();
        const int previousHScroll = view_->horizontalScrollBar()->value();
        const int previousVScroll = view_->verticalScrollBar()->value();

        scene_.clear();
        root_ = scanTree(rootPath_, 0, -1, collapsedPaths_, previewPaths_, previewSizes_, fileOrders_, rootPath_);
        assignTopLevelBranches(*root_);

        qreal yCursor = 0.0;
        layoutTree(*root_, 0.0, yCursor);
        layoutFileLinks(*root_, fileLinks_);
        translateTree(*root_, QPointF(140.0, 120.0 - root_->center.y()));

        scene_.setRoot(root_.get());
        scene_.setUserColors(&userColors_);
        scene_.setFileLinks(&fileLinks_);
        visitNodes(*root_, [this](Node& node) {
            scene_.addItem(new NodeItem(&node, this));
        });

        QRectF bounds;
        visitNodes(*root_, [&bounds](Node& node) {
            QRectF nodeRect(node.center.x() - node.size.width() / 2.0 - 220.0,
                            node.center.y() - node.size.height() / 2.0 - 80.0,
                            node.size.width() + 440.0,
                            node.size.height() + 160.0);
            if (node.previewOpen) {
                const QRectF previewRect(node.center.x() - node.size.width() / 2.0 + 40.0,
                                         node.center.y() + node.size.height() / 2.0 + 8.0,
                                         node.previewSize.width(),
                                         node.previewSize.height());
                nodeRect = nodeRect.united(previewRect.adjusted(-40.0, -40.0, 80.0, 40.0));
            }
            bounds = bounds.isNull() ? nodeRect : bounds.united(nodeRect);
        });
        scene_.setSceneRect(bounds.adjusted(-FreeCanvasMargin, -FreeCanvasMargin,
                                            FreeCanvasMargin, FreeCanvasMargin));
        if (fitAfterRebuild) {
            scheduleRestoreViewStateOrFit();
        } else {
            view_->setTransform(previousTransform);
            view_->horizontalScrollBar()->setValue(previousHScroll);
            view_->verticalScrollBar()->setValue(previousVScroll);
        }
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
        if (!loadViewState(transform, horizontal, vertical)) {
            fitToMap();
            return;
        }

        restoringViewState_ = true;
        view_->setTransform(transform);
        view_->horizontalScrollBar()->setValue(horizontal);
        view_->verticalScrollBar()->setValue(vertical);
        restoringViewState_ = false;
    }

    void fitToMap()
    {
        if (scene_.items().isEmpty()) {
            return;
        }
        view_->resetTransform();
        view_->fitInView(scene_.itemsBoundingRect().adjusted(-160.0, -160.0, 260.0, 160.0), Qt::KeepAspectRatio);
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
    QString queuedPreviewPath_;
    QString queuedCollapsePath_;
    QTimer previewClickTimer_;
    std::map<QString, QSizeF> previewSizes_;
    std::map<QString, QStringList> fileOrders_;
    QGraphicsProxyWidget* renameProxy_ = nullptr;
    QLineEdit* renameEdit_ = nullptr;
    QString renamingPath_;
    bool finishingRename_ = false;
    MindMapScene scene_;
    QSplitter* editorSplitter_ = nullptr;
    BoardView* view_ = nullptr;
    NodeItem* dragHoverFolder_ = nullptr;
    NodeItem* dragHoverLinkTarget_ = nullptr;
    QStringList copiedPaths_;
    std::vector<FileLink> fileLinks_;
    QAction* editorPaneAction_ = nullptr;
    QWidget* sideEditorPanel_ = nullptr;
    QStackedWidget* sidePreviewStack_ = nullptr;
    QTextEdit* sidePreviewText_ = nullptr;
    QLabel* sidePreviewImage_ = nullptr;
    QVideoWidget* sidePreviewVideo_ = nullptr;
    QMediaPlayer* sidePreviewPlayer_ = nullptr;
    QAudioOutput* sidePreviewAudioOutput_ = nullptr;
    TextEditor* sideEditor_ = nullptr;
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
    std::unique_ptr<Node> root_;
};

void NodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    QMenu menu;
    const bool multiItemSelection = isSelected() && window_->selectedDeletableItemCount() > 1;
    auto addColorMenu = [this](QMenu& parentMenu, std::vector<QAction*>& colorActions, QAction*& clearColorAction) {
        if (!window_->mycelStorageEnabled()) {
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
        clearColorAction->setEnabled(window_->hasUserColor(node_));
    };

    if (multiItemSelection) {
        QAction* previewSelectionAction = menu.addAction(QStringLiteral("プレビューを開く/閉じる"));
        previewSelectionAction->setEnabled(window_->hasSelectedFiles());
        QAction* copySelectionAction = menu.addAction(QStringLiteral("コピー"));
        QAction* deleteSelectionAction = menu.addAction(QStringLiteral("削除"));

        QAction* selected = menu.exec(event->screenPos());
        if (selected == previewSelectionAction) {
            window_->toggleSelectedFilePreviews();
        } else if (selected == copySelectionAction) {
            window_->copySelectedItems();
        } else if (selected == deleteSelectionAction) {
            window_->deleteSelectedItems();
        }
        return;
    }

    if (node_->isDir) {
        QAction* collapseAction = menu.addAction(node_->collapsed ? QStringLiteral("展開") : QStringLiteral("折りたたむ"));
        QAction* fileAction = menu.addAction(QStringLiteral("ファイルを作成"));
        QAction* folderAction = menu.addAction(QStringLiteral("フォルダを作成"));
        QAction* renameAction = menu.addAction(QStringLiteral("名前変更"));
        renameAction->setEnabled(node_ != window_->rootNode());
        QAction* copyAction = menu.addAction(QStringLiteral("コピー"));
        copyAction->setEnabled(node_ != window_->rootNode());
        QAction* pasteAction = menu.addAction(QStringLiteral("貼り付け"));
        pasteAction->setEnabled(window_->canPasteClipboardToFolder(node_));
        QAction* deleteAction = menu.addAction(QStringLiteral("削除"));
        deleteAction->setEnabled(window_->canDeleteFolder(node_));
        std::vector<QAction*> colorActions;
        QAction* clearColorAction = nullptr;
        addColorMenu(menu, colorActions, clearColorAction);
        QAction* openAction = menu.addAction(QStringLiteral("開く"));
        QAction* selected = menu.exec(event->screenPos());
        if (selected == collapseAction) {
            window_->toggleCollapsed(node_);
        } else if (selected == fileAction) {
            window_->createFile(node_);
        } else if (selected == folderAction) {
            window_->createFolder(node_);
        } else if (selected == renameAction) {
            window_->renameFile(node_);
        } else if (selected == copyAction) {
            window_->copyNode(node_);
        } else if (selected == pasteAction) {
            window_->pasteClipboardToFolder(node_);
        } else if (selected == deleteAction) {
            window_->deleteFolder(node_);
        } else if (selected && selected->data().canConvert<QColor>()) {
            window_->setNodeColor(node_, selected->data().value<QColor>());
        } else if (clearColorAction && selected == clearColorAction) {
            window_->clearNodeColor(node_);
        } else if (selected == openAction) {
            window_->openNode(node_);
        }
    } else {
        QAction* previewAction = menu.addAction(QStringLiteral("プレビューを開く/閉じる"));
        QAction* unlinkAction = menu.addAction(QStringLiteral("関連を解除"));
        unlinkAction->setEnabled(window_->hasIncomingFileLink(node_));
        QAction* editAction = menu.addAction(QStringLiteral("編集"));
        editAction->setEnabled(window_->canEditTextFile(node_));
        QAction* renameAction = menu.addAction(QStringLiteral("名前変更"));
        QAction* copyAction = menu.addAction(QStringLiteral("コピー"));
        QAction* deleteAction = menu.addAction(QStringLiteral("削除"));
        std::vector<QAction*> colorActions;
        QAction* clearColorAction = nullptr;
        addColorMenu(menu, colorActions, clearColorAction);
        QAction* openAction = menu.addAction(QStringLiteral("開く"));
        QAction* selected = menu.exec(event->screenPos());
        if (selected == previewAction) {
            window_->toggleInlinePreview(node_);
        } else if (selected == unlinkAction) {
            window_->removeIncomingFileLinks(node_);
        } else if (selected == editAction) {
            window_->editTextFile(node_);
        } else if (selected == renameAction) {
            window_->renameFile(node_);
        } else if (selected == copyAction) {
            window_->copyNode(node_);
        } else if (selected == deleteAction) {
            window_->deleteFile(node_);
        } else if (selected && selected->data().canConvert<QColor>()) {
            window_->setNodeColor(node_, selected->data().value<QColor>());
        } else if (clearColorAction && selected == clearColorAction) {
            window_->clearNodeColor(node_);
        } else if (selected == openAction) {
            window_->openNode(node_);
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

QString NodeItem::windowMarkdownPreviewText() const
{
    return window_->inlineMarkdownPreviewText(node_);
}

void NodeItem::resizePreview(const QSizeF& size)
{
    window_->setPreviewSize(node_, size);
}

void NodeItem::createPreviewWidget()
{
    if (!node_->previewOpen || node_->isDir) {
        return;
    }

    auto* textEdit = new QTextEdit;
    textEdit->setReadOnly(true);
    textEdit->setUndoRedoEnabled(false);
    textEdit->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    textEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    textEdit->setFrameStyle(0);
    textEdit->setAttribute(Qt::WA_TranslucentBackground);
    textEdit->viewport()->setAttribute(Qt::WA_TranslucentBackground);
    textEdit->viewport()->setAutoFillBackground(false);
    textEdit->document()->setDocumentMargin(8.0);
    textEdit->document()->setDefaultStyleSheet(QStringLiteral("* { color: #243036; } a { color: #1168b3; }"));
    textEdit->setStyleSheet(QStringLiteral(
        "QTextEdit { background: transparent; border: none; color: #243036; "
        "selection-background-color: #bfdbfe; selection-color: #111827; }"));

    const QFileInfo info(node_->path);
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
    const QRectF contentRect = previewRect().adjusted(2.0, 2.0, -18.0, -18.0);
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

void NodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (resizingPreview_) {
        resizingPreview_ = false;
        setFlag(QGraphicsItem::ItemIsMovable, true);
        resizePreview(node_->previewSize);
        event->accept();
        return;
    }

    QGraphicsItem::mouseReleaseEvent(event);
    if ((pos() - dragStart_).manhattanLength() < 16.0) {
        window_->clearInternalDropHover();
        window_->clearLinkDropHover();
        window_->clearDragPreview();
        window_->setDragVisuals(this, 1.0, 10.0);
        if (event->button() == Qt::LeftButton && scene()) {
            scene()->clearSelection();
            setSelected(true);
            event->accept();
            return;
        }
        return;
    }

    NodeItem* linkTarget = window_->linkTargetItemForDrop(this, event->scenePos());
    if (linkTarget) {
        window_->clearInternalDropHover();
        window_->clearLinkDropHover();
        window_->clearDragPreview();
        window_->addFileLink(linkTarget->node(), node_);
        return;
    }

    NodeItem* folderTarget = window_->folderItemForDrop(this);
    if (folderTarget) {
        window_->clearInternalDropHover();
        window_->clearLinkDropHover();
        window_->moveDragItemsToFolder(this, folderTarget->node());
        return;
    }

    if (window_->isMultiDragSelection(this)) {
        window_->clearInternalDropHover();
        window_->clearLinkDropHover();
        window_->clearDragPreview();
        window_->setDragVisuals(this, 1.0, 10.0);
        return;
    }

    window_->clearInternalDropHover();
    window_->clearLinkDropHover();
    if (window_->reorderNodeByY(node_, this, sceneBoundingRect().center().y())) {
        return;
    }
    window_->clearDragPreview();
    window_->setDragVisuals(this, 1.0, 10.0);
}

void NodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (resizingPreview_) {
        prepareGeometryChange();
        const QPointF delta = event->scenePos() - resizeStartScene_;
        node_->previewSize = QSizeF(std::clamp(resizeStartSize_.width() + delta.x(), 260.0, 900.0),
                                    std::clamp(resizeStartSize_.height() + delta.y(), 110.0, 520.0));
        syncPreviewWidgetGeometry();
        update();
        event->accept();
        return;
    }

    QGraphicsItem::mouseMoveEvent(event);
    if ((pos() - dragStart_).manhattanLength() >= 16.0) {
        const bool multiDrag = window_->isMultiDragSelection(this);
        if (multiDrag) {
            window_->setDragVisuals(this, 0.72, 100.0);
            window_->previewDragSelection(this);
        } else if (node_->isDir) {
            window_->previewMoveDescendants(node_, pos() - layoutCenter_);
        }
        if (window_->updateLinkDropHover(this, event->scenePos())) {
            window_->clearInternalDropHover();
            window_->clearDragPreviewForSource(this);
            return;
        }
        if (window_->updateInternalDropHover(this)) {
            window_->clearLinkDropHover();
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

void NodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    window_->cancelQueuedInlinePreviewToggle();
    QGraphicsItem::mouseDoubleClickEvent(event);
    if (node_->isDir) {
        window_->toggleCollapsed(node_);
    } else {
        window_->toggleInlinePreview(node_);
    }
}

}  // namespace

int main(int argc, char* argv[])
{
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
    app.setWindowIcon(QIcon(":/icons/mycel.png"));
    if (!resolveStartupStorageMode(options)) {
        return 0;
    }
    MainWindow window(options.rootPath, options.mycelStorageEnabled);
    window.show();
    return app.exec();
}
