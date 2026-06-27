// Unit tests for the pure file-operation logic (src/mycel_fileops.hpp). No GUI; runs under
// ctest. Covers the design plan's core cases: conflict rename, move, descendant prohibition,
// same-dir no-op, and batch collisions.

#include "mycel_fileops.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>

#include <cstdio>

using mycel::FileOperationService;
using mycel::isDescendantPath;

static int g_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);         \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

static bool writeFile(const QString& path, const QString& content = QStringLiteral("x"))
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(content.toUtf8()) == content.toUtf8().size();
}

static void testAvailableDestination()
{
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QDir d(dir.path());

    // No conflict: returns the preferred name.
    CHECK(FileOperationService::availableDestination(d.path(), "Note.txt", false) == d.filePath("Note.txt"));

    // Conflict: " 2" inserted before the extension.
    CHECK(writeFile(d.filePath("Note.txt")));
    CHECK(FileOperationService::availableDestination(d.path(), "Note.txt", false) == d.filePath("Note 2.txt"));

    // Complete suffix (.tar.gz) is preserved as one extension.
    CHECK(writeFile(d.filePath("archive.tar.gz")));
    CHECK(FileOperationService::availableDestination(d.path(), "archive.tar.gz", false)
          == d.filePath("archive 2.tar.gz"));

    // A directory name has no extension split.
    CHECK(d.mkdir("Folder"));
    CHECK(FileOperationService::availableDestination(d.path(), "Folder", true) == d.filePath("Folder 2"));

    // Reserved names block a candidate even if no such file exists yet (batch moves).
    QSet<QString> reserved;
    CHECK(FileOperationService::availableDestination(d.path(), "New.txt", false, &reserved) == d.filePath("New.txt"));
    CHECK(FileOperationService::availableDestination(d.path(), "New.txt", false, &reserved) == d.filePath("New 2.txt"));
}

static void testMoveBasic()
{
    QTemporaryDir dir;
    const QDir d(dir.path());
    CHECK(d.mkdir("sub"));
    CHECK(writeFile(d.filePath("a.txt")));

    const auto res = FileOperationService::moveInto({{d.filePath("a.txt"), false, false}}, d.filePath("sub"));
    CHECK(!res.blocked);
    CHECK(res.failed.empty());
    CHECK(res.moved.size() == 1);
    CHECK(res.moved[0].destinationName == QStringLiteral("a.txt"));
    CHECK(QFile::exists(d.filePath("sub/a.txt")));
    CHECK(!QFile::exists(d.filePath("a.txt")));
}

static void testMoveConflictRename()
{
    QTemporaryDir dir;
    const QDir d(dir.path());
    CHECK(d.mkdir("sub"));
    CHECK(writeFile(d.filePath("a.txt"), "src"));
    CHECK(writeFile(d.filePath("sub/a.txt"), "existing"));

    const auto res = FileOperationService::moveInto({{d.filePath("a.txt"), false, false}}, d.filePath("sub"));
    CHECK(res.moved.size() == 1);
    CHECK(res.moved[0].destinationName == QStringLiteral("a 2.txt"));
    CHECK(QFile::exists(d.filePath("sub/a 2.txt")));
    CHECK(QFile::exists(d.filePath("sub/a.txt")));  // pre-existing file untouched
}

static void testMoveBatchConflict()
{
    QTemporaryDir dir;
    const QDir d(dir.path());
    CHECK(d.mkdir("sub"));
    CHECK(d.mkdir("x"));
    CHECK(d.mkdir("y"));
    CHECK(writeFile(d.filePath("x/a.txt")));
    CHECK(writeFile(d.filePath("y/a.txt")));

    const auto res = FileOperationService::moveInto(
        {{d.filePath("x/a.txt"), false, false}, {d.filePath("y/a.txt"), false, false}}, d.filePath("sub"));
    CHECK(res.moved.size() == 2);
    CHECK(QFile::exists(d.filePath("sub/a.txt")));
    CHECK(QFile::exists(d.filePath("sub/a 2.txt")));  // batch collision resolved
}

static void testMoveBlockedSkippedRoot()
{
    QTemporaryDir dir;
    const QDir d(dir.path());
    CHECK(d.mkpath("parent/child"));

    // Moving a directory into its own descendant is blocked, nothing moves.
    const auto blocked = FileOperationService::moveInto({{d.filePath("parent"), true, false}},
                                                        d.filePath("parent/child"));
    CHECK(blocked.blocked);
    CHECK(blocked.moved.empty());
    CHECK(QFile::exists(d.filePath("parent/child")));

    // A source already in the target directory is a no-op.
    CHECK(writeFile(d.filePath("a.txt")));
    const auto same = FileOperationService::moveInto({{d.filePath("a.txt"), false, false}}, d.path());
    CHECK(same.skippedSameDir);
    CHECK(same.moved.empty());
    CHECK(QFile::exists(d.filePath("a.txt")));

    // Root requests are skipped.
    const auto root = FileOperationService::moveInto({{d.filePath("a.txt"), false, true}}, d.filePath("parent"));
    CHECK(root.moved.empty());
    CHECK(QFile::exists(d.filePath("a.txt")));
}

static void testIsDescendant()
{
    QTemporaryDir dir;
    const QDir d(dir.path());
    CHECK(d.mkpath("a/b"));
    CHECK(isDescendantPath(d.filePath("a"), d.filePath("a/b")));
    CHECK(!isDescendantPath(d.filePath("a/b"), d.filePath("a")));
    CHECK(!isDescendantPath(d.filePath("a"), d.filePath("a")));
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    testAvailableDestination();
    testMoveBasic();
    testMoveConflictRename();
    testMoveBatchConflict();
    testMoveBlockedSkippedRoot();
    testIsDescendant();

    if (g_failures == 0) {
        std::printf("All FileOperationService tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed.\n", g_failures);
    return 1;
}
