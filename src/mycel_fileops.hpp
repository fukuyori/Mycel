#pragma once

// Pure filesystem operations, free of any Qt widget / GUI / metadata dependency, so they
// can be unit tested in isolation (see tests/fileops_test.cpp). Shared between the app
// (src/main.cpp) and the test target.

#include <filesystem>
#include <system_error>
#include <utility>
#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSet>
#include <QtCore/QString>

namespace mycel {

// True when `candidate` lives strictly under `ancestor` in the filesystem tree.
inline bool isDescendantPath(const QString& ancestor, const QString& candidate)
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

// Conflict-free destination naming, recursive copy, and validated moves. Move results carry
// the old->new path mapping so callers (MainWindow today, a MetadataService later) can update
// their own state.
class FileOperationService {
public:
    struct MoveRequest {
        QString path;
        bool isDir = false;
        bool isRoot = false;
    };
    struct MovedEntry {
        QString oldPath;
        QString newPath;
        QString sourceParentPath;
        QString name;
        QString destinationName;
        bool isDir = false;
    };
    struct MoveResult {
        std::vector<MovedEntry> moved;
        std::vector<std::pair<QString, QString>> failed;  // (sourcePath, errorMessage)
        bool blocked = false;        // self/descendant violation: caller aborts and warns
        bool skippedSameDir = false; // a source already lives in the target directory
    };

    static QString availableDestination(const QString& targetDirectoryPath, const QString& preferredName,
                                        bool isDirectory, QSet<QString>* reservedNames = nullptr)
    {
        QFileInfo preferredInfo(preferredName);
        const QString completeSuffix = preferredInfo.completeSuffix();
        const QString baseName = isDirectory || completeSuffix.isEmpty()
                                     ? preferredName
                                     : preferredName.left(preferredName.size() - completeSuffix.size() - 1);
        const QString suffix = isDirectory || completeSuffix.isEmpty() ? QString() : QStringLiteral(".") + completeSuffix;

        QDir target(targetDirectoryPath);
        QString candidateName = preferredName;
        QString candidate = target.filePath(candidateName);
        for (int number = 2; QFileInfo::exists(candidate) ||
                               (reservedNames && reservedNames->contains(candidateName.toCaseFolded()));
             ++number) {
            candidateName = QStringLiteral("%1 %2%3").arg(baseName).arg(number).arg(suffix);
            candidate = target.filePath(candidateName);
        }
        if (reservedNames) {
            reservedNames->insert(candidateName.toCaseFolded());
        }
        return candidate;
    }

    static bool copyDirectoryRecursively(const QString& sourcePath, const QString& destinationPath)
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

    // Move every request into targetDirPath. Root requests are skipped; a source already
    // inside the target directory is a no-op (skippedSameDir). Any self/descendant move
    // aborts the whole batch (blocked) without touching the filesystem. Batch name
    // collisions are resolved together so concurrently moved siblings never clobber.
    static MoveResult moveInto(const std::vector<MoveRequest>& requests, const QString& targetDirPath)
    {
        MoveResult result;
        for (const MoveRequest& req : requests) {
            if (req.isRoot || req.path.isEmpty()) {
                continue;
            }
            if (req.path == targetDirPath || (req.isDir && isDescendantPath(req.path, targetDirPath))) {
                result.blocked = true;
                return result;
            }
        }

        QSet<QString> reserved;
        for (const MoveRequest& req : requests) {
            if (req.isRoot || req.path.isEmpty()) {
                continue;
            }
            QFileInfo info(req.path);
            const QString preferred = QDir(targetDirPath).filePath(info.fileName());
            if (QDir::cleanPath(preferred) == QDir::cleanPath(req.path)) {
                result.skippedSameDir = true;
                continue;
            }
            const QString destination = availableDestination(targetDirPath, info.fileName(), req.isDir, &reserved);

            std::error_code ec;
            std::filesystem::rename(std::filesystem::path(req.path.toStdString()),
                                    std::filesystem::path(destination.toStdString()), ec);
            if (ec) {
                result.failed.emplace_back(req.path, QString::fromStdString(ec.message()));
                continue;
            }
            result.moved.push_back({req.path, destination, info.absolutePath(), info.fileName(),
                                    QFileInfo(destination).fileName(), req.isDir});
        }
        return result;
    }
};

}  // namespace mycel
