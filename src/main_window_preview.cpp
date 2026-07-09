#include "main_window.h"

bool MainWindow::focusEditorForSelectedFile()
{
        Node* node = singleSelectedNode();
        if (!node) {
            return false;
        }
        return focusEditorForNode(node);
    }

bool MainWindow::focusEditorForNode(Node* node)
{
        if (!canEditTextFile(node)) {
            return false;
        }
        return focusEditorForPath(node->path);
    }

bool MainWindow::focusEditorForPath(const QString& path)
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

void MainWindow::focusBoard()
{
        if (view_) {
            view_->setFocus(Qt::MouseFocusReason);
        }
    }

void MainWindow::saveSideEditorFromShortcut(bool returnFocusToFile)
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

QString MainWindow::subjectFileNameFromEditor() const
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

QString MainWindow::editorTextWithoutSubjectLine(const QString& text) const
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

bool MainWindow::toggleSelectedFilePreviews()
{
        const QStringList paths = selectedFilePaths();
        if (paths.isEmpty()) {
            return false;
        }
        toggleFilePreviewsForPaths(paths);
        view_->setFocus(Qt::ShortcutFocusReason);
        return true;
    }

bool MainWindow::toggleSelectedFoldersCollapsed()
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

bool MainWindow::toggleSelectedPreviewOrCollapse()
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

void MainWindow::updateSideEditorForSelection()
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

void MainWindow::syncEditorPaneVisibility()
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

void MainWindow::clearSideEditor(const QString& message, const QString& status)
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

void MainWindow::loadSideEditorFile(const QString& path)
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

bool MainWindow::showUrlShortcutSidePreview(const QFileInfo& info)
{
        const auto url = urlForShortcutFile(info);
        if (!url) {
            return false;
        }

        QString cachePath;
        if (const auto embedUrl = youtubeEmbedUrlForFile(info)) {
            const QString youtubeCachePath = youtubeThumbnailCachePathForEmbedUrl(*embedUrl);
            if (!youtubeCachePath.isEmpty() && QFileInfo::exists(youtubeCachePath)) {
                cachePath = youtubeCachePath;
            }
        }
        if (cachePath.isEmpty()) {
            const QString urlCachePath = urlThumbnailCachePathForUrl(*url);
            if (!urlCachePath.isEmpty() && QFileInfo::exists(urlCachePath)) {
                cachePath = urlCachePath;
            }
        }

        if (!cachePath.isEmpty()) {
            QPixmap pixmap(cachePath);
            if (!pixmap.isNull()) {
                if (auto* imagePreview = dynamic_cast<AspectImagePreview*>(sidePreviewImage_)) {
                    imagePreview->setPreviewPixmap(pixmap);
                } else {
                    sidePreviewImage_->setPixmap(pixmap);
                }
                sidePreviewStack_->setCurrentWidget(sidePreviewImage_);
                setSidePaneMode(false, QStringLiteral("URL プレビュー"));
                sideEditorStatusLabel_->setText(QStringLiteral("URL プレビュー"));
                return true;
            }
        }

        const bool started = fetchUrlThumbnailForPreview(info.absoluteFilePath(), *url, false);
        showSidePreviewText(started ? QStringLiteral("サムネイルを取得中です。\n\n%1").arg(url->toString())
                                    : QStringLiteral("URLプレビュー画像を取得できませんでした。\n\n%1").arg(url->toString()),
                            QStringLiteral("URL プレビュー"));
        return true;
    }

void MainWindow::loadSidePreviewFile(const QString& path)
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
        if (showUrlShortcutSidePreview(info)) {
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
QWebEngineView* MainWindow::ensureHtmlPreviewView()
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

bool MainWindow::isDocumentThumbnailFile(const QFileInfo& info) const
{
#if MYCEL_HAS_PDF
        if (isPdfThumbnailFile(info)) {
            return true;
        }
#endif
        return isEpubThumbnailFile(info);
    }

QImage MainWindow::renderDocumentFirstPage(const QFileInfo& info) const
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

QString MainWindow::thumbnailCachePathFor(const QFileInfo& info) const
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

QString MainWindow::documentThumbnailMemKey(const QFileInfo& info) const
{
        return QStringLiteral("mycel:thumb:%1:%2:%3:%4")
            .arg(info.absoluteFilePath())
            .arg(info.lastModified().toMSecsSinceEpoch())
            .arg(info.size())
            .arg(kThumbnailVersion);
    }

QPixmap MainWindow::cachedDocumentThumbnail(const QFileInfo& info)
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

QPixmap MainWindow::documentThumbnail(const QFileInfo& info)
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

void MainWindow::showSidePreviewText(const QString& text, const QString& status)
{
        stopSidePreviewMedia();
        sidePreviewText_->clear();
        sidePreviewText_->setPlainText(text);
        sidePreviewStack_->setCurrentWidget(sidePreviewText_);
        setSidePaneMode(false, status);
        sideEditorStatusLabel_->setText(status);
    }

void MainWindow::applyTheme(AppTheme theme, bool persist, bool refreshTree)
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

void MainWindow::updateThemeActions()
{
        if (lightThemeAction_) {
            lightThemeAction_->setChecked(uiTheme_ == AppTheme::Light);
        }
        if (darkThemeAction_) {
            darkThemeAction_->setChecked(uiTheme_ == AppTheme::Dark);
        }
    }

void MainWindow::applyTextPaneTheme()
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

void MainWindow::applyRenameEditTheme(QLineEdit* edit)
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

void MainWindow::setSidePaneMode(bool editing, const QString& detail)
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

void MainWindow::stopSidePreviewMedia()
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

bool MainWindow::saveSideEditorNow()
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

void MainWindow::refreshInlinePreviewForPath(const QString& path)
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

QStringList MainWindow::inlinePreviewLines(Node* node) const
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

bool MainWindow::hasSavedPreviewSizePath(const QString& path) const
{
        return previewSizes_.find(path) != previewSizes_.end() ||
               previewImageScales_.find(path) != previewImageScales_.end();
    }

void MainWindow::resetPreviewSizePath(const QString& path)
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

QString MainWindow::inlineMarkdownPreviewText(Node* node) const
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

void MainWindow::toggleInlinePreview(Node* node)
{
        if (!node || node->isDir) {
            return;
        }
        toggleInlinePreviewPath(node->path);
    }

bool MainWindow::isInlinePreviewOpen(Node* node) const
{
        return node && !node->isDir && previewPaths_.contains(node->path);
    }

void MainWindow::setInlinePreview(Node* node, bool open)
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

void MainWindow::queueInlinePreviewToggle(Node* node)
{
        if (!node || node->isDir) {
            return;
        }
        queuedCollapsePath_.clear();
        queuedPreviewPath_ = node->path;
        previewClickTimer_.start(QApplication::doubleClickInterval() + 40);
    }

void MainWindow::queueCollapsedToggle(Node* node)
{
        if (!node || !node->isDir) {
            return;
        }
        queuedPreviewPath_.clear();
        queuedCollapsePath_ = node->path;
        previewClickTimer_.start(QApplication::doubleClickInterval() + 40);
    }

void MainWindow::cancelQueuedInlinePreviewToggle()
{
        previewClickTimer_.stop();
        queuedPreviewPath_.clear();
        queuedCollapsePath_.clear();
    }

void MainWindow::toggleInlinePreviewPath(const QString& path)
{
        if (previewPaths_.contains(path)) {
            closeInlinePreviewPath(path);
            return;
        }
        openInlinePreviewPath(path);
    }

void MainWindow::setPreviewSize(Node* node, const QSizeF& size, bool preferHeight)
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

void MainWindow::setImagePreviewScale(Node* node, qreal scale)
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

void MainWindow::toggleCollapsed(Node* node)
{
        if (!node || !node->isDir) {
            return;
        }
        toggleCollapsedPath(node->path);
    }

void MainWindow::toggleCollapsedPath(const QString& path)
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

void MainWindow::setSelectedFilePreviews(bool open)
{
        setFilePreviewsForPaths(selectedNodePaths(), open);
    }

QStringList MainWindow::selectedFilePaths() const
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

bool MainWindow::toggleFilePreviewsForPaths(const QStringList& paths)
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

void MainWindow::setFilePreviewsForPaths(const QStringList& paths, bool open)
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

bool MainWindow::openInlinePreviewPath(const QString& path)
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

bool MainWindow::closeInlinePreviewPath(const QString& path)
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

bool MainWindow::prepareInlinePreviewOpenPath(const QString& path)
{
        const QFileInfo info(path);
        const auto embedUrl = youtubeEmbedUrlForFile(info);
        if (embedUrl) {
            const QString cachePath = youtubeThumbnailCachePathForEmbedUrl(*embedUrl);
            if (!cachePath.isEmpty() && QFileInfo::exists(cachePath)) {
                return true;
            }

            fetchYouTubeThumbnailForInlinePreview(path, *embedUrl);
            return false;
        }

        const auto shortcutUrl = urlForShortcutFile(info);
        if (!shortcutUrl) {
            return true;
        }

        const QString urlCachePath = urlThumbnailCachePathForUrl(*shortcutUrl);
        if (!urlCachePath.isEmpty() && QFileInfo::exists(urlCachePath)) {
            return true;
        }

        fetchUrlThumbnailForPreview(path, *shortcutUrl, true);
        return false;
    }

QString MainWindow::youtubeThumbnailCacheDirectoryPath() const
{
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/youtube-thumbnails"));
    }

QString MainWindow::youtubeThumbnailCachePathForEmbedUrl(const QString& embedUrl) const
{
        const QString videoId = youtubeVideoIdFromEmbedUrl(embedUrl);
        if (videoId.isEmpty()) {
            return {};
        }
        return QDir(youtubeThumbnailCacheDirectoryPath()).filePath(videoId + QStringLiteral(".jpg"));
    }

QString MainWindow::urlThumbnailCacheDirectoryPath() const
{
        return QDir(rootPath_).filePath(QStringLiteral(".mycel/url-thumbnails"));
    }

QString MainWindow::urlThumbnailCachePathForUrl(const QUrl& url) const
{
        if (!url.isValid() || url.scheme().isEmpty()) {
            return {};
        }
        const QString key = url.toString();
        const QString hash = QString::fromLatin1(
            QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex());
        return QDir(urlThumbnailCacheDirectoryPath()).filePath(hash + QStringLiteral(".png"));
    }

void MainWindow::fetchYouTubeThumbnailForInlinePreview(const QString& path, const QString& embedUrl)
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

bool MainWindow::fetchUrlThumbnailForPreview(const QString& path, const QUrl& pageUrl, bool openInlinePreview)
{
        if (!mycelStorageEnabled_) {
            recordDebugEvent(QStringLiteral("url thumbnail skipped: .mycel disabled"));
            return false;
        }

        const QString cachePath = urlThumbnailCachePathForUrl(pageUrl);
        if (cachePath.isEmpty()) {
            recordDebugEvent(QStringLiteral("url thumbnail skipped: invalid url"));
            return false;
        }

        if (openInlinePreview) {
            pendingUrlThumbnailInlineOpenPaths_.insert(path);
        }
        if (pendingUrlThumbnailPaths_.contains(path)) {
            return true;
        }

        QDir dir;
        if (!dir.mkpath(urlThumbnailCacheDirectoryPath())) {
            recordDebugEvent(QStringLiteral("url thumbnail cache mkdir failed"));
            return false;
        }

        pendingUrlThumbnailPaths_.insert(path);
        recordDebugEvent(QStringLiteral("url thumbnail page fetch: %1").arg(relativeKeyForPath(path)));

        auto* pageManager = new QNetworkAccessManager(this);
        QNetworkRequest pageRequest{pageUrl};
        pageRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        pageRequest.setHeader(QNetworkRequest::UserAgentHeader,
                              QStringLiteral("Mozilla/5.0 Mycel/%1").arg(QString::fromLatin1(MYCEL_VERSION)));
        QNetworkReply* pageReply = pageManager->get(pageRequest);
        connect(pageReply, &QNetworkReply::finished, this, [this, pageManager, pageReply, path, pageUrl, cachePath] {
            const QNetworkReply::NetworkError pageError = pageReply->error();
            const QByteArray pageData = pageReply->readAll().left(512 * 1024);
            pageReply->deleteLater();
            pageManager->deleteLater();

            if (pageError != QNetworkReply::NoError) {
                pendingUrlThumbnailPaths_.remove(path);
                pendingUrlThumbnailInlineOpenPaths_.remove(path);
                recordDebugEvent(QStringLiteral("url thumbnail page fetch failed: %1").arg(relativeKeyForPath(path)));
                if (sideEditorPath_ == path && !sideEditorEditing_) {
                    showSidePreviewText(QStringLiteral("URLプレビュー画像を取得できませんでした。\n\n%1")
                                            .arg(pageUrl.toString()),
                                        QStringLiteral("URL プレビュー"));
                }
                return;
            }

            const auto imageUrl = firstImageUrlFromHtml(QString::fromUtf8(pageData), pageUrl);
            if (!imageUrl) {
                pendingUrlThumbnailPaths_.remove(path);
                pendingUrlThumbnailInlineOpenPaths_.remove(path);
                recordDebugEvent(QStringLiteral("url thumbnail image not found: %1").arg(relativeKeyForPath(path)));
                if (sideEditorPath_ == path && !sideEditorEditing_) {
                    showSidePreviewText(QStringLiteral("URLプレビュー画像が見つかりませんでした。\n\n%1")
                                            .arg(pageUrl.toString()),
                                        QStringLiteral("URL プレビュー"));
                }
                return;
            }

            auto* imageManager = new QNetworkAccessManager(this);
            QNetworkRequest imageRequest{*imageUrl};
            imageRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
            imageRequest.setHeader(QNetworkRequest::UserAgentHeader,
                                   QStringLiteral("Mozilla/5.0 Mycel/%1").arg(QString::fromLatin1(MYCEL_VERSION)));
            QNetworkReply* imageReply = imageManager->get(imageRequest);
            connect(imageReply, &QNetworkReply::finished, this,
                    [this, imageManager, imageReply, path, pageUrl, cachePath] {
                        const QNetworkReply::NetworkError imageError = imageReply->error();
                        const QByteArray imageData = imageReply->readAll();
                        imageReply->deleteLater();
                        imageManager->deleteLater();
                        pendingUrlThumbnailPaths_.remove(path);

                        QPixmap pixmap;
                        if (imageError != QNetworkReply::NoError || !pixmap.loadFromData(imageData)) {
                            pendingUrlThumbnailInlineOpenPaths_.remove(path);
                            recordDebugEvent(QStringLiteral("url thumbnail image fetch failed: %1")
                                                 .arg(relativeKeyForPath(path)));
                            if (sideEditorPath_ == path && !sideEditorEditing_) {
                                showSidePreviewText(QStringLiteral("URLプレビュー画像を取得できませんでした。\n\n%1")
                                                        .arg(pageUrl.toString()),
                                                    QStringLiteral("URL プレビュー"));
                            }
                            return;
                        }

                        if (!pixmap.save(cachePath, "PNG")) {
                            pendingUrlThumbnailInlineOpenPaths_.remove(path);
                            recordDebugEvent(QStringLiteral("url thumbnail cache write failed"));
                            return;
                        }

                        if (sideEditorPath_ == path && !sideEditorEditing_) {
                            if (auto* imagePreview = dynamic_cast<AspectImagePreview*>(sidePreviewImage_)) {
                                imagePreview->setPreviewPixmap(pixmap);
                            } else {
                                sidePreviewImage_->setPixmap(pixmap);
                            }
                            sidePreviewStack_->setCurrentWidget(sidePreviewImage_);
                            setSidePaneMode(false, QStringLiteral("URL プレビュー"));
                            sideEditorStatusLabel_->setText(QStringLiteral("URL プレビュー"));
                        }

                        const bool openInline = pendingUrlThumbnailInlineOpenPaths_.contains(path);
                        pendingUrlThumbnailInlineOpenPaths_.remove(path);
                        if (openInline && QFileInfo::exists(path)) {
                            previewPaths_.insert(path);
                            savePreviewFile();
                            relayout();
                            selectNodePath(path);
                        }
                        recordDebugEvent(QStringLiteral("url thumbnail cached: %1").arg(relativeKeyForPath(path)));
                    });
        });
        return true;
    }

QString MainWindow::cachedYouTubeThumbnailPathForFile(const QString& path) const
{
        const auto embedUrl = youtubeEmbedUrlForFile(QFileInfo(path));
        if (!embedUrl) {
            return {};
        }
        return youtubeThumbnailCachePathForEmbedUrl(*embedUrl);
    }

QString MainWindow::cachedUrlThumbnailPathForFile(const QString& path) const
{
        const auto url = urlForShortcutFile(QFileInfo(path));
        if (!url) {
            return {};
        }
        return urlThumbnailCachePathForUrl(*url);
    }

bool MainWindow::isInlinePreviewObject(QObject* watched) const
{
        for (QObject* object = watched; object; object = object->parent()) {
            if (object->property("mycelInlinePreview").toBool()) {
                return true;
            }
        }
        return false;
    }

QString MainWindow::inlinePreviewPathForObject(QObject* watched) const
{
        for (QObject* object = watched; object; object = object->parent()) {
            const QVariant path = object->property("mycelInlinePreviewPath");
            if (path.isValid()) {
                return path.toString();
            }
        }
        return {};
    }

bool MainWindow::isSidePreviewObject(QObject* watched) const
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

