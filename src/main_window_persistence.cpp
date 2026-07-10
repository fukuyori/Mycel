#include "main_window.h"

QJsonObject MainWindow::readJsonObjectFile(const QString& path)
{
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QJsonDocument::fromJson(file.readAll()).object();
    }

void MainWindow::writeJsonObjectFile(const QString& path, const QJsonObject& object)
{
        writeJsonFileAtomic(path, object);
    }

void MainWindow::mergeChildObjectSection(const QString& parentPath, const QString& childPath, const QString& section,
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

void MainWindow::mergeChildStringArray(const QString& parentPath, const QString& childPath, const QString& section,
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

void MainWindow::mergeChildLinks(const QString& parentPath, const QString& childPath,
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

MainWindow::MetadataSnapshot MainWindow::captureMetadataSnapshot() const
{
        return MetadataSnapshot{collapsedPaths_, userColors_,  previewPaths_, previewSizes_,
                                previewImageScales_, fileOrders_, fileLinks_, externalRootLinks_,
                                boardPatternName_, boardCards_};
    }

void MainWindow::restoreMetadataSnapshot(const MetadataSnapshot& s)
{
        collapsedPaths_ = s.collapsedPaths;
        userColors_ = s.userColors;
        previewPaths_ = s.previewPaths;
        previewSizes_ = s.previewSizes;
        previewImageScales_ = s.previewImageScales;
        fileOrders_ = s.fileOrders;
        fileLinks_ = s.fileLinks;
        externalRootLinks_ = s.externalRootLinks;
        if (!s.boardPatternName.isEmpty()) {
            if (s.boardPatternName == boardPatternName_) {
                boardCards_ = s.boardCards;
                saveBoardPattern();
            } else {
                // The active pattern changed since this entry: restore into that pattern's file.
                restoreBoardCardsToPatternFile(s.boardPatternName, s.boardCards);
            }
        }
    }

void MainWindow::restoreBoardCardsToPatternFile(const QString& name, const std::map<QString, BoardCardState>& cards)
{
        if (!mycelStorageEnabled_ || name.isEmpty()) {
            return;
        }
        QJsonObject rootObject = readJsonObjectFile(boardPatternFilePath(name));
        QJsonObject cardsObject;
        for (const auto& [key, state] : cards) {
            QJsonObject card;
            card.insert(QStringLiteral("x"), state.pos.x());
            card.insert(QStringLiteral("y"), state.pos.y());
            card.insert(QStringLiteral("visible"), state.visible);
            cardsObject.insert(key, card);
        }
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("cards"), cardsObject);
        writeJsonFileAtomic(boardPatternFilePath(name), rootObject);
    }

void MainWindow::saveAllMetadata()
{
        saveColorFile();
        saveOrderFile();
        savePreviewFile();
        saveLinkFile();
        saveExternalRootsFile();
        saveCollapsedFile();
        saveHashCacheFile();
    }

QString MainWindow::orderKeyForDirectory(const QString& directoryPath) const
{
        const QString relative = QDir(rootPath_).relativeFilePath(directoryPath);
        return relative == QStringLiteral(".") ? QString() : relative;
    }

QString MainWindow::orderFilePath() const
{
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/order.json"));
    }

QString MainWindow::colorFilePath() const
{
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/colors.json"));
    }

QString MainWindow::previewFilePath() const
{
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/previews.json"));
    }

QString MainWindow::linkFilePath() const
{
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/links.json"));
    }

QString MainWindow::externalRootsFilePath() const
{
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/external-roots.json"));
    }

QString MainWindow::collapsedFilePath() const
{
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/collapsed.json"));
    }

QString MainWindow::viewStateFilePath() const
{
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/view.json"));
    }

QString MainWindow::hashCacheFilePath() const
{
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/hashcache.json"));
    }

QString MainWindow::relativeKeyForPath(const QString& path) const
{
        return QDir(rootPath_).relativeFilePath(path);
    }

QString MainWindow::absolutePathForKey(const QString& key) const
{
        return QDir(rootPath_).absoluteFilePath(key);
    }

void MainWindow::loadOrderFile()
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

void MainWindow::saveOrderFile()
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

void MainWindow::loadColorFile()
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

void MainWindow::saveColorFile()
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

void MainWindow::loadHashCacheFile()
{
        fileHashCache_.clear();
        if (!mycelStorageEnabled_) {
            return;
        }

        QFile file(hashCacheFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonObject files = doc.object().value(QStringLiteral("files")).toObject();
        for (auto it = files.begin(); it != files.end(); ++it) {
            const QJsonObject entry = it.value().toObject();
            FileHashCacheEntry cacheEntry;
            cacheEntry.size = static_cast<qint64>(entry.value(QStringLiteral("size")).toDouble());
            cacheEntry.mtimeMs = static_cast<qint64>(entry.value(QStringLiteral("mtime")).toDouble());
            cacheEntry.hash = QByteArray::fromHex(entry.value(QStringLiteral("hash")).toString().toLatin1());
            if (!cacheEntry.hash.isEmpty()) {
                fileHashCache_[it.key()] = cacheEntry;
            }
        }
    }

void MainWindow::saveHashCacheFile()
{
        if (!mycelStorageEnabled_) {
            return;
        }

        QDir root(rootPath_);
        if (!root.mkpath(QStringLiteral(".mycel"))) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral(".mycel フォルダを作成できませんでした。"));
            return;
        }

        QJsonObject files;
        for (const auto& [key, entry] : fileHashCache_) {
            QJsonObject entryObject;
            entryObject.insert(QStringLiteral("size"), static_cast<double>(entry.size));
            entryObject.insert(QStringLiteral("mtime"), static_cast<double>(entry.mtimeMs));
            entryObject.insert(QStringLiteral("hash"), QString::fromLatin1(entry.hash.toHex()));
            files.insert(key, entryObject);
        }

        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("files"), files);

        if (!writeJsonFileAtomic(hashCacheFilePath(), rootObject)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("ハッシュキャッシュを保存できませんでした。"));
        }
    }

void MainWindow::loadPreviewFile()
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

void MainWindow::savePreviewFile()
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

bool MainWindow::loadCollapsedFile()
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

void MainWindow::saveCollapsedFile()
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

void MainWindow::loadLinkFile()
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
        // A target sits beside exactly one source (see addFileLink()); links.json can end up
        // with two sources claiming the same target (e.g. an external rename collision reconciled
        // by rekeyPathMetadataAfterRename before this file was last saved), which would otherwise
        // make layoutFileLinks() silently reposition that target once per source.
        dedupeFileLinksKeepingLastPerTarget(fileLinks_);
    }

void MainWindow::saveLinkFile()
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

void MainWindow::loadExternalRootsFile()
{
        externalRootLinks_.clear();
        if (!mycelStorageEnabled_) {
            return;
        }

        QFile file(externalRootsFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonArray links = doc.object().value(QStringLiteral("links")).toArray();
        QSet<QString> seen;
        for (const QJsonValue& value : links) {
            const QJsonObject link = value.toObject();
            const QString dirKey = link.value(QStringLiteral("dir")).toString();
            const QString target = link.value(QStringLiteral("target")).toString();
            if (target.isEmpty()) {
                continue;
            }
            const QString key = dirKey + QLatin1Char('\n') + target;
            if (seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            externalRootLinks_.push_back({dirKey, target});
        }
    }

void MainWindow::saveExternalRootsFile()
{
        if (!mycelStorageEnabled_) {
            return;
        }
        // Unlike the always-written metadata files, this one only exists once a link is made.
        if (externalRootLinks_.empty() && !QFileInfo::exists(externalRootsFilePath())) {
            return;
        }

        QDir root(rootPath_);
        if (!root.mkpath(QStringLiteral(".mycel"))) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral(".mycel フォルダを作成できませんでした。"));
            return;
        }

        QJsonArray links;
        for (const ExternalRootLink& link : externalRootLinks_) {
            if (link.target.isEmpty()) {
                continue;
            }
            QJsonObject object;
            object.insert(QStringLiteral("dir"), link.dirKey);
            object.insert(QStringLiteral("target"), link.target);
            links.append(object);
        }

        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("links"), links);

        if (!writeJsonFileAtomic(externalRootsFilePath(), rootObject)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"),
                                 QStringLiteral("外部ルートのリンク設定を保存できませんでした。"));
        }
    }

QJsonObject MainWindow::currentWindowStateObject() const
{
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
        return windowObject;
    }

QJsonObject MainWindow::currentViewStateObject() const
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

        const QJsonObject windowObject = currentWindowStateObject();

        QJsonObject rootObject;
        rootObject.insert(QStringLiteral("version"), 1);
        rootObject.insert(QStringLiteral("transform"), transformObject);
        rootObject.insert(QStringLiteral("scroll"), scrollObject);
        rootObject.insert(QStringLiteral("center"), centerObject);
        rootObject.insert(QStringLiteral("window"), windowObject);
        return rootObject;
    }

std::optional<QJsonObject> MainWindow::loadViewStateObject() const
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

bool MainWindow::readViewStateObject(const QJsonObject& rootObject, QTransform& transform, int& horizontal, int& vertical,
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

bool MainWindow::loadViewState(QTransform& transform, int& horizontal, int& vertical,
                       QPointF& center, bool& hasCenter) const
{
        const std::optional<QJsonObject> rootObject = loadViewStateObject();
        if (!rootObject) {
            return false;
        }

        return readViewStateObject(*rootObject, transform, horizontal, vertical, center, hasCenter);
    }

void MainWindow::saveViewState()
{
        if (!view_ || !mycelStorageEnabled_) {
            return;
        }
        viewStateSaveTimer_.stop();

        QJsonObject rootObject;
        if (boardMode_) {
            // Board mode: the canvas belongs to the pattern (saved there); keep the tree's
            // stored transform/centre untouched and refresh only window + board flags.
            rootObject = loadViewStateObject().value_or(QJsonObject());
            rootObject.insert(QStringLiteral("version"), 1);
            rootObject.insert(QStringLiteral("window"), currentWindowStateObject());
            saveBoardPattern();
        } else {
            rootObject = currentViewStateObject();
        }
        QJsonObject boardObject;
        boardObject.insert(QStringLiteral("active"), boardMode_);
        boardObject.insert(QStringLiteral("pattern"), boardPatternName_);
        rootObject.insert(QStringLiteral("board"), boardObject);

        QDir root(rootPath_);
        if (!root.mkpath(QStringLiteral(".mycel"))) {
            return;
        }

        writeJsonFileAtomic(viewStateFilePath(), rootObject);
    }

void MainWindow::restoreWindowStateFromSettingsFile()
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

void MainWindow::scheduleViewStateSave()
{
        if (inlineRenameActivitySuspended_ || restoringViewState_ || pendingViewRestoreReapplies_ > 0 ||
            !mycelStorageEnabled_) {
            return;  // ignore scroll churn while the restored view is still settling
        }
        viewStateSaveTimer_.start();
    }

