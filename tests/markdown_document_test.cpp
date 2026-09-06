#include "markdown_document.h"

#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QFontMetricsF>
#include <QtGui/QGuiApplication>
#include <QtGui/QTextBlock>
#include <QtGui/QTextDocument>
#include <QtGui/QTextObjectInterface>

#include <iostream>

namespace {

QString visible(const QString& s)
{
    QString v = s;
    v.replace(QChar(mycel::kRubyAnchor), QStringLiteral("<A>"));
    v.replace(QChar(mycel::kRubySeparator), QStringLiteral("<S>"));
    v.replace(QChar(mycel::kRubyTerminator), QStringLiteral("<T>"));
    v.replace(QChar(mycel::kLineBreakSentinel), QStringLiteral("<ZWSP>"));
    v.replace(QChar(QChar::LineSeparator), QStringLiteral("<LS>"));
    v.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    return v;
}

bool expectEqual(const QString& actual, const QString& expected, const char* label)
{
    if (actual == expected) {
        return true;
    }
    std::cerr << label << " failed\n  actual:   " << visible(actual).toStdString()
              << "\n  expected: " << visible(expected).toStdString() << '\n';
    return false;
}

// Ruby rewriting of one text run (annotation characters shown as <A>base<S>reading<T>).
bool checkRuby(const char* label, const char* input, const char* expected)
{
    return expectEqual(visible(mycel::detail::rubyLine(QString::fromUtf8(input))),
                       QString::fromUtf8(expected), label);
}

// Per-block description of a loaded document: [quote]/[bg] flags, text, ruby objects as
// {base|reading}, and bold runs as <b>…</b>.
QString describe(const QTextDocument& doc)
{
    QStringList parts;
    for (QTextBlock b = doc.begin(); b.isValid(); b = b.next()) {
        QString s;
        if (b.blockFormat().intProperty(QTextFormat::BlockQuoteLevel) > 0) {
            s += QStringLiteral("[quote]");
        }
        if (b.blockFormat().background().style() != Qt::NoBrush) {
            s += QStringLiteral("[bg]");
        }
        bool bold = false;
        for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
            const QTextFragment f = it.fragment();
            const QTextCharFormat format = f.charFormat();
            const bool isBold = format.fontWeight() >= QFont::Bold;
            if (isBold != bold) {
                s += isBold ? QStringLiteral("<b>") : QStringLiteral("</b>");
                bold = isBold;
            }
            if (format.objectType() == mycel::RubyTextObject::kObjectType) {
                // One fragment per object character.
                for (int i = 0; i < f.length(); ++i) {
                    s += QStringLiteral("{%1|%2}")
                             .arg(format.stringProperty(mycel::RubyTextObject::BaseText),
                                  format.stringProperty(mycel::RubyTextObject::ReadingText));
                }
                continue;
            }
            s += f.text();
        }
        if (bold) {
            s += QStringLiteral("</b>");
        }
        parts << visible(s);
    }
    return parts.join(QStringLiteral(" | "));
}

// The ruby object must be registered on the layout and be taller than the base text alone, so
// the reading gets its own room above the line.
bool checkRubyGeometry()
{
    QTextDocument doc;
    mycel::loadMarkdown(&doc, QStringLiteral("青梅《おうめ》"), false);
    QTextObjectInterface* handler = doc.documentLayout()->handlerForObject(mycel::RubyTextObject::kObjectType);
    if (!handler) {
        std::cerr << "ruby geometry failed: no handler registered\n";
        return false;
    }
    const QTextFragment fragment = doc.begin().begin().fragment();
    const QSizeF size = handler->intrinsicSize(&doc, fragment.position(), fragment.charFormat());
    const QFontMetricsF metrics(doc.defaultFont());
    const bool tallEnough = size.height() >= metrics.ascent();
    const bool wideEnough = size.width() >= metrics.horizontalAdvance(QStringLiteral("青梅")) - 0.5;
    if (!tallEnough || !wideEnough) {
        std::cerr << "ruby geometry failed: size " << size.width() << "x" << size.height()
                  << " for ascent " << metrics.ascent() << '\n';
        return false;
    }
    return true;
}

bool checkDocument(const char* label, const char* markdown, const char* expected)
{
    QTextDocument doc;
    mycel::loadMarkdown(&doc, QString::fromUtf8(markdown), false);
    return expectEqual(describe(doc), QString::fromUtf8(expected), label);
}

}  // namespace

int main(int argc, char** argv)
{
#ifndef Q_OS_WIN
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
#endif
    QGuiApplication app(argc, argv);

    bool ok = true;

    // 1) Ruby notation → annotation characters.
    ok &= checkRuby("shorthand", "吾輩《わがはい》は猫である", "<A>吾輩<S>わがはい<T>は猫である");
    ok &= checkRuby("shorthand stops at kana", "耳まで火照《ほて》って", "耳まで<A>火照<S>ほて<T>って");
    ok &= checkRuby("repetition mark", "稍々《やや》", "<A>稍々<S>やや<T>");
    ok &= checkRuby("explicit full-width", "武州｜青梅《おうめ》の宿", "武州<A>青梅<S>おうめ<T>の宿");
    ok &= checkRuby("explicit half-width", "孤立語・|膠着語《こうちゃくご》・屈折語",
                    "孤立語・<A>膠着語<S>こうちゃくご<T>・屈折語");
    ok &= checkRuby("explicit nearest bar", "a | b|膠着語《x》 c", "a | b<A>膠着語<S>x<T> c");
    ok &= checkRuby("two rubies", "一応《いちおう》何時《いつ》もの",
                    "<A>一応<S>いちおう<T><A>何時<S>いつ<T>もの");
    ok &= checkRuby("no base", "かな《よみ》", "かな《よみ》");
    ok &= checkRuby("unclosed", "青梅《おうめ", "青梅《おうめ");
    ok &= checkRuby("explicit unclosed", "｜青梅《", "｜青梅《");
    ok &= checkRuby("inline code kept", "コード `青梅《おうめ》` と 青梅《おうめ》",
                    "コード `青梅《おうめ》` と <A>青梅<S>おうめ<T>");

    // 2) Whole documents through Qt's importer.
    ok &= checkDocument("doc: ruby object", "青梅《おうめ》は地名", "{青梅|おうめ}は地名");
    ok &= checkDocument("doc: ruby with line break", "青梅《おうめ》\n次の行",
                        "{青梅|おうめ}<LS>次の行");
    ok &= checkDocument("doc: ruby in bold", "**太字《ふとじ》**", "<b>{太字|ふとじ}</b>");
    ok &= checkDocument("doc: two rubies", "一応《いちおう》何時《いつ》もの",
                        "{一応|いちおう}{何時|いつ}もの");
    ok &= checkDocument("doc: ruby in fence kept", "```\n青梅《おうめ》\n```", "青梅《おうめ》");
    ok &= checkRubyGeometry();
    ok &= checkDocument("doc: alert", "> [!NOTE]\n> 青梅《おうめ》は地名\n> 二行目",
                        "[quote][bg]<b>Note</b> | [quote][bg]{青梅|おうめ}は地名<LS>二行目");
    ok &= checkDocument("doc: alert paragraphs then text",
                        "> [!WARNING]\n> 一つ目\n>\n> 二つ目\n\n本文",
                        "[quote][bg]<b>Warning</b> | [quote][bg]一つ目 | [quote][bg]二つ目 | 本文");
    ok &= checkDocument("doc: lower-case is a plain quote", "> [!note]\n> text",
                        "[quote][!note]<LS>text");
    ok &= checkDocument("doc: plain quote", "> a\n> b", "[quote]a<LS>b");

    if (!ok) {
        return 1;
    }
    std::cout << "markdown_document tests passed\n";
    return 0;
}
