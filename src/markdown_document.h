#pragma once

// Markdown → QTextDocument for the lightweight (QTextEdit) preview path: the "newline = line
// break" rule from markdown_line_breaks.h plus text-only renditions of the two Markdown extensions
// that the QtWebEngine renderer draws natively:
//
// - GitHub Alerts ("> [!NOTE]" ...): the [!TYPE] line becomes a bold, accent-coloured title and
//   the whole quote gets a tinted background. QTextDocument has no <aside>, so this is the closest
//   equivalent of GitHub's box.
// - Aozora Bunko ruby (漢字《かんじ》 / ｜文字列《よみ》): drawn as an inline object (RubyTextObject)
//   that paints the reading above the base, since Qt's rich text has no ruby of its own. Inline
//   code and fenced code keep the source verbatim, like the web renderer.
//
// The ruby is carried through Qt's Markdown importer with the Unicode interlinear annotation
// characters (U+FFF9 anchor / U+FFFA separator / U+FFFB terminator), which are exactly meant for
// this and are invisible should one ever survive.

#include "markdown_line_breaks.h"
#include "ruby_text_object.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QColor>
#include <QtGui/QTextBlock>
#include <QtGui/QTextCharFormat>
#include <QtGui/QTextCursor>
#include <QtGui/QTextDocument>

namespace mycel {

inline constexpr char16_t kRubyAnchor = 0xFFF9;
inline constexpr char16_t kRubySeparator = 0xFFFA;
inline constexpr char16_t kRubyTerminator = 0xFFFB;

// GitHub's alert palette. `rgb` values are "r,g,b" so the web renderer can build rgb()/rgba().
struct MarkdownAlertColor {
    const char* type;   // lower-case alert type, also the CSS class suffix
    const char* title;  // title shown above the body
    const char* light;
    const char* dark;
};

inline const MarkdownAlertColor* markdownAlertColors(int* count)
{
    static const MarkdownAlertColor colors[] = {
        {"note", "Note", "9,105,218", "68,147,248"},
        {"tip", "Tip", "26,127,55", "63,185,80"},
        {"important", "Important", "130,80,223", "171,125,248"},
        {"warning", "Warning", "154,103,0", "210,153,34"},
        {"caution", "Caution", "207,34,46", "248,81,73"},
    };
    if (count) {
        *count = int(sizeof(colors) / sizeof(colors[0]));
    }
    return colors;
}

inline const MarkdownAlertColor* markdownAlertColor(const QString& upperType)
{
    int count = 0;
    const MarkdownAlertColor* colors = markdownAlertColors(&count);
    for (int i = 0; i < count; ++i) {
        if (upperType.compare(QLatin1String(colors[i].type), Qt::CaseInsensitive) == 0) {
            return &colors[i];
        }
    }
    return nullptr;
}

inline QColor markdownAlertAccent(const MarkdownAlertColor& color, bool dark)
{
    const QStringList parts = QString::fromLatin1(dark ? color.dark : color.light).split(QLatin1Char(','));
    return QColor(parts.value(0).toInt(), parts.value(1).toInt(), parts.value(2).toInt());
}

namespace detail {

inline bool isAlertMarker(const QString& content)
{
    static const QRegularExpression marker(
        QStringLiteral("^\\s*\\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\\]\\s*$"));
    return marker.match(content).hasMatch();
}

inline bool isRubyBaseChar(QChar ch)
{
    static const QString extras = QStringLiteral("々仝〆〇ヶ");
    return ch.script() == QChar::Script_Han || extras.contains(ch);
}

// Rewrites the ruby notation in one run of plain text (no code spans) into annotation characters.
inline QString rubyToAnnotations(const QString& text)
{
    // Explicit form first: ｜base《reading》 (half-width bar accepted; nearest bar wins).
    static const QRegularExpression explicitRuby(
        QStringLiteral("[｜|]([^《》｜|\\r\\n]+)《([^《》\\r\\n]+)》"));
    QString out;
    out.reserve(text.size());
    int pos = 0;
    for (;;) {
        const QRegularExpressionMatch m = explicitRuby.match(text, pos);
        if (!m.hasMatch()) {
            out += QStringView(text).mid(pos);
            break;
        }
        out += QStringView(text).mid(pos, m.capturedStart() - pos);
        out += QChar(kRubyAnchor) + m.captured(1) + QChar(kRubySeparator) + m.captured(2) +
               QChar(kRubyTerminator);
        pos = m.capturedEnd();
    }

    // Shorthand: the run of kanji right before 《reading》.
    static const QRegularExpression shorthand(QStringLiteral("《([^《》\\r\\n]+)》"));
    QString result;
    result.reserve(out.size());
    pos = 0;
    for (;;) {
        const QRegularExpressionMatch m = shorthand.match(out, pos);
        if (!m.hasMatch()) {
            result += QStringView(out).mid(pos);
            break;
        }
        // Walk back over the kanji run inside the part not yet copied. Stop at an annotation
        // terminator so an explicit ruby is never absorbed.
        int start = m.capturedStart();
        int baseStart = start;
        while (baseStart > pos && isRubyBaseChar(out.at(baseStart - 1))) {
            --baseStart;
        }
        if (baseStart == start) {
            // No kanji before 《: plain text.
            result += QStringView(out).mid(pos, m.capturedEnd() - pos);
        } else {
            result += QStringView(out).mid(pos, baseStart - pos);
            result += QChar(kRubyAnchor) + out.mid(baseStart, start - baseStart) + QChar(kRubySeparator) +
                      m.captured(1) + QChar(kRubyTerminator);
        }
        pos = m.capturedEnd();
    }
    return result;
}

// Applies rubyToAnnotations() to the parts of a line outside `code spans`.
inline QString rubyLine(const QString& line)
{
    static const QRegularExpression codeSpan(QStringLiteral("`+[^`]*`+"));
    QString out;
    int pos = 0;
    for (;;) {
        const QRegularExpressionMatch m = codeSpan.match(line, pos);
        if (!m.hasMatch()) {
            out += rubyToAnnotations(line.mid(pos));
            break;
        }
        out += rubyToAnnotations(line.mid(pos, m.capturedStart() - pos));
        out += m.captured(0);
        pos = m.capturedEnd();
    }
    return out;
}

}  // namespace detail

// Source-level pass: ruby → annotation characters (outside code), then the hard line breaks.
inline QString prepareMarkdownSource(const QString& markdown)
{
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    QStringList out;
    out.reserve(lines.size());
    bool inFence = false;
    QString fenceMarker;
    for (const QString& line : lines) {
        QString body = line;
        if (body.endsWith(QLatin1Char('\r'))) {
            body.chop(1);
        }
        int depth = 0;
        const QString content = detail::withoutQuoteMarkers(body, &depth);
        if (inFence) {
            if (detail::closesFence(content, fenceMarker)) {
                inFence = false;
            }
            out.append(line);
            continue;
        }
        if (detail::isFenceLine(content, &fenceMarker)) {
            inFence = true;
            out.append(line);
            continue;
        }
        // An alert marker gets its own paragraph (an empty quote line after it), so the title
        // is a separate block that styleAlerts() can recognise and restyle.
        if (depth > 0 && detail::isAlertMarker(content)) {
            out.append(line);
            out.append(QString(depth, QLatin1Char('>')));
            continue;
        }
        if (!line.contains(QChar(0x300A))) {  // 《
            out.append(line);
            continue;
        }
        const bool cr = line.endsWith(QLatin1Char('\r'));
        QString converted = detail::rubyLine(body);
        if (cr) {
            converted.append(QLatin1Char('\r'));
        }
        out.append(converted);
    }
    return markdownWithHardLineBreaks(out.join(QLatin1Char('\n')));
}

// Turns the annotation characters left by prepareMarkdownSource() into ruby objects: each
// "anchor base separator reading terminator" run becomes one inline RubyTextObject that draws
// the reading above the base. The object inherits the font and colour of the base text.
inline void styleRubyAnnotations(QTextDocument* doc)
{
    if (!doc) {
        return;
    }
    RubyTextObject::ensureRegistered(doc);
    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        for (;;) {
            const QString text = block.text();
            const int anchor = text.indexOf(QChar(kRubyAnchor));
            if (anchor < 0) {
                break;
            }
            const int separator = text.indexOf(QChar(kRubySeparator), anchor + 1);
            const int terminator = separator < 0 ? -1 : text.indexOf(QChar(kRubyTerminator), separator + 1);
            const int position = block.position();
            if (separator < 0 || terminator < 0) {
                cursor.setPosition(position + anchor);
                cursor.deleteChar();  // stray anchor: drop it and keep the text
                continue;
            }
            const QString baseText = text.mid(anchor + 1, separator - anchor - 1);
            const QString reading = text.mid(separator + 1, terminator - separator - 1);
            cursor.setPosition(position + anchor + 1);
            const QTextCharFormat inherited = cursor.charFormat();  // format of the base text
            cursor.setPosition(position + anchor);
            cursor.setPosition(position + terminator + 1, QTextCursor::KeepAnchor);
            cursor.insertText(QString(QChar(QChar::ObjectReplacementCharacter)),
                              RubyTextObject::makeFormat(inherited, baseText, reading));
        }
    }
    cursor.endEditBlock();
}

// Turns "[!TYPE]" quote blocks into titled, tinted alert boxes.
inline void styleAlerts(QTextDocument* doc, bool dark)
{
    if (!doc) {
        return;
    }
    static const QRegularExpression marker(
        QStringLiteral("^\\s*\\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\\]\\s*$"));
    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().intProperty(QTextFormat::BlockQuoteLevel);
        if (level <= 0) {
            continue;
        }
        const QRegularExpressionMatch m = marker.match(block.text());
        if (!m.hasMatch()) {
            continue;
        }
        const MarkdownAlertColor* color = markdownAlertColor(m.captured(1));
        if (!color) {
            continue;
        }
        const QColor accent = markdownAlertAccent(*color, dark);
        QColor tint = accent;
        tint.setAlphaF(dark ? 0.16 : 0.08);

        // Title: replace the marker text, bold in the accent colour.
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
        QTextCharFormat titleFormat;
        titleFormat.setFontWeight(QFont::Bold);
        titleFormat.setForeground(accent);
        cursor.insertText(QString::fromLatin1(color->title), titleFormat);

        // Body: every following block that is still inside this quote.
        QTextBlock last = block;
        for (QTextBlock next = block.next(); next.isValid(); next = next.next()) {
            if (next.blockFormat().intProperty(QTextFormat::BlockQuoteLevel) < level) {
                break;
            }
            last = next;
        }
        for (QTextBlock b = block; b.isValid(); b = b.next()) {
            QTextBlockFormat format = b.blockFormat();
            format.setBackground(tint);
            QTextCursor blockCursor(b);
            blockCursor.setBlockFormat(format);
            if (b == last) {
                break;
            }
        }
    }
    cursor.endEditBlock();
}

// Document-level pass after QTextDocument::setMarkdown(prepareMarkdownSource(...)).
inline void finishMarkdownDocument(QTextDocument* doc, bool dark)
{
    mergeHardLineBreaks(doc);
    styleRubyAnnotations(doc);
    styleAlerts(doc, dark);
}

inline void loadMarkdown(QTextDocument* doc, const QString& markdown, bool dark)
{
    if (!doc) {
        return;
    }
    doc->setMarkdown(prepareMarkdownSource(markdown));
    finishMarkdownDocument(doc, dark);
}

}  // namespace mycel
