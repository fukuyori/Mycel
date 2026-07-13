#pragma once

// Pure search logic for the node search feature (docs/search-feature-design.ja.md): index
// storage, query normalization, multi-word AND matching, result ranking, and current-result
// navigation. Free of Qt Widgets / QGraphicsScene so it can be unit tested in isolation
// (see tests/search_controller_test.cpp). Shared between the app and the test target.

#include <algorithm>
#include <atomic>
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace mycel {

// One searchable file or folder. `path` is the identifier and uses the same normalized
// absolute form as Node::path; `relativePath` is what the UI shows ('/' separators).
struct SearchEntry {
    QString path;
    QString relativePath;
    QString name;
    QString normalizedName;
    QString normalizedRelativePath;
    bool isDirectory = false;
};

class SearchController {
public:
    // The same normalization is applied to names, relative paths, and the query, so
    // matching ignores letter case and half/full-width differences (NFKC).
    static QString normalizeSearchText(const QString& text)
    {
        return text.normalized(QString::NormalizationForm_KC).toCaseFolded();
    }

    static SearchEntry makeEntry(const QString& absolutePath, const QString& rootPath, bool isDirectory)
    {
        SearchEntry entry;
        entry.isDirectory = isDirectory;
        assignPaths(entry, QDir::cleanPath(absolutePath), rootPath);
        return entry;
    }

    // --- index ---------------------------------------------------------------------------

    // Root switches drop the whole search state: index, query, board restriction, and any
    // content-match set from the background full-text scan.
    void resetForRoot(const QString& rootPath)
    {
        rootPath_ = QDir::cleanPath(rootPath);
        index_.clear();
        indexComplete_ = false;
        rawQuery_.clear();
        allowedPaths_.reset();
        contentMatchQuery_.clear();
        contentMatchPaths_.clear();
        evaluate(QString(), -1);
    }

    QString rootPath() const { return rootPath_; }

    // Replaces the whole index (startup scan, background reconcile) and re-evaluates the
    // current query against it. `complete` is false for the provisional visible-nodes-only
    // index used while the startup scan is still running.
    void setIndex(std::vector<SearchEntry> entries, bool complete)
    {
        const QString previousPath = currentPath();
        const int previousIndex = current_;
        index_ = std::move(entries);
        indexComplete_ = complete;
        evaluate(previousPath, previousIndex);
    }

    bool indexComplete() const { return indexComplete_; }
    int indexSize() const { return static_cast<int>(index_.size()); }
    const std::vector<SearchEntry>& indexEntries() const { return index_; }

    void addPath(const QString& absolutePath, bool isDirectory)
    {
        const QString previousPath = currentPath();
        const int previousIndex = current_;
        const QString cleaned = QDir::cleanPath(absolutePath);
        for (auto& entry : index_) {
            if (entry.path == cleaned) {
                entry.isDirectory = isDirectory;
                evaluate(previousPath, previousIndex);
                return;
            }
        }
        index_.push_back(makeEntry(cleaned, rootPath_, isDirectory));
        evaluate(previousPath, previousIndex);
    }

    // Removes the path and everything below it (folder deletion).
    void removeSubtree(const QString& absolutePath)
    {
        const QString previousPath = currentPath();
        const int previousIndex = current_;
        const QString cleaned = QDir::cleanPath(absolutePath);
        const QString prefix = cleaned + QLatin1Char('/');
        index_.erase(std::remove_if(index_.begin(), index_.end(),
                         [&](const SearchEntry& entry) {
                             return entry.path == cleaned || entry.path.startsWith(prefix);
                         }),
            index_.end());
        evaluate(previousPath, previousIndex);
    }

    // Rename/move: rewrites the entry and all descendants under the old path.
    void rekeySubtree(const QString& oldPath, const QString& newPath)
    {
        const QString oldCleaned = QDir::cleanPath(oldPath);
        const QString newCleaned = QDir::cleanPath(newPath);
        if (oldCleaned == newCleaned) {
            return;
        }
        const QString prefix = oldCleaned + QLatin1Char('/');
        QString previousPath = currentPath();
        const int previousIndex = current_;
        // The current result moves with the rename.
        if (previousPath == oldCleaned) {
            previousPath = newCleaned;
        } else if (previousPath.startsWith(prefix)) {
            previousPath = newCleaned + previousPath.mid(oldCleaned.size());
        }
        for (auto& entry : index_) {
            if (entry.path == oldCleaned) {
                assignPaths(entry, newCleaned, rootPath_);
            } else if (entry.path.startsWith(prefix)) {
                assignPaths(entry, newCleaned + entry.path.mid(oldCleaned.size()), rootPath_);
            }
        }
        evaluate(previousPath, previousIndex);
    }

    // --- query and results ---------------------------------------------------------------

    // Board mode restricts results to the visible cards; tree mode passes std::nullopt.
    void setAllowedPaths(std::optional<QSet<QString>> allowed)
    {
        const QString previousPath = currentPath();
        const int previousIndex = current_;
        allowedPaths_ = std::move(allowed);
        evaluate(previousPath, previousIndex);
    }

    void setQuery(const QString& query)
    {
        const QString previousPath = currentPath();
        const int previousIndex = current_;
        rawQuery_ = query;
        evaluate(previousPath, previousIndex);
    }

    QString query() const { return rawQuery_; }
    bool hasQuery() const { return !tokens_.isEmpty(); }

    int resultCount() const { return static_cast<int>(results_.size()); }
    const SearchEntry& resultAt(int i) const { return index_[static_cast<size_t>(results_[static_cast<size_t>(i)])]; }
    int currentIndex() const { return current_; }

    const SearchEntry* currentEntry() const
    {
        if (current_ < 0 || current_ >= resultCount()) {
            return nullptr;
        }
        return &resultAt(current_);
    }

    QString currentPath() const
    {
        const SearchEntry* entry = currentEntry();
        return entry ? entry->path : QString();
    }

    // Enter / Shift+Enter: wrap-around navigation. Returns the new current entry, or
    // nullptr when there are no results.
    const SearchEntry* next() { return step(1); }
    const SearchEntry* previous() { return step(-1); }
    void clearCurrent() { current_ = -1; }

    // Paths of every match, for highlighting visible nodes.
    QSet<QString> matchPaths() const
    {
        QSet<QString> paths;
        paths.reserve(resultCount());
        for (int idx : results_) {
            paths.insert(index_[static_cast<size_t>(idx)].path);
        }
        return paths;
    }

    // --- content (full-text) matches -------------------------------------------------------

    // Reported by the background content scan (runContentSearch) for a specific query text.
    // A file whose body matched joins the results after every name/path match; the stored
    // query guards against stale reports — once the input changes, the set is ignored until
    // the scan for the new text lands.
    void setContentMatches(const QString& forQuery, QSet<QString> paths)
    {
        contentMatchQuery_ = normalizeSearchText(forQuery).simplified();
        contentMatchPaths_ = std::move(paths);
        reevaluate();
    }

    void clearContentMatches()
    {
        if (contentMatchQuery_.isEmpty() && contentMatchPaths_.isEmpty()) {
            return;
        }
        contentMatchQuery_.clear();
        contentMatchPaths_.clear();
        reevaluate();
    }

    // True when result i is a body-only hit (its name/relative path did not match).
    bool resultIsContentMatch(int i) const
    {
        return i >= 0 && i < resultCount() && resultRanks_[static_cast<size_t>(i)] == ContentMatchRank;
    }

private:
    static void assignPaths(SearchEntry& entry, const QString& cleanedAbsolutePath, const QString& rootPath)
    {
        entry.path = cleanedAbsolutePath;
        entry.relativePath = rootPath.isEmpty()
            ? cleanedAbsolutePath
            : QDir(rootPath).relativeFilePath(cleanedAbsolutePath);
        entry.name = QFileInfo(cleanedAbsolutePath).fileName();
        entry.normalizedName = normalizeSearchText(entry.name);
        entry.normalizedRelativePath = normalizeSearchText(entry.relativePath);
    }

    const SearchEntry* step(int delta)
    {
        if (results_.empty()) {
            current_ = -1;
            return nullptr;
        }
        const int count = resultCount();
        if (current_ < 0) {
            current_ = delta > 0 ? 0 : count - 1;
        } else {
            current_ = (current_ + delta % count + count) % count;
        }
        return &resultAt(current_);
    }

    void reevaluate()
    {
        const QString previousPath = currentPath();
        const int previousIndex = current_;
        evaluate(previousPath, previousIndex);
    }

    // Recomputes results for the current query and index. Callers snapshot the current
    // result BEFORE mutating index_ (the old result indices become invalid). The current
    // result survives by path when still matching; when it was removed, the entry that
    // took its slot becomes current (i.e. deletion advances to the next result).
    void evaluate(const QString& previousPath, int previousIndex)
    {
        const QString normalizedQuery = normalizeSearchText(rawQuery_).simplified();
        tokens_ = normalizedQuery.isEmpty()
            ? QStringList()
            : normalizedQuery.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        results_.clear();
        resultRanks_.clear();

        if (!tokens_.isEmpty()) {
            // Ranks: 0 exact name, 1 name prefix, 2 all words in the name, 3 words only
            // found in the relative path, ContentMatchRank(4) body-only hits from the
            // background content scan. Ties sort by relative path, case-insensitive.
            std::vector<std::pair<int, int>> ranked;
            ranked.reserve(index_.size());
            std::vector<bool> matchedByName(index_.size(), false);
            for (size_t i = 0; i < index_.size(); ++i) {
                const SearchEntry& entry = index_[i];
                if (allowedPaths_ && !allowedPaths_->contains(entry.path)) {
                    continue;
                }
                bool allWordsMatch = true;
                bool allWordsInName = true;
                for (const QString& token : tokens_) {
                    const bool inName = entry.normalizedName.contains(token);
                    if (!inName) {
                        allWordsInName = false;
                        if (!entry.normalizedRelativePath.contains(token)) {
                            allWordsMatch = false;
                            break;
                        }
                    }
                }
                if (!allWordsMatch) {
                    continue;
                }
                matchedByName[i] = true;
                int rank = 3;
                if (entry.normalizedName == normalizedQuery) {
                    rank = 0;
                } else if (entry.normalizedName.startsWith(normalizedQuery)) {
                    rank = 1;
                } else if (allWordsInName) {
                    rank = 2;
                }
                ranked.emplace_back(rank, static_cast<int>(i));
            }
            // Body-only matches join after every name/path match, but only while the scan
            // that produced them was for exactly this query text.
            if (!contentMatchPaths_.isEmpty() && contentMatchQuery_ == normalizedQuery) {
                for (size_t i = 0; i < index_.size(); ++i) {
                    const SearchEntry& entry = index_[i];
                    if (matchedByName[i] || entry.isDirectory) {
                        continue;
                    }
                    if (allowedPaths_ && !allowedPaths_->contains(entry.path)) {
                        continue;
                    }
                    if (contentMatchPaths_.contains(entry.path)) {
                        ranked.emplace_back(ContentMatchRank, static_cast<int>(i));
                    }
                }
            }
            std::stable_sort(ranked.begin(), ranked.end(), [this](const auto& a, const auto& b) {
                if (a.first != b.first) {
                    return a.first < b.first;
                }
                return QString::compare(index_[static_cast<size_t>(a.second)].relativePath,
                           index_[static_cast<size_t>(b.second)].relativePath, Qt::CaseInsensitive)
                    < 0;
            });
            results_.reserve(ranked.size());
            resultRanks_.reserve(ranked.size());
            for (const auto& rankedEntry : ranked) {
                results_.push_back(rankedEntry.second);
                resultRanks_.push_back(rankedEntry.first);
            }
        }

        current_ = -1;
        if (!results_.empty() && !previousPath.isEmpty()) {
            for (int i = 0; i < resultCount(); ++i) {
                if (resultAt(i).path == previousPath) {
                    current_ = i;
                    break;
                }
            }
            if (current_ < 0 && previousIndex >= 0) {
                current_ = std::min(previousIndex, resultCount() - 1);
            }
        }
    }

    static constexpr int ContentMatchRank = 4;

    QString rootPath_;
    std::vector<SearchEntry> index_;
    bool indexComplete_ = false;
    QString rawQuery_;
    QStringList tokens_;
    std::optional<QSet<QString>> allowedPaths_;
    QString contentMatchQuery_;      // normalized+simplified query the content scan ran for
    QSet<QString> contentMatchPaths_;
    std::vector<int> results_;       // indices into index_, ranked
    std::vector<int> resultRanks_;   // rank per results_ slot (ContentMatchRank = body-only)
    int current_ = -1;               // index into results_, -1 = none
};

// --- background full-text scan (docs/search-feature-design.ja.md §13) ----------------------

// §13: cap body reads at 1MiB until measurements justify a different limit.
constexpr qint64 ContentSearchMaxFileSize = 1 * 1024 * 1024;

// Body text cached per path, keyed for staleness by (size, mtime): the file is re-read only
// when either changed. QString payloads are implicitly shared, so copying the whole map into
// a worker thread copies structure, not text.
struct ContentSearchCacheEntry {
    qint64 size = 0;
    qint64 mtimeMs = 0;
    QString normalizedText;
};

struct ContentSearchResult {
    QSet<QString> matchedPaths;
    std::map<QString, ContentSearchCacheEntry> cache;  // input cache + refreshed entries
    bool cancelled = false;
};

// Runs on a worker thread — never on the GUI thread (§13). `candidatePaths` is already
// filtered to text-preview extensions by the caller; `skipFile` excludes files whose data is
// not locally present (cloud placeholders — searching must not trigger a download). Files
// over ContentSearchMaxFileSize are skipped and dropped from the cache. Bodies are decoded
// as UTF-8, the same treatment the text preview applies.
inline ContentSearchResult runContentSearch(const QStringList& candidatePaths, const QString& rawQuery,
                                            std::map<QString, ContentSearchCacheEntry> cache,
                                            const std::function<bool(const QFileInfo&)>& skipFile,
                                            const std::atomic_bool& cancelled)
{
    ContentSearchResult result;
    const QString normalizedQuery = SearchController::normalizeSearchText(rawQuery).simplified();
    const QStringList tokens = normalizedQuery.isEmpty()
        ? QStringList()
        : normalizedQuery.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        result.cache = std::move(cache);
        return result;
    }
    for (const QString& path : candidatePaths) {
        if (cancelled.load()) {
            result.cancelled = true;
            break;
        }
        const QFileInfo info(path);
        if (!info.isFile() || info.size() > ContentSearchMaxFileSize) {
            cache.erase(path);
            continue;
        }
        if (skipFile && skipFile(info)) {
            continue;
        }
        const qint64 mtimeMs = info.lastModified().toMSecsSinceEpoch();
        auto it = cache.find(path);
        if (it == cache.end() || it->second.size != info.size() || it->second.mtimeMs != mtimeMs) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            ContentSearchCacheEntry entry;
            entry.size = info.size();
            entry.mtimeMs = mtimeMs;
            entry.normalizedText = SearchController::normalizeSearchText(QString::fromUtf8(file.readAll()));
            it = cache.insert_or_assign(path, std::move(entry)).first;
        }
        const QString& text = it->second.normalizedText;
        bool allWordsMatch = true;
        for (const QString& token : tokens) {
            if (!text.contains(token)) {
                allWordsMatch = false;
                break;
            }
        }
        if (allWordsMatch) {
            result.matchedPaths.insert(path);
        }
    }
    result.cache = std::move(cache);
    return result;
}

}  // namespace mycel
