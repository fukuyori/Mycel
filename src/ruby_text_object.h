#pragma once

#include <QtCore/QObject>
#include <QtCore/QSizeF>
#include <QtCore/QString>
#include <QtGui/QTextCharFormat>
#include <QtGui/QTextFormat>
#include <QtGui/QTextObjectInterface>

class QPainter;
class QTextDocument;

namespace mycel {

// Inline ruby (furigana) for QTextDocument. Qt's rich text has no way to stack a reading over its
// base text, so the ruby is inserted as an inline object (U+FFFC) whose char format carries the
// base and the reading, and this handler draws the two lines itself: the reading, at half size,
// centred above the base. The object's intrinsic height includes the reading, so the line grows
// to make room, exactly like <ruby> in a browser.
class RubyTextObject : public QObject, public QTextObjectInterface
{
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    static constexpr int kObjectType = QTextFormat::UserObject + 1;
    enum Property {
        BaseText = QTextFormat::UserProperty + 1,
        ReadingText = QTextFormat::UserProperty + 2,
    };
    // Reading font size relative to the base font.
    static constexpr qreal kReadingScale = 0.5;

    explicit RubyTextObject(QObject* parent = nullptr);

    // Registers a handler on the document's layout once (later calls are no-ops).
    static void ensureRegistered(QTextDocument* doc);

    // Char format for one ruby object, inheriting font/colour from `inherited`.
    static QTextCharFormat makeFormat(const QTextCharFormat& inherited, const QString& base,
                                      const QString& reading);

    QSizeF intrinsicSize(QTextDocument* doc, int posInDocument, const QTextFormat& format) override;
    void drawObject(QPainter* painter, const QRectF& rect, QTextDocument* doc, int posInDocument,
                    const QTextFormat& format) override;
};

}  // namespace mycel
