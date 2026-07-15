#include "main_window.h"
#include "file_name_order.h"

QString MainWindow::createFileInDirectory(const QString& dirPath, const QString& afterSiblingPath)
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
        searchController_.addPath(path, false);
        refreshSearchAfterIndexUpdate();
        rebuild(false);
        selectNodePath(path, true);  // focus the newly created file
        const QString trashPath = allocateTrashPath(name);
        recordHistory(QStringLiteral("ファイル作成"), {{trashPath, path}}, {{path, trashPath}},
                      historyBefore, historySelection);
        return path;
    }

QString MainWindow::createFolderInDirectory(const QString& dirPath, const QString& afterSiblingPath)
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
        searchController_.addPath(path, true);
        refreshSearchAfterIndexUpdate();
        rebuild(false);
        selectNodePath(path, true);  // focus the newly created folder
        const QString trashPath = allocateTrashPath(name);
        recordHistory(QStringLiteral("フォルダ作成"), {{trashPath, path}}, {{path, trashPath}},
                      historyBefore, historySelection);
        return path;
    }

void MainWindow::createFolder(Node* parent)
{
        if (parent && parent->isDir) {
            createFolderInDirectory(parent->path);
        }
    }

void MainWindow::createFile(Node* parent, const QString&)
{
        if (parent && parent->isDir) {
            createFileInDirectory(parent->path);
        }
    }

void MainWindow::createFileInFolderPath(const QString& folderPath)
{
        createFileInDirectory(folderPath);
    }

void MainWindow::createFolderInFolderPath(const QString& folderPath)
{
        createFolderInDirectory(folderPath);
    }

void MainWindow::createFileInSelectedFolder()
{
        Node* node = singleSelectedNode();
        if (!node) {
            recordDebugEvent(QStringLiteral("create file: no selected node"));
            return;
        }
        if (node->isExternalRoot) {
            recordDebugEvent(QStringLiteral("create file: external root door is a boundary"));
            return;  // the linked root owns its own contents; switch into it to edit them
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

void MainWindow::createFolderInSelectedFolder()
{
        Node* node = singleSelectedNode();
        if (!node) {
            recordDebugEvent(QStringLiteral("create folder: no selected node"));
            return;
        }
        if (node->isExternalRoot) {
            recordDebugEvent(QStringLiteral("create folder: external root door is a boundary"));
            return;  // the linked root owns its own contents; switch into it to edit them
        }
        // Shift+N follows the same rule as N: a selected folder gets the new folder inside it,
        // a selected file gets it beside itself (same directory).
        if (node->isDir) {
            recordDebugEvent(QStringLiteral("create folder in selected folder: %1").arg(relativeKeyForPath(node->path)));
            createFolder(node);
            return;
        }

        Node* parent = findVisibleNodeByPath(root_.get(), node->parentPath);
        if (!parent || !parent->isDir) {
            recordDebugEvent(QStringLiteral("create folder: selection has no visible parent"));
            return;
        }
        recordDebugEvent(QStringLiteral("create folder beside selection: %1").arg(relativeKeyForPath(parent->path)));
        createFolderInDirectory(parent->path, node->path);  // place directly below the selection
    }

bool MainWindow::handleBoardShortcut(QKeyEvent* event)
{
        if (!event || event->isAutoRepeat()) {
            return false;
        }
        if (renameEdit_ || sideEditorEditing_ || textInputHasFocus()) {
            return false;
        }

        Qt::KeyboardModifiers modifiers = event->modifiers();
        modifiers &= ~Qt::KeypadModifier;
        if (event->key() == Qt::Key_Escape && modifiers == Qt::NoModifier && searchBarActive()) {
            closeSearchBar();  // Esc ends the search even when the canvas has focus (§3.2)
            return true;
        }
        if (boardMode_) {
            // Board mode: no file-structure operations (create/delete/rename/paste/move). Keep
            // preview, open, edit and colour; everything else falls through to the view's own keys.
            if (event->key() == Qt::Key_E && modifiers == Qt::NoModifier) {
                return focusEditorForSelectedFile();
            }
            if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) &&
                modifiers == Qt::NoModifier) {
                return toggleSelectedFilePreviews();
            }
            if (event->key() == Qt::Key_O && modifiers == Qt::NoModifier) {
                return openSelectedNode();
            }
            if (event->key() == Qt::Key_O && modifiers == Qt::ShiftModifier) {
                return openSelectedWithApplication();
            }
            if (event->key() == Qt::Key_0 && modifiers == Qt::NoModifier) {
                return clearNodeColorForPaths(selectedNodePaths());
            }
            if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_6 && modifiers == Qt::NoModifier) {
                return assignPaletteColorToSelection(event->key() - Qt::Key_1);
            }
            return false;
        }
        if (event->key() == Qt::Key_E && modifiers == Qt::NoModifier) {
            return focusEditorForSelectedFile();
        }
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && modifiers == Qt::NoModifier) {
            // Enter renames (Finder-style); the old preview/collapse/sub-root role moved to Space.
            Node* node = singleSelectedNode();
            if (!node || node == root_.get() || node->isExternalRoot) {
                return false;
            }
            beginInlineRename(node);
            return true;
        }
        if (event->key() == Qt::Key_Space && modifiers == Qt::NoModifier) {
            return toggleSelectedPreviewOrCollapse();
        }
        if (event->key() == Qt::Key_O && modifiers == Qt::NoModifier) {
            return openSelectedNode();
        }
        if (event->key() == Qt::Key_O && modifiers == Qt::ShiftModifier) {
            return openSelectedWithApplication();
        }
        if (event->key() == Qt::Key_D && modifiers == Qt::ShiftModifier) {
            return removeIncomingFileLinksForPaths(selectedNodePaths());
        }
        if (event->key() == Qt::Key_0 && modifiers == Qt::NoModifier) {
            return clearNodeColorForPaths(selectedNodePaths());
        }
        if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_6 && modifiers == Qt::NoModifier) {
            return assignPaletteColorToSelection(event->key() - Qt::Key_1);
        }
        if ((event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) && modifiers == Qt::ShiftModifier) {
            return extendSelectionVertically(event->key() == Qt::Key_Up);
        }
        if ((event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) && modifiers == Qt::ControlModifier) {
            return reorderSelectedNode(event->key() == Qt::Key_Up ? -1 : 1);
        }
        if (event->key() == Qt::Key_Left && modifiers == Qt::ControlModifier) {
            return promoteSelectedNode();
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

bool MainWindow::isDeleteShortcut(int key, Qt::KeyboardModifiers modifiers) const
{
#ifdef Q_OS_MACOS
        return (key == Qt::Key_Delete && modifiers == Qt::NoModifier) ||
               (key == Qt::Key_Backspace && modifiers == Qt::NoModifier) ||
               (key == Qt::Key_Backspace && modifiers == Qt::MetaModifier);
#else
        return key == Qt::Key_Delete && modifiers == Qt::NoModifier;
#endif
    }

bool MainWindow::isSelectAllShortcut(int key, Qt::KeyboardModifiers modifiers) const
{
#ifdef Q_OS_MACOS
        return key == Qt::Key_A && modifiers == Qt::MetaModifier;
#else
        return key == Qt::Key_A && modifiers == Qt::ControlModifier;
#endif
    }

bool MainWindow::selectAllVisibleNodes()
{
        const bool selectedAny = selection_.selectAll();
        if (selectedAny) {
            view_->setFocus(Qt::ShortcutFocusReason);
            recordDebugEvent(QStringLiteral("selected all visible nodes"));
        }
        return selectedAny;
    }

bool MainWindow::textInputHasFocus() const
{
        return textInputWidgetHasFocus();
    }

void MainWindow::openNode(Node* node)
{
        if (!node) {
            return;
        }
        openPath(node->path);
    }

void MainWindow::openPath(const QString& path)
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

void MainWindow::openWithApplication(const QString& path)
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

bool MainWindow::openSelectedWithApplication()
{
        Node* node = singleSelectedNode();
        if (!node || node->isDir) {  // the "open with" picker is a file-only operation
            return false;
        }
        openWithApplication(node->path);
        return true;
    }

void MainWindow::sortFolderChildren(const QString& folderPath, bool byDate, bool descending)
{
        const QFileInfo folderInfo(folderPath);
        if (!folderInfo.exists() || !folderInfo.isDir()) {
            return;
        }
        QDir dir(folderInfo.absoluteFilePath());
        QFileInfoList entries = dir.entryInfoList(
            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::NoSort);
        const QCollator fileNameCollator = mycel::makeFileNameCollator();
        std::sort(entries.begin(), entries.end(), [byDate, descending, &fileNameCollator](const QFileInfo& a,
                                                                                         const QFileInfo& b) {
            if (a.isDir() != b.isDir()) {
                return a.isDir();  // folders first, regardless of direction
            }
            int cmp;
            if (byDate) {
                const QDateTime ta = a.lastModified();
                const QDateTime tb = b.lastModified();
                cmp = ta < tb ? -1 : (ta > tb ? 1 : 0);
                if (cmp == 0) {
                    cmp = mycel::compareFileNames(fileNameCollator, a.fileName(), b.fileName());
                }
            } else {
                cmp = mycel::compareFileNames(fileNameCollator, a.fileName(), b.fileName());
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

bool MainWindow::isGoScriptFile(const QString& path) const
{
        const QFileInfo info(path);
        return info.isFile() && info.suffix().compare(QStringLiteral("go"), Qt::CaseInsensitive) == 0;
    }

bool MainWindow::isPipelineScriptFile(const QString& path) const
{
        const QFileInfo info(path);
        const QString suffix = info.suffix().toLower();
        return info.isFile() && (suffix == QStringLiteral("py") || suffix == QStringLiteral("go"));
    }

std::pair<QString, QStringList> MainWindow::pipelineRunnerFor(const QString& scriptPath) const
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

void MainWindow::runProcessWithOutputDialog(const QString& title, const QString& program, const QStringList& args,
                                    const QString& workingDir, std::function<void(int)> onFinished)
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

void MainWindow::runGoScript(const QString& filePath)
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

QString MainWindow::pipelineInputFor(const QString& scriptPath) const
{
        for (const FileLink& link : fileLinks_) {
            if (link.to == scriptPath) {
                return link.from;
            }
        }
        return QString();
    }

QString MainWindow::pipelineOutputFor(const QString& scriptPath) const
{
        for (const FileLink& link : fileLinks_) {
            if (link.from == scriptPath) {
                return link.to;
            }
        }
        return QString();
    }

QString MainWindow::createPipelineOutput(const QString& scriptPath, const QString& inputPath)
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

void MainWindow::runPipelineForScript(const QString& scriptPath)
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

bool MainWindow::openSelectedNode()
{
        Node* node = singleSelectedNode();
        if (!node) {
            return false;
        }
        openNode(node);
        view_->setFocus(Qt::ShortcutFocusReason);
        return true;
    }

bool MainWindow::canEditTextFile(Node* node) const
{
        if (!node || node->isDir) {
            return false;
        }
        const QFileInfo info(node->path);
        return info.exists() && info.isFile() && isTextPreviewFile(info) && info.size() <= 4 * 1024 * 1024;
    }

bool MainWindow::canEditTextFilePath(const QString& path) const
{
        const QFileInfo info(path);
        return info.exists() && info.isFile() && isTextPreviewFile(info) && info.size() <= 4 * 1024 * 1024;
    }

void MainWindow::editTextFile(Node* node)
{
        if (!canEditTextFile(node)) {
            QMessageBox::information(this, QStringLiteral("Mycel"), QStringLiteral("このファイルは内蔵エディタで編集できません。"));
            return;
        }

        openCenteredTextEditor(node->path);
    }

void MainWindow::editTextFilePath(const QString& path)
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

bool MainWindow::openCenteredTextEditor(QString path)
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

void MainWindow::centerTextEditorDialog(QDialog* dialog) const
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

QColor MainWindow::colorForNode(const Node* node) const
{
        if (!node) {
            return neutralStroke();
        }
        const auto found = userColors_.find(node->path);
        return found == userColors_.end() ? neutralStroke() : found->second;
    }

QColor MainWindow::fillForNode(const Node* node) const
{
        if (!node) {
            return neutralFill();
        }
        const auto found = userColors_.find(node->path);
        return found == userColors_.end() ? neutralFill() : softFillFromColor(found->second);
    }

bool MainWindow::hasUserColor(Node* node) const
{
        return mycelStorageEnabled_ && node && userColors_.find(node->path) != userColors_.end();
    }

bool MainWindow::hasUserColorPath(const QString& path) const
{
        return mycelStorageEnabled_ && userColors_.find(path) != userColors_.end();
    }

bool MainWindow::hasUserColorForNode(const Node* node) const
{
        return node && userColors_.find(node->path) != userColors_.end();
    }

bool MainWindow::canDeleteFolder(Node* node) const
{
        return node && node->isDir && node != root_.get();
    }

void MainWindow::setNodeColor(Node* node, const QColor& color)
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
        // Hash the file the moment the user marks it as worth tracking, rather than waiting for
        // a scan to get to it: a file colored right after being created/copied in has otherwise
        // not been observed yet, so an external rename before the next scan would be unrecoverable.
        ensureHashCachedForRenameTracking(node->path);
        scene_.update();  // color only affects painting, not layout
        recordHistory(QStringLiteral("色変更"), {}, {}, historyBefore, historySelection);
    }

void MainWindow::setNodeColorPath(const QString& path, const QColor& color)
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
        ensureHashCachedForRenameTracking(path);
        scene_.update();  // color only affects painting, not layout
        selectNodePath(path, true);
        recordHistory(QStringLiteral("色変更"), {}, {}, historyBefore, historySelection);
    }

void MainWindow::clearNodeColor(Node* node)
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

void MainWindow::clearNodeColorPath(const QString& path)
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

void MainWindow::setNodeColorForPaths(const QStringList& paths, const QColor& color)
{
        if (!mycelStorageEnabled_ || !color.isValid()) {
            return;
        }
        QStringList targets;
        for (const QString& path : paths) {
            if (path.isEmpty()) {
                continue;
            }
            const auto it = userColors_.find(path);
            if (it != userColors_.end() && it->second == color) {
                continue;
            }
            targets.append(path);
        }
        if (targets.isEmpty()) {
            return;
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        for (const QString& path : targets) {
            userColors_[path] = color;
            ensureHashCachedForRenameTracking(path);
        }
        saveColorFile();
        scene_.update();  // color only affects painting, not layout
        recordHistory(QStringLiteral("色変更"), {}, {}, historyBefore, historySelection);
    }

bool MainWindow::clearNodeColorForPaths(const QStringList& paths)
{
        if (!mycelStorageEnabled_) {
            return false;
        }
        QStringList targets;
        for (const QString& path : paths) {
            if (!path.isEmpty() && userColors_.find(path) != userColors_.end()) {
                targets.append(path);
            }
        }
        if (targets.isEmpty()) {
            return false;
        }
        const MetadataSnapshot historyBefore = captureMetadataSnapshot();
        const QStringList historySelection = selectedNodePaths();
        for (const QString& path : targets) {
            userColors_.erase(path);
        }
        saveColorFile();
        scene_.update();  // color only affects painting, not layout
        recordHistory(QStringLiteral("色クリア"), {}, {}, historyBefore, historySelection);
        return true;
    }

bool MainWindow::assignPaletteColorToSelection(int paletteIndex)
{
        if (!mycelStorageEnabled_) {
            return false;
        }
        const auto palette = colorPalette();
        if (paletteIndex < 0 || paletteIndex >= static_cast<int>(palette.size())) {
            return false;
        }
        const QStringList paths = selectedNodePaths();
        if (paths.isEmpty()) {
            return false;
        }
        setNodeColorForPaths(paths, palette[static_cast<std::size_t>(paletteIndex)].second);
        return true;
    }

void MainWindow::beginInlineRename(Node* node)
{
        if (!node || node == root_.get() || node->isExternalRoot) {
            return;  // renaming an external door would rename the linked root on disk
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

void MainWindow::renameFile(Node* node)
{
        beginInlineRename(node);
    }

void MainWindow::renamePathInline(const QString& path)
{
        beginInlineRename(nodeForPath(path));
    }

void MainWindow::renameSelectedItem()
{
        beginInlineRename(singleSelectedNode());
    }

bool MainWindow::renamePathTo(const QString& path, const QString& name)
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
        saveHashCacheFile();
        rebuild(false);
        recordHistory(QStringLiteral("名前変更"), {{path, destination}}, {{destination, path}},
                      historyBefore, historySelection);
        return true;
    }

void MainWindow::deleteFile(Node* node)
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
        saveHashCacheFile();
        rebuild(false);
        recordHistory(QStringLiteral("削除"), {{filePath, trashPath}}, {{trashPath, filePath}},
                      historyBefore, historySelection);
    }

void MainWindow::deleteFilePath(const QString& path)
{
        deleteFile(nodeForPath(path));
    }

void MainWindow::deleteFolder(Node* node)
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
        saveHashCacheFile();
        rebuild(false);
        recordHistory(QStringLiteral("削除"), {{folderPath, trashPath}}, {{trashPath, folderPath}},
                      historyBefore, historySelection);
    }

void MainWindow::deleteFolderPath(const QString& path)
{
        deleteFolder(nodeForPath(path));
    }

bool MainWindow::hasSelectedFiles() const
{
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (nodeItem && !nodeItem->node()->isDir) {
                return true;
            }
        }
        return false;
    }

int MainWindow::selectedFileCount() const
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

int MainWindow::selectedDeletableItemCount() const
{
        int count = 0;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem) {
                continue;
            }
            Node* node = nodeItem->node();
            if (node && !node->isExternalRoot && (!node->isDir || canDeleteFolder(node))) {
                ++count;
            }
        }
        return count;
    }

void MainWindow::deleteSelectedItems()
{
        QStringList filePaths;
        QStringList folderPaths;
        for (QGraphicsItem* item : scene_.selectedItems()) {
            auto* nodeItem = dynamic_cast<NodeItem*>(item);
            if (!nodeItem) {
                continue;
            }
            Node* node = nodeItem->node();
            if (!node || node->isExternalRoot) {  // never delete a linked external root from here
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
        saveHashCacheFile();
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

bool MainWindow::hasSelectedItems() const
{
        return !scene_.selectedItems().isEmpty();
    }

QStringList MainWindow::selectedNodePaths() const
{
        return selection_.selectedPaths();
    }

void MainWindow::selectNodeItem(NodeItem* item, bool additive)
{
        selection_.select(item, additive);
        if (item && item->node()) {
            selectionRangeAnchorPath_ = item->node()->path;
        } else if (!additive) {
            selectionRangeAnchorPath_.clear();
        }
        selectionRangeCursorPath_ = selectionRangeAnchorPath_;
    }

void MainWindow::selectNodeItem(NodeItem* item, Qt::KeyboardModifiers modifiers)
{
        const bool range = (modifiers & Qt::ShiftModifier) != 0;
        const bool additive = (modifiers & (Qt::ControlModifier | Qt::MetaModifier)) != 0;
        if (range && selectNodeRangeToItem(item, additive)) {
            return;
        }
        selectNodeItem(item, additive || range);
    }

void MainWindow::restoreFolderSelection(const QString& folderPath)
{
        selectNodePath(folderPath);
    }

bool MainWindow::selectNodePath(const QString& path, bool ensureVisible)
{
        NodeItem* selectedItem = selection_.selectByPath(path);
        selectionRangeAnchorPath_ = selectedItem && selectedItem->node() ? selectedItem->node()->path : QString();
        selectionRangeCursorPath_ = selectionRangeAnchorPath_;
        view_->setFocus(Qt::ShortcutFocusReason);
        if (ensureVisible) {
            ensureNodeItemVisible(selectedItem);
        }
        if (selectedItem && ensureVisible) {
            QTimer::singleShot(0, this, [this, path] { ensureNodePathVisible(path); });
        }
        return selectedItem != nullptr;
    }

bool MainWindow::restoreSelection(const QStringList& paths, bool ensureVisible)
{
        NodeItem* firstSelectedItem = selection_.selectByPaths(paths);
        selectionRangeAnchorPath_ = firstSelectedItem && firstSelectedItem->node()
                                        ? firstSelectedItem->node()->path
                                        : QString();
        selectionRangeCursorPath_ = selectionRangeAnchorPath_;
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

bool MainWindow::moveSelectionWithTab(bool reverse)
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

bool MainWindow::moveSelectionVertically(bool upward)
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

bool MainWindow::moveSelectionToParent()
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

bool MainWindow::moveSelectionToFirstChild()
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

bool MainWindow::selectNodeRangeToItem(NodeItem* item, bool additive)
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
        selectionRangeCursorPath_ = targetPath;
        recordDebugEvent(QStringLiteral("selected range: %1 -> %2 (%3)")
                             .arg(relativeKeyForPath(anchorPath),
                                  relativeKeyForPath(targetPath))
                             .arg(rangePaths.size()));
        return true;
    }

bool MainWindow::extendSelectionVertically(bool upward)
{
        const QStringList paths = visibleNodePathsInOrder();
        if (paths.isEmpty()) {
            return false;
        }

        QString anchorPath = selectionRangeAnchorPath_;
        QString cursorPath = selectionRangeCursorPath_;
        if (anchorPath.isEmpty() || !paths.contains(anchorPath)) {
            Node* selected = singleSelectedNode();
            if (!selected) {
                return false;
            }
            anchorPath = selected->path;
            cursorPath = anchorPath;
        }
        if (cursorPath.isEmpty() || !paths.contains(cursorPath)) {
            cursorPath = anchorPath;
        }

        const int anchorIndex = paths.indexOf(anchorPath);
        const int cursorIndex = paths.indexOf(cursorPath) + (upward ? -1 : 1);
        if (cursorIndex < 0 || cursorIndex >= paths.size()) {
            return true;  // already at the edge: consume the key without changing the selection
        }

        QStringList rangePaths;
        for (int index = std::min(anchorIndex, cursorIndex); index <= std::max(anchorIndex, cursorIndex); ++index) {
            rangePaths.append(paths[index]);
        }
        selection_.selectByPaths(rangePaths, false);
        selectionRangeAnchorPath_ = anchorPath;
        selectionRangeCursorPath_ = paths[cursorIndex];
        ensureNodePathVisible(selectionRangeCursorPath_);
        recordDebugEvent(QStringLiteral("extended range selection: %1 -> %2 (%3)")
                             .arg(relativeKeyForPath(anchorPath),
                                  relativeKeyForPath(selectionRangeCursorPath_))
                             .arg(rangePaths.size()));
        return true;
    }

Node* MainWindow::singleSelectedNode() const
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

void MainWindow::exportArchive()
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

void MainWindow::importArchive()
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
        updateNativeWatcherModeForRoot();
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

void MainWindow::recordDebugEvent(const QString& message)
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

void MainWindow::copyDebugPaneToClipboard()
{
        if (!debugText_) {
            return;
        }
        refreshDebugPane(QStringLiteral("debug copied"));
        QApplication::clipboard()->setText(debugText_->toPlainText());
    }

void MainWindow::refreshDebugPane(const QString& eventMessage)
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

QString MainWindow::debugObjectName(QObject* object) const
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

QString MainWindow::debugKeyName(QKeyEvent* event) const
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

void MainWindow::finishInlineRename(bool commit)
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

void MainWindow::suspendInlineRenameActivity()
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

void MainWindow::resumeInlineRenameActivity()
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

QString MainWindow::uniqueAssetsDirectoryName(const QDir& parentDir, const QString& preferredName) const
{
        QString candidate = preferredName;
        for (int number = 2; parentDir.exists(candidate); ++number) {
            candidate = QStringLiteral("%1 %2").arg(preferredName, QString::number(number));
        }
        return candidate;
    }

QFileInfoList MainWindow::archiveEntriesForDirectory(const QString& directoryPath) const
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

void MainWindow::addCommonArchiveMetadata(QJsonObject& object, const QString& path, int order) const
{
        object.insert(QStringLiteral("path"), archiveRelativePath(relativeKeyForPath(path)));
        object.insert(QStringLiteral("order"), order);

        const auto color = userColors_.find(path);
        if (color != userColors_.end() && color->second.isValid()) {
            object.insert(QStringLiteral("color"), color->second.name(QColor::HexRgb));
        }
    }

void MainWindow::appendDirectoryToArchive(const QString& directoryPath, int depth, int order, QString& output,
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

void MainWindow::appendFileToArchive(const QFileInfo& fileInfo, int depth, int order, QString& output,
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

QString MainWindow::archiveLinksSection() const
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

void MainWindow::addImportedOrderEntry(const QString& destinationRoot, const QString& path, int order,
                               std::map<QString, std::vector<ArchiveOrderEntry>>& orderEntries) const
{
        const QFileInfo info(path);
        const QString parentRelative = QDir(destinationRoot).relativeFilePath(info.absolutePath());
        const QString key = parentRelative == QStringLiteral(".") ? QString() : archiveRelativePath(parentRelative);
        orderEntries[key].push_back({order, info.fileName()});
    }

void MainWindow::addImportedCommonMetadata(const QJsonObject& object, const QString& path, bool isDir,
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

bool MainWindow::archivePathForObject(const QJsonObject& object, QString& relativePath, QStringList& failures) const
{
        relativePath = archiveRelativePath(object.value(QStringLiteral("path")).toString());
        if (!isSafeArchiveRelativePath(relativePath)) {
            failures.append(QStringLiteral("不正なパス: %1").arg(relativePath));
            return false;
        }
        return true;
    }

void MainWindow::importArchiveFolder(const QJsonObject& object, const QString& destinationRoot,
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

bool MainWindow::readArchiveTextBlock(const QStringList& lines, int& index, QString& text, QStringList& failures) const
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

void MainWindow::importArchiveFile(const QJsonObject& object, const QStringList& lines, int& index, const QDir& archiveDir,
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

void MainWindow::importArchiveLinks(const QStringList& lines, int& index, const QString& destinationRoot,
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

void MainWindow::appendCreatedItemToOrder(const QString& directoryPath, const QString& newName, bool isDir,
                                  const QString& afterName)
{
        if (!mycelStorageEnabled_) {
            return;
        }

        // Hash the new file right away rather than waiting for a scan to notice it: a file
        // renamed externally before the next scan/sweep would otherwise have no "before" hash
        // to reconcile against.
        if (!isDir) {
            ensureHashCachedForRenameTracking(QDir(directoryPath).filePath(newName));
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

