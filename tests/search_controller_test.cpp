// Unit tests for the pure node-search logic (src/search_controller.h). No GUI; runs under
// ctest. Covers the design document's cases (docs/search-feature-design.ja.md §11.1):
// empty query, case folding, NFKC width normalization, ranking order, path-only matches,
// multi-word AND, mixed files/folders, wrap-around navigation, index replacement, and
// stable ordering of same-name files.

#include "search_controller.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>
#include <QtCore/QThread>

#include <atomic>
#include <cstdio>
#include <utility>
#include <vector>

using mycel::SearchController;
using mycel::SearchEntry;

static int g_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);         \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

static const QString kRoot = QStringLiteral("/searchroot");

static SearchEntry entryFor(const QString& relativePath, bool isDirectory = false)
{
    return SearchController::makeEntry(kRoot + QLatin1Char('/') + relativePath, kRoot, isDirectory);
}

static SearchController makeController(std::vector<SearchEntry> entries, bool complete = true)
{
    SearchController controller;
    controller.resetForRoot(kRoot);
    controller.setIndex(std::move(entries), complete);
    return controller;
}

static bool writeFileContent(const QString& path, const QString& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray utf8 = content.toUtf8();
    return file.write(utf8) == utf8.size();
}

static void testEmptyQuery()
{
    auto controller = makeController({entryFor(QStringLiteral("plan.md"))});

    controller.setQuery(QString());
    CHECK(controller.resultCount() == 0);
    CHECK(!controller.hasQuery());
    CHECK(controller.currentEntry() == nullptr);
    CHECK(controller.matchPaths().isEmpty());

    controller.setQuery(QStringLiteral("   "));
    CHECK(controller.resultCount() == 0);

    controller.setQuery(QStringLiteral("plan"));
    CHECK(controller.resultCount() == 1);

    // Clearing the query drops results and the current position.
    controller.next();
    controller.setQuery(QString());
    CHECK(controller.resultCount() == 0);
    CHECK(controller.currentEntry() == nullptr);
}

static void testCaseInsensitive()
{
    auto controller = makeController({entryFor(QStringLiteral("README.md"))});

    controller.setQuery(QStringLiteral("readme"));
    CHECK(controller.resultCount() == 1);

    controller.setQuery(QStringLiteral("ReAdMe"));
    CHECK(controller.resultCount() == 1);
}

static void testWidthNormalization()
{
    auto controller = makeController({
        entryFor(QStringLiteral("データ.txt")),
        entryFor(QStringLiteral("ABC.txt")),
    });

    // Half-width katakana query matches the full-width name.
    controller.setQuery(QStringLiteral("ﾃﾞｰﾀ"));
    CHECK(controller.resultCount() == 1);
    CHECK(controller.resultAt(0).name == QStringLiteral("データ.txt"));

    // Full-width latin query matches the ASCII name.
    controller.setQuery(QStringLiteral("ＡＢＣ"));
    CHECK(controller.resultCount() == 1);
    CHECK(controller.resultAt(0).name == QStringLiteral("ABC.txt"));
}

static void testRankingOrder()
{
    // Inserted deliberately out of rank order.
    auto controller = makeController({
        entryFor(QStringLiteral("planning/readme.txt")),      // words only in the path
        entryFor(QStringLiteral("notes/myplan.txt")),         // partial name match
        entryFor(QStringLiteral("plan.md")),                  // name prefix
        entryFor(QStringLiteral("plan"), true),               // exact name
    });

    controller.setQuery(QStringLiteral("plan"));
    CHECK(controller.resultCount() == 4);
    CHECK(controller.resultAt(0).name == QStringLiteral("plan"));
    CHECK(controller.resultAt(1).name == QStringLiteral("plan.md"));
    CHECK(controller.resultAt(2).name == QStringLiteral("myplan.txt"));
    CHECK(controller.resultAt(3).name == QStringLiteral("readme.txt"));
}

static void testRelativePathOnlyMatch()
{
    auto controller = makeController({
        entryFor(QStringLiteral("notes"), true),
        entryFor(QStringLiteral("notes/idea.txt")),
        entryFor(QStringLiteral("other/todo.txt")),
    });

    controller.setQuery(QStringLiteral("notes"));
    CHECK(controller.resultCount() == 2);
    CHECK(controller.resultAt(0).name == QStringLiteral("notes"));       // exact name first
    CHECK(controller.resultAt(1).name == QStringLiteral("idea.txt"));    // path-only match
}

static void testMultiWordAnd()
{
    auto controller = makeController({
        entryFor(QStringLiteral("01_現状把握/業務フロー.md")),
        entryFor(QStringLiteral("02_その他/業務メモ.md")),
    });

    controller.setQuery(QStringLiteral("業務"));
    CHECK(controller.resultCount() == 2);

    // Both words must appear in the name or the relative path.
    controller.setQuery(QStringLiteral("現状 業務"));
    CHECK(controller.resultCount() == 1);
    CHECK(controller.resultAt(0).name == QStringLiteral("業務フロー.md"));

    controller.setQuery(QStringLiteral("現状 存在しない"));
    CHECK(controller.resultCount() == 0);
}

static void testMixedFilesAndFolders()
{
    auto controller = makeController({
        entryFor(QStringLiteral("業務.md")),
        entryFor(QStringLiteral("業務"), true),
    });

    controller.setQuery(QStringLiteral("業務"));
    CHECK(controller.resultCount() == 2);
    CHECK(controller.resultAt(0).isDirectory);                            // exact name first
    CHECK(controller.resultAt(1).name == QStringLiteral("業務.md"));
}

static void testNextPreviousWrap()
{
    auto controller = makeController({
        entryFor(QStringLiteral("a/dup.txt")),
        entryFor(QStringLiteral("b/dup.txt")),
        entryFor(QStringLiteral("c/dup.txt")),
    });
    controller.setQuery(QStringLiteral("dup"));
    CHECK(controller.resultCount() == 3);
    CHECK(controller.currentIndex() == -1);  // typing alone selects nothing

    CHECK(controller.next() != nullptr);
    CHECK(controller.currentIndex() == 0);
    controller.next();
    controller.next();
    CHECK(controller.currentIndex() == 2);
    controller.next();  // wraps to the first result
    CHECK(controller.currentIndex() == 0);

    controller.previous();  // wraps back to the last result
    CHECK(controller.currentIndex() == 2);

    controller.clearCurrent();
    controller.previous();  // Shift+Enter from nothing starts at the end
    CHECK(controller.currentIndex() == 2);
}

static void testIndexReplacementKeepsCurrent()
{
    auto controller = makeController({
        entryFor(QStringLiteral("a/plan.txt")),
        entryFor(QStringLiteral("b/plan.txt")),
        entryFor(QStringLiteral("c/plan.txt")),
    });
    controller.setQuery(QStringLiteral("plan"));
    controller.next();
    controller.next();
    CHECK(controller.currentEntry()->relativePath == QStringLiteral("b/plan.txt"));

    // Same path still present: the current result survives the replacement.
    controller.setIndex({
        entryFor(QStringLiteral("a/plan.txt")),
        entryFor(QStringLiteral("b/plan.txt")),
        entryFor(QStringLiteral("c/plan.txt")),
        entryFor(QStringLiteral("d/plan.txt")),
    }, true);
    CHECK(controller.resultCount() == 4);
    CHECK(controller.currentEntry()->relativePath == QStringLiteral("b/plan.txt"));

    // Current result deleted: its slot now holds the next result.
    controller.setIndex({
        entryFor(QStringLiteral("a/plan.txt")),
        entryFor(QStringLiteral("c/plan.txt")),
    }, true);
    CHECK(controller.resultCount() == 2);
    CHECK(controller.currentEntry() != nullptr);
    CHECK(controller.currentEntry()->relativePath == QStringLiteral("c/plan.txt"));

    // Everything gone: no current position.
    controller.setIndex({}, true);
    CHECK(controller.resultCount() == 0);
    CHECK(controller.currentEntry() == nullptr);
}

static void testStableOrderForSameNames()
{
    auto controller = makeController({
        entryFor(QStringLiteral("c/dup.txt")),
        entryFor(QStringLiteral("a/dup.txt")),
        entryFor(QStringLiteral("b/dup.txt")),
    });
    controller.setQuery(QStringLiteral("dup"));
    CHECK(controller.resultCount() == 3);
    CHECK(controller.resultAt(0).relativePath == QStringLiteral("a/dup.txt"));
    CHECK(controller.resultAt(1).relativePath == QStringLiteral("b/dup.txt"));
    CHECK(controller.resultAt(2).relativePath == QStringLiteral("c/dup.txt"));
}

static void testIncrementalUpdates()
{
    auto controller = makeController({
        entryFor(QStringLiteral("proj"), true),
        entryFor(QStringLiteral("proj/old"), true),
        entryFor(QStringLiteral("proj/old/file.txt")),
    });
    controller.setQuery(QStringLiteral("note"));
    CHECK(controller.resultCount() == 0);

    // Creation appears in the results without a full rebuild.
    controller.addPath(kRoot + QStringLiteral("/proj/note.txt"), false);
    CHECK(controller.resultCount() == 1);

    // Adding the same path twice does not duplicate the entry.
    controller.addPath(kRoot + QStringLiteral("/proj/note.txt"), false);
    CHECK(controller.resultCount() == 1);

    // Rename/move rewrites the folder and its descendants.
    controller.rekeySubtree(kRoot + QStringLiteral("/proj/old"), kRoot + QStringLiteral("/proj/renamed"));
    controller.setQuery(QStringLiteral("renamed"));
    CHECK(controller.resultCount() == 2);
    controller.setQuery(QStringLiteral("old"));
    CHECK(controller.resultCount() == 0);
    controller.setQuery(QStringLiteral("file"));
    CHECK(controller.resultCount() == 1);
    CHECK(controller.resultAt(0).relativePath == QStringLiteral("proj/renamed/file.txt"));

    // Folder deletion removes the subtree.
    controller.removeSubtree(kRoot + QStringLiteral("/proj/renamed"));
    CHECK(controller.resultCount() == 0);
    controller.setQuery(QStringLiteral("note"));
    CHECK(controller.resultCount() == 1);  // sibling untouched
}

static void testAllowedPathsRestriction()
{
    auto controller = makeController({
        entryFor(QStringLiteral("cards/a.txt")),
        entryFor(QStringLiteral("cards/b.txt")),
    });
    controller.setQuery(QStringLiteral("cards"));
    CHECK(controller.resultCount() == 2);

    // Board mode: only visible cards remain in the result set.
    controller.setAllowedPaths(QSet<QString>{kRoot + QStringLiteral("/cards/a.txt")});
    CHECK(controller.resultCount() == 1);
    CHECK(controller.resultAt(0).name == QStringLiteral("a.txt"));

    controller.setAllowedPaths(std::nullopt);
    CHECK(controller.resultCount() == 2);
}

static void testContentMatchMerge()
{
    auto controller = makeController({
        entryFor(QStringLiteral("plan.md")),               // name match
        entryFor(QStringLiteral("notes/body.txt")),        // body-only match
        entryFor(QStringLiteral("notes/other.txt")),       // no match at all
        entryFor(QStringLiteral("plandir"), true),         // name match (dir)
    });
    controller.setQuery(QStringLiteral("plan"));
    CHECK(controller.resultCount() == 2);

    controller.setContentMatches(QStringLiteral("plan"),
        {kRoot + QStringLiteral("/notes/body.txt"), kRoot + QStringLiteral("/plan.md")});
    CHECK(controller.resultCount() == 3);
    // Body-only hits rank after every name/path match; a name match that also matched by
    // body keeps its name rank.
    CHECK(controller.resultAt(2).name == QStringLiteral("body.txt"));
    CHECK(!controller.resultIsContentMatch(0));
    CHECK(!controller.resultIsContentMatch(1));
    CHECK(controller.resultIsContentMatch(2));

    // A stale report (different query text) is ignored.
    controller.setQuery(QStringLiteral("plandir"));
    CHECK(controller.resultCount() == 1);
    CHECK(!controller.resultIsContentMatch(0));

    // Directories never join via content matches.
    controller.setQuery(QStringLiteral("plan"));
    controller.setContentMatches(QStringLiteral("plan"), {kRoot + QStringLiteral("/plandir")});
    CHECK(controller.resultCount() == 2);

    // clearContentMatches drops body-only results again.
    controller.setContentMatches(QStringLiteral("plan"), {kRoot + QStringLiteral("/notes/body.txt")});
    CHECK(controller.resultCount() == 3);
    controller.clearContentMatches();
    CHECK(controller.resultCount() == 2);
}

static void testContentMatchAllowedPaths()
{
    auto controller = makeController({
        entryFor(QStringLiteral("cards/a.txt")),
        entryFor(QStringLiteral("cards/b.txt")),
    });
    controller.setQuery(QStringLiteral("hello"));
    CHECK(controller.resultCount() == 0);
    controller.setContentMatches(QStringLiteral("hello"),
        {kRoot + QStringLiteral("/cards/a.txt"), kRoot + QStringLiteral("/cards/b.txt")});
    CHECK(controller.resultCount() == 2);

    // Board mode restriction also applies to body-only matches.
    controller.setAllowedPaths(QSet<QString>{kRoot + QStringLiteral("/cards/a.txt")});
    CHECK(controller.resultCount() == 1);
    CHECK(controller.resultAt(0).name == QStringLiteral("a.txt"));
}

static void testRunContentSearch()
{
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QDir d(dir.path());
    CHECK(writeFileContent(d.filePath("hit.txt"), QStringLiteral("現状把握のための業務フロー整理")));
    CHECK(writeFileContent(d.filePath("miss.txt"), QStringLiteral("nothing to see here")));
    CHECK(writeFileContent(d.filePath("wide.txt"), QStringLiteral("abc definitions")));

    const QStringList candidates{d.filePath("hit.txt"), d.filePath("miss.txt"), d.filePath("wide.txt")};
    std::atomic_bool cancelled{false};

    auto result = mycel::runContentSearch(candidates, QStringLiteral("業務"), {}, {}, cancelled);
    CHECK(!result.cancelled);
    CHECK(result.matchedPaths.size() == 1);
    CHECK(result.matchedPaths.contains(d.filePath("hit.txt")));
    CHECK(result.cache.size() == 3);

    // Multi-word AND across the body; width/case normalization applies (ＡＢＣ → abc).
    result = mycel::runContentSearch(candidates, QStringLiteral("現状 業務"), std::move(result.cache), {}, cancelled);
    CHECK(result.matchedPaths.size() == 1);
    result = mycel::runContentSearch(candidates, QStringLiteral("ＡＢＣ"), std::move(result.cache), {}, cancelled);
    CHECK(result.matchedPaths.size() == 1);
    CHECK(result.matchedPaths.contains(d.filePath("wide.txt")));

    // A changed file (different size) is re-read on the next scan with the old cache.
    CHECK(writeFileContent(d.filePath("miss.txt"), QStringLiteral("now with 業務 inside")));
    result = mycel::runContentSearch(candidates, QStringLiteral("業務"), std::move(result.cache), {}, cancelled);
    CHECK(result.matchedPaths.size() == 2);

    // The skip predicate (cloud placeholders in the app) excludes files from matching.
    result = mycel::runContentSearch(candidates, QStringLiteral("業務"), std::move(result.cache),
        [&d](const QFileInfo& info) { return info.fileName() == QStringLiteral("miss.txt"); }, cancelled);
    CHECK(result.matchedPaths.size() == 1);

    // Cancellation stops the walk and reports it.
    cancelled.store(true);
    result = mycel::runContentSearch(candidates, QStringLiteral("業務"), std::move(result.cache), {}, cancelled);
    CHECK(result.cancelled);
    CHECK(result.matchedPaths.isEmpty());
}

static void testRunContentSearchSizeLimit()
{
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QDir d(dir.path());
    // Over the 1MiB cap: never read, never matched, and evicted from the cache.
    QString big = QStringLiteral("業務 ");
    big += QString(static_cast<int>(mycel::ContentSearchMaxFileSize), QLatin1Char('x'));
    CHECK(writeFileContent(d.filePath("big.txt"), big));

    std::atomic_bool cancelled{false};
    std::map<QString, mycel::ContentSearchCacheEntry> seeded;
    seeded[d.filePath("big.txt")] = {1, 1, QStringLiteral("業務")};
    auto result = mycel::runContentSearch({d.filePath("big.txt")}, QStringLiteral("業務"),
                                          std::move(seeded), {}, cancelled);
    CHECK(result.matchedPaths.isEmpty());
    CHECK(result.cache.find(d.filePath("big.txt")) == result.cache.end());
}

static void testResetForRoot()
{
    auto controller = makeController({entryFor(QStringLiteral("plan.md"))});
    controller.setQuery(QStringLiteral("plan"));
    CHECK(controller.resultCount() == 1);

    controller.resetForRoot(QStringLiteral("/otherroot"));
    CHECK(controller.indexSize() == 0);
    CHECK(controller.query().isEmpty());
    CHECK(controller.resultCount() == 0);
    CHECK(!controller.indexComplete());
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    testEmptyQuery();
    testCaseInsensitive();
    testWidthNormalization();
    testRankingOrder();
    testRelativePathOnlyMatch();
    testMultiWordAnd();
    testMixedFilesAndFolders();
    testNextPreviousWrap();
    testIndexReplacementKeepsCurrent();
    testStableOrderForSameNames();
    testIncrementalUpdates();
    testAllowedPathsRestriction();
    testContentMatchMerge();
    testContentMatchAllowedPaths();
    testRunContentSearch();
    testRunContentSearchSizeLimit();
    testResetForRoot();

    if (g_failures == 0) {
        std::printf("All SearchController tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed.\n", g_failures);
    return 1;
}
