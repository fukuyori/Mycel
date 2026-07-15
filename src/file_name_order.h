#pragma once

#include <QtCore/QCollator>
#include <QtCore/QString>

namespace mycel {

inline QCollator makeFileNameCollator()
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    return collator;
}

inline int compareFileNames(const QCollator& collator, const QString& a, const QString& b)
{
    const int naturalComparison = collator.compare(a, b);
    if (naturalComparison != 0) {
        return naturalComparison;
    }

    // Numeric and case-insensitive collation can consider distinct names equivalent
    // (for example, "item 2" and "item 02"). Keep the stored order deterministic.
    const int caseInsensitiveComparison = QString::compare(a, b, Qt::CaseInsensitive);
    if (caseInsensitiveComparison != 0) {
        return caseInsensitiveComparison;
    }
    return QString::compare(a, b, Qt::CaseSensitive);
}

}  // namespace mycel
