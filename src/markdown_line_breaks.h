#pragma once

#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QTextBlock>
#include <QtGui/QTextCursor>
#include <QtGui/QTextDocument>

namespace mycel {

// Mycel shows Markdown with "newline = line break" semantics: every line the author wrote stays on
// its own line, and only a blank line starts a new paragraph. CommonMark (and Qt's Markdown
// importer) instead joins consecutive lines of a paragraph into one. Two steps bridge the gap:
//
// 1. markdownWithHardLineBreaks() appends a sentinel plus a backslash hard break to each line that
//    CommonMark would otherwise merge with the next one. Lines inside fenced / indented code,
//    tables, headings, rules and HTML blocks are left untouched, as is a line whose successor
//    starts a new block (a hard break at the end of a paragraph would be printed literally).
// 2. Qt imports a hard break as a *new block* carrying the paragraph margins, which looks exactly
//    like a paragraph break. mergeHardLineBreaks() therefore finds the sentinel and joins the
//    following block back into the same paragraph with a line separator, so the text wraps
//    without paragraph spacing and a broken list item stays one item.
//
// setMarkdownWithLineBreaks() runs both on a QTextDocument.

// Zero-width space: invisible if it ever survives, and never produced by Markdown syntax itself.
inline constexpr char16_t kLineBreakSentinel = 0x200B;

namespace detail {

inline QString withoutQuoteMarkers(QString content, int* depth)
{
    static const QRegularExpression marker(QStringLiteral("^ {0,3}> ?"));
    int levels = 0;
    for (;;) {
        const QRegularExpressionMatch m = marker.match(content);
        if (!m.hasMatch()) {
            break;
        }
        content.remove(0, m.capturedLength());
        ++levels;
    }
    if (depth) {
        *depth = levels;
    }
    return content;
}

inline int leadingIndent(const QString& s)
{
    int columns = 0;
    for (const QChar ch : s) {
        if (ch == QLatin1Char(' ')) {
            ++columns;
        } else if (ch == QLatin1Char('\t')) {
            columns += 4;
        } else {
            break;
        }
    }
    return columns;
}

inline bool isBlank(const QString& s) { return s.trimmed().isEmpty(); }

inline bool isFenceLine(const QString& s, QString* marker = nullptr)
{
    static const QRegularExpression fence(QStringLiteral("^ {0,3}(`{3,}|~{3,})"));
    const QRegularExpressionMatch m = fence.match(s);
    if (!m.hasMatch()) {
        return false;
    }
    if (marker) {
        *marker = m.captured(1);
    }
    return true;
}

inline bool closesFence(const QString& s, const QString& opening)
{
    QString marker;
    if (!isFenceLine(s, &marker) || marker.at(0) != opening.at(0) || marker.size() < opening.size()) {
        return false;
    }
    return s.trimmed() == marker;  // a closing fence carries nothing but the marker
}

inline bool isHeading(const QString& s)
{
    static const QRegularExpression heading(QStringLiteral("^ {0,3}#{1,6}(\\s|$)"));
    return heading.match(s).hasMatch();
}

inline bool isThematicBreak(const QString& s)
{
    static const QRegularExpression rule(QStringLiteral("^ {0,3}([-*_])(\\s*\\1){2,}\\s*$"));
    return rule.match(s).hasMatch();
}

inline bool isListItem(const QString& s)
{
    static const QRegularExpression item(QStringLiteral("^\\s*([-*+]|\\d{1,9}[.)])(\\s|$)"));
    return item.match(s).hasMatch();
}

inline bool isTableSeparator(const QString& s)
{
    static const QRegularExpression separator(QStringLiteral("^\\s*\\|?[\\s:-]*\\|[\\s:|-]*$"));
    return s.contains(QLatin1Char('-')) && separator.match(s).hasMatch();
}

inline bool isHtmlBlockStart(const QString& s)
{
    static const QRegularExpression html(QStringLiteral("^ {0,3}<[A-Za-z/!?]"));
    return html.match(s).hasMatch();
}

inline bool endsWithHardBreak(const QString& s)
{
    return s.endsWith(QLatin1Char('\\')) || s.endsWith(QStringLiteral("  "));
}

}  // namespace detail

inline QString markdownWithHardLineBreaks(const QString& markdown)
{
    using namespace detail;

    const QStringList raw = markdown.split(QLatin1Char('\n'));
    const int count = raw.size();

    // Content with the trailing CR and blockquote markers removed, plus the quote depth.
    QStringList content;
    QList<int> quoteDepth;
    content.reserve(count);
    quoteDepth.reserve(count);
    for (const QString& line : raw) {
        QString body = line;
        if (body.endsWith(QLatin1Char('\r'))) {
            body.chop(1);
        }
        int depth = 0;
        content.append(withoutQuoteMarkers(body, &depth));
        quoteDepth.append(depth);
    }

    QStringList out;
    out.reserve(count);
    bool inFence = false;
    QString fenceMarker;
    bool inIndentedCode = false;
    bool inTable = false;
    bool previousBlank = true;

    for (int i = 0; i < count; ++i) {
        const QString& line = raw.at(i);
        const QString& body = content.at(i);
        const bool blank = isBlank(body);

        if (inFence) {
            if (closesFence(body, fenceMarker)) {
                inFence = false;
            }
            out.append(line);
            previousBlank = false;
            continue;
        }
        if (isFenceLine(body, &fenceMarker)) {
            inFence = true;
            out.append(line);
            previousBlank = false;
            continue;
        }
        if (blank) {
            inIndentedCode = false;
            inTable = false;
            out.append(line);
            previousBlank = true;
            continue;
        }
        // An indented code block can only start after a blank line (it cannot interrupt a
        // paragraph); once started it runs until the first non-indented line.
        if (!inIndentedCode && previousBlank && leadingIndent(body) >= 4) {
            inIndentedCode = true;
        }
        if (inIndentedCode) {
            if (leadingIndent(body) >= 4) {
                out.append(line);
                previousBlank = false;
                continue;
            }
            inIndentedCode = false;
        }
        previousBlank = false;

        if (!inTable && body.contains(QLatin1Char('|')) && i + 1 < count &&
            isTableSeparator(content.at(i + 1))) {
            inTable = true;
        }
        if (inTable || isHeading(body) || isThematicBreak(body) || isHtmlBlockStart(body) ||
            endsWithHardBreak(body)) {
            out.append(line);
            continue;
        }

        // The line is paragraph text (possibly a list item or quoted). Add the break only when
        // the next line would be merged into the same paragraph by CommonMark.
        bool joinable = i + 1 < count;
        if (joinable) {
            const QString& next = content.at(i + 1);
            joinable = !isBlank(next) && !isFenceLine(next) && !isHeading(next) &&
                       !isThematicBreak(next) && !isListItem(next) && !isHtmlBlockStart(next) &&
                       quoteDepth.at(i + 1) <= quoteDepth.at(i) &&
                       !(next.contains(QLatin1Char('|')) && i + 2 < count &&
                         isTableSeparator(content.at(i + 2)));
        }
        if (!joinable) {
            out.append(line);
            continue;
        }
        QString broken = line;
        const bool cr = broken.endsWith(QLatin1Char('\r'));
        if (cr) {
            broken.chop(1);
        }
        broken.append(QChar(kLineBreakSentinel));
        broken.append(QLatin1Char('\\'));
        if (cr) {
            broken.append(QLatin1Char('\r'));
        }
        out.append(broken);
    }
    return out.join(QLatin1Char('\n'));
}

// Joins every block that ends with the sentinel to its successor with a line separator, so the
// hard breaks produced by markdownWithHardLineBreaks() wrap inside one paragraph instead of
// opening a new, margin-spaced block. Stray sentinels (no successor) are simply removed.
inline void mergeHardLineBreaks(QTextDocument* doc)
{
    if (!doc) {
        return;
    }
    const QChar sentinel(kLineBreakSentinel);
    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        for (;;) {
            const QString text = block.text();
            if (!text.endsWith(sentinel)) {
                break;
            }
            cursor.setPosition(block.position() + text.size());
            cursor.deletePreviousChar();  // the sentinel
            if (!block.next().isValid()) {
                break;
            }
            cursor.deleteChar();  // the block separator: pulls the next block into this one
            cursor.insertText(QString(QChar(QChar::LineSeparator)));
            block = doc->findBlock(cursor.position());
        }
    }
    cursor.endEditBlock();
}

inline void setMarkdownWithLineBreaks(QTextDocument* doc, const QString& markdown)
{
    if (!doc) {
        return;
    }
    doc->setMarkdown(markdownWithHardLineBreaks(markdown));
    mergeHardLineBreaks(doc);
}

}  // namespace mycel
