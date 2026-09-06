#include "markdown_line_breaks.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QTextBlock>
#include <QtGui/QTextDocument>
#include <QtGui/QTextList>

#include <iostream>

namespace {

// "⏎" in an expected string stands for the inserted break marker (sentinel + backslash).
QString expand(const QString& expected)
{
    QString s = expected;
    s.replace(QChar(0x23CE), QString(QChar(mycel::kLineBreakSentinel)) + QLatin1Char('\\'));
    return s;
}

QString visible(const QString& s)
{
    QString v = s;
    v.replace(QChar(mycel::kLineBreakSentinel), QStringLiteral("<ZWSP>"));
    v.replace(QChar(QChar::LineSeparator), QStringLiteral("<LS>"));
    v.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    v.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
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

bool check(const char* label, const char* input, const char* expected)
{
    return expectEqual(mycel::markdownWithHardLineBreaks(QString::fromUtf8(input)),
                       expand(QString::fromUtf8(expected)), label);
}

QString blocksOf(const QTextDocument& doc)
{
    QStringList parts;
    for (QTextBlock b = doc.begin(); b.isValid(); b = b.next()) {
        parts << (b.textList() ? QStringLiteral("[list]") : QString()) + visible(b.text());
    }
    return parts.join(QStringLiteral(" | "));
}

bool checkDocument(const char* label, const char* markdown, const QString& expectedBlocks)
{
    QTextDocument doc;
    mycel::setMarkdownWithLineBreaks(&doc, QString::fromUtf8(markdown));
    return expectEqual(blocksOf(doc), expectedBlocks, label);
}

}  // namespace

int main(int argc, char** argv)
{
    // QTextDocument needs an application for font metrics. On Windows the default platform works
    // without a visible window; elsewhere fall back to the offscreen plugin for headless runs.
#ifndef Q_OS_WIN
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
#endif
    QGuiApplication app(argc, argv);

    bool ok = true;

    // 1) The preprocessor: where the break marker is (and is not) inserted.
    ok &= check("two lines", "a\nb", "a⏎\nb");
    ok &= check("three lines", "a\nb\nc", "a⏎\nb⏎\nc");
    ok &= check("paragraph break", "a\n\nb", "a\n\nb");
    ok &= check("crlf", "a\r\nb\r\n", "a⏎\r\nb\r\n");
    ok &= check("already hard (backslash)", "a\\\nb", "a\\\nb");
    ok &= check("already hard (spaces)", "a  \nb", "a  \nb");
    ok &= check("before heading", "a\n# h", "a\n# h");
    ok &= check("heading itself", "# h\na", "# h\na");
    ok &= check("before list", "a\n- x", "a\n- x");
    ok &= check("list items", "- x\n- y", "- x\n- y");
    ok &= check("list continuation", "- x\ncont", "- x⏎\ncont");
    ok &= check("ordered list", "1. x\n2. y", "1. x\n2. y");
    ok &= check("before rule", "a\n---", "a\n---");
    ok &= check("before quote", "a\n> q", "a\n> q");
    ok &= check("inside quote", "> a\n> b", "> a⏎\n> b");
    ok &= check("quote then lazy line", "> a\nb", "> a⏎\nb");
    ok &= check("fenced code", "```\na\nb\n```\nc", "```\na\nb\n```\nc");
    ok &= check("tilde fence", "~~~\na\nb\n~~~", "~~~\na\nb\n~~~");
    ok &= check("text before fence", "a\n```\nb\n```", "a\n```\nb\n```");
    ok &= check("indented code", "x\n\n    a\n    b\ny", "x\n\n    a\n    b\ny");
    ok &= check("indented continuation", "a\n    b", "a⏎\n    b");
    ok &= check("table", "| a | b |\n|---|---|\n| 1 | 2 |\nafter", "| a | b |\n|---|---|\n| 1 | 2 |\nafter");
    ok &= check("text before table", "t\n| a |\n|---|", "t\n| a |\n|---|");
    ok &= check("html block", "<div>\na\n</div>", "<div>\na\n</div>");
    ok &= check("japanese", "吾輩は猫である。\n名前はまだ無い。", "吾輩は猫である。⏎\n名前はまだ無い。");

    // 2) The imported document: broken lines share one block (joined by a line separator, no
    //    paragraph spacing), paragraphs stay separate blocks, and a broken list item stays one item.
    ok &= checkDocument("doc: two lines", "a\nb", QStringLiteral("a<LS>b"));
    ok &= checkDocument("doc: three lines", "a\nb\nc", QStringLiteral("a<LS>b<LS>c"));
    ok &= checkDocument("doc: paragraphs", "a\n\nb", QStringLiteral("a | b"));
    ok &= checkDocument("doc: mixed", "a\nb\n\nc", QStringLiteral("a<LS>b | c"));
    ok &= checkDocument("doc: list item continuation", "- x\ncont\n- y",
                        QStringLiteral("[list]x<LS>cont | [list]y"));
    ok &= checkDocument("doc: quote", "> a\n> b", QStringLiteral("a<LS>b"));
    ok &= checkDocument("doc: no sentinel left", "a\nb", QStringLiteral("a<LS>b"));
    ok &= checkDocument("doc: japanese", "吾輩は猫である。\n名前はまだ無い。",
                        QStringLiteral("吾輩は猫である。<LS>名前はまだ無い。"));

    if (!ok) {
        return 1;
    }
    std::cout << "markdown_line_breaks tests passed\n";
    return 0;
}
