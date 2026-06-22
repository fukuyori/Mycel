#include <QtCore/QDir>
#include <QtCore/QDateTime>
#include <QtCore/QEvent>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QIODevice>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMimeData>
#include <QtCore/QPointF>
#include <QtCore/QRectF>
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
#include <QtGui/QShortcut>
#include <QtGui/QTextDocument>
#include <QtGui/QTextOption>
#include <QtGui/QWheelEvent>
#include <QtGui/QAction>
#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QColorDialog>
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
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace {

constexpr int MaxDepth = 5;
constexpr int MaxChildren = 90;
constexpr qreal XStep = 260.0;
constexpr qreal ParentChildGap = 80.0;
constexpr qreal YStep = 72.0;
constexpr qreal FreeCanvasMargin = 12000.0;

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
    color.setAlpha(34);
    return color;
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
bool isImagePreviewFile(const QFileInfo& info);

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
        QStringLiteral("html"), QStringLiteral("sh")
    };
    return extensions.contains(info.suffix().toLower()) || info.fileName() == QStringLiteral("CMakeLists.txt");
}

bool isMarkdownPreviewFile(const QFileInfo& info)
{
    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown");
}

bool isImagePreviewFile(const QFileInfo& info)
{
    static const QSet<QString> extensions = {
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("bmp"), QStringLiteral("svg")
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

bool resolveStartupStorageMode(StartupOptions& options)
{
    if (!options.mycelStorageEnabled) {
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
        QPushButton* chooseButton = box.addButton(QStringLiteral("別フォルダを選択"), QMessageBox::ActionRole);
        QPushButton* quitButton = box.addButton(QStringLiteral("終了"), QMessageBox::RejectRole);
        box.setDefaultButton(createButton);
        box.setEscapeButton(quitButton);
        box.exec();

        if (box.clickedButton() == createButton) {
            if (createMycelFolder(options.rootPath)) {
                return true;
            }
            QMessageBox::warning(nullptr, QStringLiteral("Mycel"),
                                 QStringLiteral(".mycel フォルダを作成できませんでした。"));
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
        return true;
    }

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
    }

    Node* node() const { return node_; }
    QPointF layoutCenter() const { return layoutCenter_; }
    QRectF labelSceneRect() const
    {
        const QRectF box(-node_->size.width() / 2.0, -node_->size.height() / 2.0,
                         node_->size.width(), node_->size.height());
        const qreal leftPadding = node_->isDir ? 66.0 : 40.0;
        return mapRectToScene(QRectF(box.left() + leftPadding - 3.0, box.top() + 4.0,
                                     box.width() - leftPadding - 8.0, box.height() - 8.0));
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
        if (isSelected()) {
            painter->setPen(QPen(QColor("#0b63ce"), 2.0));
            painter->setBrush(QColor("#dbeafe"));
            painter->drawRoundedRect(box.adjusted(-4.0, -3.0, 4.0, 3.0), 8.0, 8.0);
        }

        if (node_->isDir && externalDropHover_) {
            painter->setPen(QPen(QColor("#1e8cff"), 3.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(box.adjusted(-5.0, -5.0, 5.0, 5.0), 15.0, 15.0);
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
            if ((event->modifiers() & Qt::ControlModifier) && !(event->modifiers() & Qt::ShiftModifier)) {
                event->accept();
                return;
            }
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
    QStringList windowInlinePreviewLines() const;
    QString windowMarkdownPreviewText() const;
    void resizePreview(const QSizeF& size);

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

    void drawDocument(QPainter* painter, QTextDocument& doc, const QRectF& rect)
    {
        painter->save();
        painter->translate(rect.left() + 12.0, rect.top() + 10.0);
        const QRectF clip(0.0, 0.0, rect.width() - 24.0, rect.height() - 30.0);
        painter->setClipRect(clip);
        QAbstractTextDocumentLayout::PaintContext context;
        context.clip = clip;
        context.palette.setColor(QPalette::Text, QColor("#243036"));
        context.palette.setColor(QPalette::WindowText, QColor("#243036"));
        context.palette.setColor(QPalette::Link, QColor("#1168b3"));
        doc.documentLayout()->draw(painter, context);
        painter->restore();
    }

    void paintInlinePreview(QPainter* painter, const QRectF& rect)
    {
        painter->setPen(QPen(QColor("#c4ccd1"), 1.1));
        painter->setBrush(QColor("#ffffff"));
        painter->drawRoundedRect(rect, 8.0, 8.0);

        const QFileInfo info(node_->path);
        if (isMarkdownPreviewFile(info)) {
            QTextDocument doc;
            QFont docFont = painter->font();
            docFont.setPointSize(10);
            doc.setDefaultFont(docFont);
            QTextOption option = doc.defaultTextOption();
            option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            doc.setDefaultTextOption(option);
            doc.setDefaultStyleSheet(QStringLiteral("* { color: #243036; } a { color: #1168b3; }"));
            const QString markdown = windowMarkdownPreviewText();
            doc.setMarkdown(markdown);
            if (doc.toPlainText().trimmed().isEmpty() && !markdown.trimmed().isEmpty()) {
                doc.setPlainText(markdown);
            }
            doc.setTextWidth(rect.width() - 24.0);

            drawDocument(painter, doc, rect);

            painter->setPen(QPen(QColor("#9aa5aa"), 1.2));
            const QPointF corner(rect.right() - 12.0, rect.bottom() - 4.0);
            painter->drawLine(corner, QPointF(rect.right() - 4.0, rect.bottom() - 12.0));
            painter->drawLine(QPointF(rect.right() - 8.0, rect.bottom() - 4.0),
                              QPointF(rect.right() - 4.0, rect.bottom() - 8.0));
            return;
        }

        QFont bodyFont = painter->font();
        bodyFont.setPointSize(9);
        bodyFont.setBold(false);
        bodyFont.setFamily(QStringLiteral("Menlo"));
        painter->setFont(bodyFont);
        painter->setPen(QColor("#243036"));

        QTextDocument doc;
        doc.setDefaultFont(bodyFont);
        QTextOption option = doc.defaultTextOption();
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        doc.setDefaultTextOption(option);
        doc.setPlainText(windowInlinePreviewLines().join(QLatin1Char('\n')));
        doc.setTextWidth(rect.width() - 24.0);

        drawDocument(painter, doc, rect);

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
    QPointF resizeStartScene_;
    QSizeF resizeStartSize_;
};

class MindMapScene final : public QGraphicsScene {
public:
    void setRoot(Node* root) { root_ = root; }
    void setUserColors(const std::map<QString, QColor>* userColors) { userColors_ = userColors; }

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
        }
    }

private:
    void drawEdges(QPainter* painter, const Node& node)
    {
        if (node.children.empty()) {
            return;
        }

        const QPointF start(node.center.x() + node.size.width() / 2.0, node.center.y());

        for (const auto& child : node.children) {
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
            QColor color = colorForPath(child->path);
            color.setAlpha(userColors_ && userColors_->count(child->path) ? 220 : 125);
            const qreal width = child->isDir ? (node.depth == 0 ? 2.6 : 2.1) : 1.1;
            painter->setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(path);
            drawEdges(painter, *child);
        }
    }

    QColor colorForPath(const QString& path) const
    {
        if (userColors_) {
            const auto found = userColors_->find(path);
            if (found != userColors_->end()) {
                return found->second;
            }
        }
        return neutralStroke();
    }

    Node* root_ = nullptr;
    const std::map<QString, QColor>* userColors_ = nullptr;
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

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        if (event->modifiers() & Qt::ControlModifier) {
            const QPoint delta = event->angleDelta();
            const QPoint pixelDelta = event->pixelDelta();
            const int amount = !delta.isNull() ? delta.y() : pixelDelta.y();
            if (amount != 0) {
                zoomAt(event->position(), amount > 0 ? 1.12 : 0.89);
            }
            event->accept();
            return;
        }
        QGraphicsView::wheelEvent(event);
    }

    bool event(QEvent* event) override
    {
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
            event->accept();
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
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
    }

    bool panning_ = false;
    bool rubberBandSelecting_ = false;
    Qt::MouseButton panningButton_ = Qt::NoButton;
    QPoint lastPanPoint_;
    QPoint rubberBandStart_;
    QRectF lastRubberBandSceneRect_;
    std::function<void()> cheatSheetHandler_;
};

class TextEditor final : public QPlainTextEdit {
public:
    explicit TextEditor(QWidget* parent = nullptr) : QPlainTextEdit(parent) {}

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        if (event->modifiers() & Qt::ControlModifier) {
            const QPoint delta = event->angleDelta();
            const QPoint pixelDelta = event->pixelDelta();
            const int amount = !delta.isNull() ? delta.y() : pixelDelta.y();
            if (amount > 0) {
                zoomIn(1);
            } else if (amount < 0) {
                zoomOut(1);
            }
            event->accept();
            return;
        }
        QPlainTextEdit::wheelEvent(event);
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
        QFont font(QStringLiteral("Consolas"));
        font.setStyleHint(QFont::Monospace);
        font.setPointSize(10);
        editor_->setFont(font);
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

        view_ = new BoardView(this);
        view_->setScene(&scene_);
        view_->setCheatSheetHandler([this] { showCheatSheet(); });
        setCentralWidget(view_);

        auto* toolbar = addToolBar(QStringLiteral("Mycel"));
        QAction* openAction = toolbar->addAction(QStringLiteral("Open"));
        QAction* refreshAction = toolbar->addAction(QStringLiteral("Refresh"));
        refreshAction->setShortcut(QKeySequence(Qt::Key_F5));
        QAction* fitAction = toolbar->addAction(QStringLiteral("Fit"));
        fitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
        QAction* openSelectedPreviewsAction = toolbar->addAction(QStringLiteral("Open Previews"));
        QAction* closeSelectedPreviewsAction = toolbar->addAction(QStringLiteral("Close Previews"));
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
                rootPath_ = normalizedDirectoryPath(dir);
                collapsedPaths_.clear();
                previewPaths_.clear();
                previewSizes_.clear();
                loadOrderFile();
                loadColorFile();
                loadPreviewFile();
                rebuild(true);
            }
        });
        connect(refreshAction, &QAction::triggered, this, [this] { refreshAll(); });
        connect(fitAction, &QAction::triggered, this, [this] { fitToMap(); });
        connect(openSelectedPreviewsAction, &QAction::triggered, this, [this] { setSelectedFilePreviews(true); });
        connect(closeSelectedPreviewsAction, &QAction::triggered, this, [this] { setSelectedFilePreviews(false); });
        connect(renameSelectedAction, &QAction::triggered, this, [this] { renameSelectedItem(); });
        connect(maximizeAction, &QAction::triggered, this, [this] {
            if (isMaximized()) {
                showNormal();
            } else {
                showMaximized();
            }
        });
        connect(quitAction, &QAction::triggered, this, &QWidget::close);

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
        rebuild(true);
    }

    bool mycelStorageEnabled() const
    {
        return mycelStorageEnabled_;
    }

    void createFolder(Node* parent)
    {
        if (!parent || !parent->isDir) {
            return;
        }

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
    }

    void createFile(Node* parent)
    {
        if (!parent || !parent->isDir) {
            return;
        }

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
    }

    void openNode(Node* node)
    {
        if (!node) {
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(node->path));
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
        for (int lineNo = 1; lineNo <= 7 && !stream.atEnd(); ++lineNo) {
            lines << stream.readLine();
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
        return QString::fromUtf8(data);
    }

    void toggleInlinePreview(Node* node)
    {
        if (!node || node->isDir) {
            return;
        }
        toggleInlinePreviewPath(node->path);
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
        rebuild(false);
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

    bool canDeleteFolder(Node* node) const
    {
        return node && node->isDir && node != root_.get();
    }

    void setNodeColor(Node* node)
    {
        if (!node || !mycelStorageEnabled_) {
            return;
        }
        const QColor current = colorForNode(node);
        const QColor selected = QColorDialog::getColor(current, this, QStringLiteral("色を選択"));
        if (!selected.isValid()) {
            return;
        }
        userColors_[node->path] = selected;
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
                "Ctrl + Q : アプリを終了\n"
                "範囲選択後 Enter : 選択範囲へズーム\n\n"
                "クリック操作\n"
                "Ctrl + ノード左クリック : 複数選択に追加/解除\n"
                "Shift + ファイルクリック : プレビューを開く/閉じる\n"
                "Shift + フォルダクリック : 折りたたみ/展開\n"
                "フォルダをダブルクリック : NewFile を作成\n"
                "Shift + フォルダをダブルクリック : NewFolder を作成\n"
                "ファイルをダブルクリック : OS の既定アプリで開く\n"
                "テキストファイルを右クリックして編集 : 内蔵エディタで開く\n"
                "右クリック : コンテキストメニュー\n\n"
                "移動/ズーム\n"
                "Alt + 左ドラッグ、中ボタンドラッグ、または空白で右ドラッグ : キャンバス移動\n"
                "Ctrl + マウスホイール : ズーム\n"
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

    NodeItem* folderItemForDrop(const NodeItem* source) const
    {
        if (!source) {
            return nullptr;
        }

        const QRectF dropRect = source->sceneBoundingRect().adjusted(-10.0, -10.0, 10.0, 10.0);
        NodeItem* best = nullptr;
        qreal bestOverlap = 0.0;
        for (QGraphicsItem* item : scene_.items(dropRect, Qt::IntersectsItemBoundingRect)) {
            auto* candidate = dynamic_cast<NodeItem*>(item);
            if (!candidate || candidate == source || !candidate->node()->isDir) {
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

    void clearDragPreview()
    {
        for (QGraphicsItem* item : scene_.items()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem) {
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
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem || nodeItem->node()->isDir) {
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
        rebuild(false);
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
            renamePathTo(path, name);
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

    void rebuild(bool fitAfterRebuild)
    {
        finishInlineRename(false);
        setWindowTitle(mycelStorageEnabled_
                           ? QStringLiteral("Mycel - %1").arg(rootPath_)
                           : QStringLiteral("Mycel - %1 [--no-mycel]").arg(rootPath_));

        const QTransform previousTransform = view_->transform();
        const int previousHScroll = view_->horizontalScrollBar()->value();
        const int previousVScroll = view_->verticalScrollBar()->value();

        scene_.clear();
        root_ = scanTree(rootPath_, 0, -1, collapsedPaths_, previewPaths_, previewSizes_, fileOrders_, rootPath_);
        assignTopLevelBranches(*root_);

        qreal yCursor = 0.0;
        layoutTree(*root_, 0.0, yCursor);
        translateTree(*root_, QPointF(140.0, 120.0 - root_->center.y()));

        scene_.setRoot(root_.get());
        scene_.setUserColors(&userColors_);
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
            scheduleFitToMap();
        } else {
            view_->setTransform(previousTransform);
            view_->horizontalScrollBar()->setValue(previousHScroll);
            view_->verticalScrollBar()->setValue(previousVScroll);
        }
    }

    void scheduleFitToMap()
    {
        QTimer::singleShot(0, this, [this] { fitToMap(); });
    }

    void fitToMap()
    {
        if (scene_.items().isEmpty()) {
            return;
        }
        view_->resetTransform();
        view_->fitInView(scene_.itemsBoundingRect().adjusted(-160.0, -160.0, 260.0, 160.0), Qt::KeepAspectRatio);
        view_->scale(0.96, 0.96);
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
    BoardView* view_ = nullptr;
    std::unique_ptr<Node> root_;
};

void NodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    QMenu menu;
    const bool multiItemSelection = isSelected() && window_->selectedDeletableItemCount() > 1;

    if (multiItemSelection) {
        QAction* refreshSelectionAction = menu.addAction(QStringLiteral("選択範囲を更新"));
        QAction* refreshAllAction = menu.addAction(QStringLiteral("全体を更新"));
        menu.addSeparator();
        QAction* openSelectionAction = menu.addAction(QStringLiteral("選択ファイルのプレビューを開く"));
        QAction* closeSelectionAction = menu.addAction(QStringLiteral("選択ファイルのプレビューを閉じる"));
        openSelectionAction->setEnabled(window_->hasSelectedFiles());
        closeSelectionAction->setEnabled(window_->hasSelectedFiles());
        QAction* deleteSelectionAction = menu.addAction(QStringLiteral("選択項目を削除"));

        QAction* selected = menu.exec(event->screenPos());
        if (selected == refreshSelectionAction) {
            window_->refreshSelectedItems();
        } else if (selected == refreshAllAction) {
            window_->refreshAll();
        } else if (selected == openSelectionAction) {
            window_->setSelectedFilePreviews(true);
        } else if (selected == closeSelectionAction) {
            window_->setSelectedFilePreviews(false);
        } else if (selected == deleteSelectionAction) {
            window_->deleteSelectedItems();
        }
        return;
    }

    QAction* refreshNodeAction = menu.addAction(QStringLiteral("この項目を更新"));
    QAction* refreshAllAction = menu.addAction(QStringLiteral("全体を更新"));
    menu.addSeparator();

    if (node_->isDir) {
        QAction* collapseAction = menu.addAction(node_->collapsed ? QStringLiteral("展開") : QStringLiteral("折りたたむ"));
        menu.addSeparator();
        QAction* folderAction = menu.addAction(QStringLiteral("フォルダを作成"));
        QAction* fileAction = menu.addAction(QStringLiteral("ファイルを作成"));
        QAction* pasteAction = menu.addAction(QStringLiteral("貼り付け"));
        pasteAction->setEnabled(window_->canPasteClipboardToFolder(node_));
        QAction* deleteAction = menu.addAction(QStringLiteral("削除"));
        deleteAction->setEnabled(window_->canDeleteFolder(node_));
        QAction* colorAction = nullptr;
        QAction* clearColorAction = nullptr;
        if (window_->mycelStorageEnabled()) {
            menu.addSeparator();
            colorAction = menu.addAction(QStringLiteral("色を設定"));
            clearColorAction = menu.addAction(QStringLiteral("色をクリア"));
            clearColorAction->setEnabled(window_->hasUserColor(node_));
        }
        menu.addSeparator();
        QAction* openAction = menu.addAction(QStringLiteral("開く"));
        QAction* selected = menu.exec(event->screenPos());
        if (selected == refreshNodeAction) {
            window_->refreshNode(node_);
        } else if (selected == refreshAllAction) {
            window_->refreshAll();
        } else if (selected == collapseAction) {
            window_->toggleCollapsed(node_);
        } else if (selected == folderAction) {
            window_->createFolder(node_);
        } else if (selected == fileAction) {
            window_->createFile(node_);
        } else if (selected == pasteAction) {
            window_->pasteClipboardToFolder(node_);
        } else if (selected == deleteAction) {
            window_->deleteFolder(node_);
        } else if (colorAction && selected == colorAction) {
            window_->setNodeColor(node_);
        } else if (clearColorAction && selected == clearColorAction) {
            window_->clearNodeColor(node_);
        } else if (selected == openAction) {
            window_->openNode(node_);
        }
    } else {
        QAction* editAction = menu.addAction(QStringLiteral("編集"));
        editAction->setEnabled(window_->canEditTextFile(node_));
        QAction* renameAction = menu.addAction(QStringLiteral("名前を変更"));
        QAction* deleteAction = menu.addAction(QStringLiteral("削除"));
        QAction* colorAction = nullptr;
        QAction* clearColorAction = nullptr;
        if (window_->mycelStorageEnabled()) {
            menu.addSeparator();
            colorAction = menu.addAction(QStringLiteral("色を設定"));
            clearColorAction = menu.addAction(QStringLiteral("色をクリア"));
            clearColorAction->setEnabled(window_->hasUserColor(node_));
        }
        menu.addSeparator();
        QAction* openAction = menu.addAction(QStringLiteral("開く"));
        QAction* selected = menu.exec(event->screenPos());
        if (selected == refreshNodeAction) {
            window_->refreshNode(node_);
        } else if (selected == refreshAllAction) {
            window_->refreshAll();
        } else if (selected == editAction) {
            window_->editTextFile(node_);
        } else if (selected == renameAction) {
            window_->renameFile(node_);
        } else if (selected == deleteAction) {
            window_->deleteFile(node_);
        } else if (colorAction && selected == colorAction) {
            window_->setNodeColor(node_);
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
        window_->clearDragPreview();
        setOpacity(1.0);
        setZValue(10.0);
        if ((event->modifiers() & Qt::ControlModifier) && !(event->modifiers() & Qt::ShiftModifier)) {
            setSelected(!isSelected());
            event->accept();
            return;
        }
        if (event->modifiers() & Qt::ShiftModifier) {
            if (node_->isDir) {
                window_->queueCollapsedToggle(node_);
            } else {
                window_->queueInlinePreviewToggle(node_);
            }
        }
        return;
    }

    if (window_->reorderNodeByY(node_, this, sceneBoundingRect().center().y())) {
        return;
    }

    NodeItem* folderTarget = window_->folderItemForDrop(this);
    if (folderTarget) {
        window_->moveNode(node_, folderTarget->node());
        return;
    }
    window_->clearDragPreview();
    setOpacity(1.0);
    setZValue(10.0);
}

void NodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (resizingPreview_) {
        prepareGeometryChange();
        const QPointF delta = event->scenePos() - resizeStartScene_;
        node_->previewSize = QSizeF(std::clamp(resizeStartSize_.width() + delta.x(), 260.0, 900.0),
                                    std::clamp(resizeStartSize_.height() + delta.y(), 110.0, 520.0));
        update();
        event->accept();
        return;
    }

    QGraphicsItem::mouseMoveEvent(event);
    if ((pos() - dragStart_).manhattanLength() >= 16.0) {
        if (node_->isDir) {
            window_->previewMoveDescendants(node_, pos() - layoutCenter_);
        }
        window_->previewReorder(node_, this, sceneBoundingRect().center().y());
    }
}

void NodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    window_->cancelQueuedInlinePreviewToggle();
    QGraphicsItem::mouseDoubleClickEvent(event);
    if (node_->isDir) {
        if (event->modifiers() & Qt::ShiftModifier) {
            window_->createFolder(node_);
        } else {
            window_->createFile(node_);
        }
    } else {
        window_->openNode(node_);
    }
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/mycel.png"));
    StartupOptions options = startupOptions(app.arguments());
    if (!resolveStartupStorageMode(options)) {
        return 0;
    }
    MainWindow window(options.rootPath, options.mycelStorageEnabled);
    window.show();
    return app.exec();
}
