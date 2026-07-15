#include "file_name_order.h"

#include <QtCore/QStringList>

#include <algorithm>
#include <iostream>

namespace {

bool expectOrder(const QStringList& actual, const QStringList& expected, const char* label)
{
    if (actual == expected) {
        return true;
    }

    std::cerr << label << " failed\n  actual:   " << actual.join(QStringLiteral(", ")).toStdString()
              << "\n  expected: " << expected.join(QStringLiteral(", ")).toStdString() << '\n';
    return false;
}

QStringList sorted(QStringList names, bool descending)
{
    const QCollator collator = mycel::makeFileNameCollator();
    std::sort(names.begin(), names.end(), [&collator, descending](const QString& a, const QString& b) {
        const int comparison = mycel::compareFileNames(collator, a, b);
        return descending ? comparison > 0 : comparison < 0;
    });
    return names;
}

}  // namespace

int main()
{
    bool ok = true;
    const QStringList input = {QStringLiteral("item 10.txt"), QStringLiteral("item 2.txt"),
                               QStringLiteral("item 1.txt")};

    ok &= expectOrder(sorted(input, false),
                      {QStringLiteral("item 1.txt"), QStringLiteral("item 2.txt"),
                       QStringLiteral("item 10.txt")},
                      "natural ascending order");
    ok &= expectOrder(sorted(input, true),
                      {QStringLiteral("item 10.txt"), QStringLiteral("item 2.txt"),
                       QStringLiteral("item 1.txt")},
                      "natural descending order");
    ok &= expectOrder(sorted({QStringLiteral("item 2.txt"), QStringLiteral("item 02.txt")}, false),
                      {QStringLiteral("item 02.txt"), QStringLiteral("item 2.txt")},
                      "deterministic equivalent-name order");

    return ok ? 0 : 1;
}
