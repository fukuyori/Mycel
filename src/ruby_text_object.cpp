#include "ruby_text_object.h"

#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QFont>
#include <QtGui/QFontMetricsF>
#include <QtGui/QPainter>
#include <QtGui/QTextDocument>

#include <algorithm>

namespace mycel {

namespace {

struct RubyFonts {
    QFont base;
    QFont reading;
};

RubyFonts fontsFor(const QTextDocument* doc, const QTextFormat& format)
{
    RubyFonts fonts;
    const QTextCharFormat charFormat = format.toCharFormat();
    // The char format only names the properties it sets; everything else comes from the document.
    fonts.base = charFormat.font().resolve(doc->defaultFont());
    fonts.reading = fonts.base;
    if (fonts.reading.pointSizeF() > 0) {
        fonts.reading.setPointSizeF(fonts.reading.pointSizeF() * RubyTextObject::kReadingScale);
    } else if (fonts.reading.pixelSize() > 0) {
        fonts.reading.setPixelSize(
            std::max(1, int(fonts.reading.pixelSize() * RubyTextObject::kReadingScale)));
    }
    fonts.reading.setBold(false);
    return fonts;
}

}  // namespace

RubyTextObject::RubyTextObject(QObject* parent) : QObject(parent) {}

void RubyTextObject::ensureRegistered(QTextDocument* doc)
{
    if (!doc) {
        return;
    }
    QAbstractTextDocumentLayout* layout = doc->documentLayout();
    if (!layout || layout->handlerForObject(kObjectType)) {
        return;
    }
    layout->registerHandler(kObjectType, new RubyTextObject(doc));
}

QTextCharFormat RubyTextObject::makeFormat(const QTextCharFormat& inherited, const QString& base,
                                           const QString& reading)
{
    QTextCharFormat format = inherited;
    format.setObjectType(kObjectType);
    format.setProperty(BaseText, base);
    format.setProperty(ReadingText, reading);
    return format;
}

namespace {

// Geometry of one ruby, measured from the ink rather than the font metrics: Japanese fonts carry
// a large internal leading above the glyphs, so placing the reading at the font ascent would leave
// a visible gap between the two lines. Distances are relative to the base baseline (positive up).
struct RubyLayout {
    qreal width = 0;
    qreal baseAdvance = 0;
    qreal readingAdvance = 0;
    qreal readingBaseline = 0;  // how far above the base baseline the reading's baseline sits
    qreal height = 0;           // extent above the baseline (Qt treats it as the object's ascent)
};

RubyLayout layoutFor(const RubyFonts& fonts, const QString& base, const QString& reading)
{
    const QFontMetricsF baseMetrics(fonts.base);
    const QFontMetricsF readingMetrics(fonts.reading);
    RubyLayout layout;
    layout.baseAdvance = baseMetrics.horizontalAdvance(base);
    layout.readingAdvance = readingMetrics.horizontalAdvance(reading);
    layout.width = std::max(layout.baseAdvance, layout.readingAdvance);

    // Ink top of the base (fallback to a typical CJK glyph height when there is no ink).
    const QRectF baseInk = baseMetrics.tightBoundingRect(base);
    const qreal baseInkTop = baseInk.isEmpty() ? baseMetrics.ascent() * 0.8 : -baseInk.top();
    // Ink extent of the reading around its own baseline.
    const QRectF readingInk = readingMetrics.tightBoundingRect(reading);
    const qreal readingBelow = readingInk.isEmpty() ? 0.0 : std::max<qreal>(0.0, readingInk.bottom());
    const qreal readingAbove = readingInk.isEmpty() ? readingMetrics.ascent() * 0.8 : -readingInk.top();

    const qreal gap = std::max<qreal>(1.0, baseMetrics.height() * 0.06);
    layout.readingBaseline = baseInkTop + gap + readingBelow;
    // Never shorter than the base ascent, so a line made only of ruby keeps its normal height.
    layout.height = std::max(baseMetrics.ascent(), layout.readingBaseline + readingAbove + 1.0);
    return layout;
}

}  // namespace

QSizeF RubyTextObject::intrinsicSize(QTextDocument* doc, int posInDocument, const QTextFormat& format)
{
    Q_UNUSED(posInDocument);
    const RubyFonts fonts = fontsFor(doc, format);
    const RubyLayout layout =
        layoutFor(fonts, format.stringProperty(BaseText), format.stringProperty(ReadingText));
    // The object sits on the baseline; the base's descenders hang below the rect as usual.
    return QSizeF(layout.width, layout.height);
}

void RubyTextObject::drawObject(QPainter* painter, const QRectF& rect, QTextDocument* doc,
                                int posInDocument, const QTextFormat& format)
{
    Q_UNUSED(posInDocument);
    const RubyFonts fonts = fontsFor(doc, format);
    const QString base = format.stringProperty(BaseText);
    const QString reading = format.stringProperty(ReadingText);
    const RubyLayout layout = layoutFor(fonts, base, reading);

    painter->save();
    const QTextCharFormat charFormat = format.toCharFormat();
    if (charFormat.foreground().style() != Qt::NoBrush) {
        painter->setPen(charFormat.foreground().color());
    }
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const qreal baseline = rect.bottom();
    painter->setFont(fonts.base);
    painter->drawText(QPointF(rect.left() + (rect.width() - layout.baseAdvance) / 2.0, baseline), base);

    painter->setFont(fonts.reading);
    painter->drawText(QPointF(rect.left() + (rect.width() - layout.readingAdvance) / 2.0,
                              baseline - layout.readingBaseline),
                      reading);
    painter->restore();
}

}  // namespace mycel
