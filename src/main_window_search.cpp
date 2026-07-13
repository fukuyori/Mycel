#include "main_window.h"

// Node search (docs/search-feature-design.ja.md): search-bar UI behaviour, result
// navigation, match highlighting, and index upkeep. The matching/ranking logic itself lives
// in src/search_controller.h, which is pure and unit-tested (tests/search_controller_test.cpp).

namespace {
constexpr int SearchDebounceMs = 200;  // §3.2: update results 150-250ms after typing stops
}

QWidget* MainWindow::createSearchBar()
{
        searchBar_ = new QWidget(this);
        searchBar_->setObjectName(QStringLiteral("SearchBar"));
        auto* barLayout = new QVBoxLayout(searchBar_);
        barLayout->setContentsMargins(8, 4, 8, 4);
        barLayout->setSpacing(2);

        auto* inputRow = new QHBoxLayout();
        inputRow->setSpacing(6);
        auto* caption = new QLabel(QStringLiteral("検索:"), searchBar_);
        searchInput_ = new QLineEdit(searchBar_);
        searchInput_->setClearButtonEnabled(true);
        searchInput_->setPlaceholderText(QStringLiteral("ファイル名・フォルダ名・相対パス"));
        searchInput_->installEventFilter(this);  // Enter/Shift+Enter/Esc: see eventFilter()
        searchCountLabel_ = new QLabel(searchBar_);
        searchCountLabel_->setMinimumWidth(110);
        searchPrevButton_ = new QToolButton(searchBar_);
        searchPrevButton_->setText(QStringLiteral("▲"));
        searchPrevButton_->setToolTip(QStringLiteral("前の結果 (Shift+Enter)"));
        searchNextButton_ = new QToolButton(searchBar_);
        searchNextButton_->setText(QStringLiteral("▼"));
        searchNextButton_->setToolTip(QStringLiteral("次の結果 (Enter)"));
        auto* closeButton = new QToolButton(searchBar_);
        closeButton->setText(QStringLiteral("✕"));
        closeButton->setToolTip(QStringLiteral("検索を閉じる (Esc)"));
        inputRow->addWidget(caption);
        inputRow->addWidget(searchInput_, 1);
        inputRow->addWidget(searchCountLabel_);
        inputRow->addWidget(searchPrevButton_);
        inputRow->addWidget(searchNextButton_);
        inputRow->addWidget(closeButton);
        barLayout->addLayout(inputRow);

        searchPathLabel_ = new QLabel(searchBar_);
        searchPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        searchPathLabel_->hide();  // shown only while a current result exists
        barLayout->addWidget(searchPathLabel_);

        searchDebounceTimer_.setSingleShot(true);
        searchDebounceTimer_.setInterval(SearchDebounceMs);
        connect(&searchDebounceTimer_, &QTimer::timeout, this, [this] { applySearchQueryFromInput(); });
        connect(searchInput_, &QLineEdit::textChanged, this, [this] {
            if (searchBarActive()) {
                searchDebounceTimer_.start();
            }
        });
        connect(searchPrevButton_, &QToolButton::clicked, this, [this] { goToSearchResult(false); });
        connect(searchNextButton_, &QToolButton::clicked, this, [this] { goToSearchResult(true); });
        connect(closeButton, &QToolButton::clicked, this, [this] { closeSearchBar(); });

        searchBar_->hide();
        return searchBar_;
    }

bool MainWindow::searchBarActive() const
{
        return searchBar_ && searchBar_->isVisible();
    }

void MainWindow::openSearchBar()
{
        if (!searchBar_ || !searchInput_) {
            return;
        }
        if (!searchBarActive()) {
            searchBar_->show();
            updateSearchCandidateScope();
            // Until the startup scan lands, search what is currently displayed instead of
            // nothing (§4.3). The finished scan replaces this provisional index.
            if (!searchController_.indexComplete()) {
                rebuildProvisionalSearchIndexFromTree();
            }
            applySearchQueryFromInput();
            recordDebugEvent(QStringLiteral("search bar opened"));
        }
        // Already open: Ctrl+F selects the existing text for retyping (§3.2).
        searchInput_->setFocus(Qt::ShortcutFocusReason);
        searchInput_->selectAll();
        updateSearchBarStatus();
    }

void MainWindow::closeSearchBar()
{
        if (!searchBar_ || !searchBarActive()) {
            return;
        }
        searchDebounceTimer_.stop();
        ++contentSearchGeneration_;  // void any in-flight body scan's queued report
        cancelContentSearch();
        const QString lastResultPath = searchCurrentPath_;
        if (searchInput_) {
            searchInput_->blockSignals(true);
            searchInput_->clear();
            searchInput_->blockSignals(false);
        }
        searchController_.setQuery(QString());
        searchController_.clearContentMatches();
        searchCurrentPath_.clear();
        const bool hadReveal = !searchRevealPaths_.isEmpty();
        searchRevealPaths_.clear();
        searchBar_->hide();
        applySearchHighlights();  // clears every mark
        if (hadReveal && !boardMode_) {
            // Drop the temporary expansion: the saved collapse state and the normal display
            // limits come back (§6.1). The last result stays selected when still displayed.
            rebuild(false);
            if (!lastResultPath.isEmpty()) {
                selectNodePath(lastResultPath, false);
            }
        }
        if (view_) {
            view_->setFocus();
        }
        recordDebugEvent(QStringLiteral("search bar closed"));
    }

void MainWindow::applySearchQueryFromInput()
{
        if (!searchBarActive() || !searchInput_) {
            return;
        }
        searchController_.setQuery(searchInput_->text());
        searchCurrentPath_ = searchController_.currentPath();
        applySearchHighlights();
        startContentSearch();  // body matches follow asynchronously (§13)
        updateSearchBarStatus();
    }

void MainWindow::goToSearchResult(bool forward)
{
        if (!searchBarActive()) {
            return;
        }
        if (searchDebounceTimer_.isActive()) {
            searchDebounceTimer_.stop();
            applySearchQueryFromInput();  // flush pending input before navigating
        }
        const mycel::SearchEntry* entry = forward ? searchController_.next() : searchController_.previous();
        if (!entry) {
            updateSearchBarStatus();
            return;
        }
        searchCurrentPath_ = entry->path;
        focusCurrentSearchResult();
        updateSearchBarStatus();
    }

void MainWindow::focusCurrentSearchResult()
{
        const QString path = searchCurrentPath_;
        if (path.isEmpty()) {
            return;
        }
        if (!nodeItemsByPath_.contains(path) && !boardMode_) {
            // The result is hidden behind a collapsed folder, the depth limit, or the
            // per-folder child cap: reveal its whole ancestor chain and rescan. The set only
            // grows during a search session (so earlier results stay visible while cycling)
            // and closeSearchBar() discards it entirely.
            QSet<QString> reveal = searchRevealPaths_;
            reveal.insert(path);
            const QString cleanRoot = QDir::cleanPath(rootPath_);
            const QString rootPrefix =
                cleanRoot.endsWith(QLatin1Char('/')) ? cleanRoot : cleanRoot + QLatin1Char('/');
            QString ancestor = QFileInfo(path).absolutePath();
            while (!ancestor.isEmpty() && QDir::cleanPath(ancestor) != cleanRoot &&
                   ancestor.startsWith(rootPrefix)) {
                reveal.insert(ancestor);
                ancestor = QFileInfo(ancestor).absolutePath();
            }
            if (reveal != searchRevealPaths_) {
                searchRevealPaths_ = reveal;
                rebuild(false);
            }
        }
        if (nodeItemsByPath_.contains(path)) {
            // selectNodePath moves the keyboard focus to the canvas; give it back to the
            // input so Enter / Shift+Enter keep cycling through the results (§3.2).
            const bool inputHadFocus = searchInput_ && searchInput_->hasFocus();
            selectNodePath(path, /*ensureVisible=*/true);
            if (inputHadFocus) {
                searchInput_->setFocus(Qt::OtherFocusReason);
            }
        }
    }

void MainWindow::updateSearchBarStatus()
{
        if (!searchBarActive() || !searchCountLabel_) {
            return;
        }
        QString text;
        if (searchController_.hasQuery()) {
            const int count = searchController_.resultCount();
            if (count == 0) {
                text = QStringLiteral("0 件");
            } else if (searchController_.currentIndex() >= 0) {
                text = QStringLiteral("%1 / %2").arg(searchController_.currentIndex() + 1).arg(count);
            } else {
                text = QStringLiteral("%1 件").arg(count);
            }
        }
        if (!searchController_.indexComplete()) {
            text += text.isEmpty() ? QStringLiteral("索引作成中…") : QStringLiteral("（索引作成中…）");
        }
        if (contentSearchRunning_) {
            text += text.isEmpty() ? QStringLiteral("本文検索中…") : QStringLiteral("（本文検索中…）");
        }
        searchCountLabel_->setText(text);
        const bool hasResults = searchController_.resultCount() > 0;
        if (searchPrevButton_) {
            searchPrevButton_->setEnabled(hasResults);
        }
        if (searchNextButton_) {
            searchNextButton_->setEnabled(hasResults);
        }
        if (searchPathLabel_) {
            const mycel::SearchEntry* entry = searchController_.currentEntry();
            searchPathLabel_->setVisible(entry != nullptr);
            if (entry) {
                // Body-only hits are labelled so they are distinguishable from name matches (§13).
                searchPathLabel_->setText(
                    searchController_.resultIsContentMatch(searchController_.currentIndex())
                        ? QStringLiteral("%1（本文一致）").arg(entry->relativePath)
                        : entry->relativePath);
            }
        }
    }

void MainWindow::applySearchHighlights()
{
        const bool marking = searchBarActive() && searchController_.hasQuery();
        const QSet<QString> matches = marking ? searchController_.matchPaths() : QSet<QString>();
        for (auto it = nodeItemsByPath_.cbegin(); it != nodeItemsByPath_.cend(); ++it) {
            it.value()->setSearchMatch(matches.contains(it.key()));
        }
    }

void MainWindow::rebuildSearchIndexFromLists(const QStringList& directories, const QStringList& files,
                                             bool complete)
{
        std::vector<mycel::SearchEntry> entries;
        entries.reserve(static_cast<size_t>(directories.size() + files.size()));
        const QString cleanRoot = QDir::cleanPath(rootPath_);
        for (const QString& directory : directories) {
            if (QDir::cleanPath(directory) == cleanRoot) {
                continue;  // the opened root itself is not a search target
            }
            entries.push_back(mycel::SearchController::makeEntry(directory, rootPath_, true));
        }
        for (const QString& file : files) {
            entries.push_back(mycel::SearchController::makeEntry(file, rootPath_, false));
        }
        searchController_.setIndex(std::move(entries), complete);
        refreshSearchAfterIndexUpdate();
    }

void MainWindow::rebuildProvisionalSearchIndexFromTree()
{
        std::vector<mycel::SearchEntry> entries;
        if (root_) {
            visitNodes(*root_, [&](Node& node) {
                if (&node == root_.get()) {
                    return;
                }
                entries.push_back(mycel::SearchController::makeEntry(node.path, rootPath_, node.isDir));
            });
        }
        searchController_.setIndex(std::move(entries), /*complete=*/false);
    }

void MainWindow::refreshSearchAfterIndexUpdate()
{
        if (!searchBarActive()) {
            return;
        }
        // Same query, refreshed result set (§8). The controller already kept (or advanced)
        // the current position; only the marks and labels need to catch up here — the
        // selection is deliberately not moved by an index change. The body scan restarts
        // over the new candidate set; the (size, mtime) cache keeps that cheap.
        searchCurrentPath_ = searchController_.currentPath();
        applySearchHighlights();
        startContentSearch();
        updateSearchBarStatus();
    }

void MainWindow::startContentSearch()
{
        ++contentSearchGeneration_;
        cancelContentSearch();
        if (!searchBarActive() || !searchController_.hasQuery()) {
            searchController_.clearContentMatches();
            return;
        }
        // Candidates: indexed files with a text-preview extension (§13 — the same set the
        // text preview accepts). The filter runs here so the worker stays a pure function;
        // isTextPreviewFile only inspects the file name, so this loop does not touch disk.
        QStringList candidates;
        for (const mycel::SearchEntry& entry : searchController_.indexEntries()) {
            if (!entry.isDirectory && isTextPreviewFile(QFileInfo(entry.path))) {
                candidates.append(entry.path);
            }
        }
        if (candidates.isEmpty()) {
            searchController_.clearContentMatches();
            return;
        }
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        contentSearchCancelled_ = cancelled;
        contentSearchRunning_ = true;
        const quint64 generation = contentSearchGeneration_;
        const QString query = searchController_.query();
        // The cache copy shares its QString payloads (implicit sharing) — cheap even when
        // the cached bodies are large. Cloud placeholders are skipped so searching never
        // triggers a download (§13).
        contentSearchThread_ = QThread::create(
            [this, generation, query, candidates, cache = contentSearchCache_, cancelled]() mutable {
                mycel::ContentSearchResult result = mycel::runContentSearch(
                    candidates, query, std::move(cache),
                    [](const QFileInfo& info) { return isCloudPlaceholderFile(info); }, *cancelled);
                if (cancelled->load()) {
                    return;  // superseded or shutting down; a newer scan owns the UI
                }
                QMetaObject::invokeMethod(
                    this,
                    [this, generation, query, result = std::move(result)] {
                        finishContentSearch(generation, query, result);
                    },
                    Qt::QueuedConnection);
            });
        contentSearchThread_->start();
    }

void MainWindow::finishContentSearch(quint64 generation, const QString& query,
                                     const mycel::ContentSearchResult& result)
{
        if (generation != contentSearchGeneration_) {
            return;  // a newer query/index superseded this scan
        }
        contentSearchRunning_ = false;
        contentSearchCache_ = result.cache;
        if (!result.cancelled && searchBarActive()) {
            searchController_.setContentMatches(query, result.matchedPaths);
            searchCurrentPath_ = searchController_.currentPath();
            applySearchHighlights();
        }
        updateSearchBarStatus();
    }

void MainWindow::cancelContentSearch()
{
        if (contentSearchCancelled_) {
            contentSearchCancelled_->store(true);
        }
        if (contentSearchThread_) {
            contentSearchThread_->wait();  // returns promptly: the flag is checked per file
            delete contentSearchThread_;
            contentSearchThread_ = nullptr;
        }
        contentSearchRunning_ = false;
    }

void MainWindow::updateSearchCandidateScope()
{
        if (boardMode_) {
            // Board mode: only the cards currently shown by the pattern are search targets
            // (§7); hidden and unplaced cards are excluded so the counts mean what they say.
            QSet<QString> visibleCards;
            for (const auto& [key, state] : boardCards_) {
                if (!state.visible) {
                    continue;
                }
                const QString path = absolutePathForKey(key);
                if (QFileInfo::exists(path)) {
                    visibleCards.insert(QDir::cleanPath(path));
                }
            }
            searchController_.setAllowedPaths(std::move(visibleCards));
        } else {
            searchController_.setAllowedPaths(std::nullopt);
        }
        refreshSearchAfterIndexUpdate();
    }
