#pragma once

// Shared editor/preview font-size settings, plus the Ctrl+wheel / Ctrl+/- zoom handlers.
// Extracted verbatim out of main.cpp.

#include <QtCore/QEvent>
#include <QtCore/QSettings>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtGui/QFont>
#include <QtGui/QKeyEvent>
#include <QtGui/QNativeGestureEvent>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QTextEdit>

#include <algorithm>
#include <vector>

class TextFontSettings {
public:
    enum class Surface {
        Editor,
        Preview,
    };

    static int configuredPointSize(Surface surface)
    {
        QSettings settings;
        const QString key = surface == Surface::Editor
                                ? QStringLiteral("editor/fontPointSize")
                                : QStringLiteral("preview/fontPointSize");
        return std::clamp(settings.value(key, settings.value(QStringLiteral("editor/fontPointSize"), DefaultPointSize)).toInt(),
                          MinPointSize,
                          MaxPointSize);
    }

    static QFont editorFont(int pointSize)
    {
        QFont font(QStringLiteral("Consolas"));
        font.setStyleHint(QFont::Monospace);
        font.setPointSize(pointSize);
        return font;
    }

    static QFont previewFont(int pointSize)
    {
        QFont font(QStringLiteral("Meiryo"));
        font.setStyleHint(QFont::SansSerif);
        font.setPointSize(pointSize);
        return font;
    }

    static void registerEditor(QPlainTextEdit* editor)
    {
        editors().push_back(editor);
        applyToEditor(editor);
    }

    static void unregisterEditor(QPlainTextEdit* editor)
    {
        auto& registered = editors();
        registered.erase(std::remove(registered.begin(), registered.end(), editor), registered.end());
    }

    static void registerPreview(QTextEdit* preview)
    {
        previews().push_back(preview);
        applyToPreview(preview);
    }

    static void unregisterPreview(QTextEdit* preview)
    {
        auto& registered = previews();
        registered.erase(std::remove(registered.begin(), registered.end(), preview), registered.end());
    }

    static void changeConfiguredPointSize(Surface surface, int delta)
    {
        setConfiguredPointSize(surface, configuredPointSize(surface) + delta);
    }

    static void resetConfiguredPointSize(Surface surface)
    {
        setConfiguredPointSize(surface, DefaultPointSize);
    }

private:
    static constexpr int DefaultPointSize = 10;
    static constexpr int MinPointSize = 7;
    static constexpr int MaxPointSize = 32;

    static std::vector<QPlainTextEdit*>& editors()
    {
        static std::vector<QPlainTextEdit*> registered;
        return registered;
    }

    static std::vector<QTextEdit*>& previews()
    {
        static std::vector<QTextEdit*> registered;
        return registered;
    }

    static void setConfiguredPointSize(Surface surface, int pointSize)
    {
        const int clampedPointSize = std::clamp(pointSize, MinPointSize, MaxPointSize);
        QSettings settings;
        settings.setValue(surface == Surface::Editor
                              ? QStringLiteral("editor/fontPointSize")
                              : QStringLiteral("preview/fontPointSize"),
                          clampedPointSize);
        settings.sync();
        if (surface == Surface::Editor) {
            for (QPlainTextEdit* editor : editors()) {
                applyToEditor(editor);
            }
        } else {
            for (QTextEdit* preview : previews()) {
                applyToPreview(preview);
            }
        }
    }

    static void applyToEditor(QPlainTextEdit* editor)
    {
        if (editor) {
            editor->setFont(editorFont(configuredPointSize(Surface::Editor)));
        }
    }

    static void applyToPreview(QTextEdit* preview)
    {
        if (preview) {
            preview->setFont(previewFont(configuredPointSize(Surface::Preview)));
        }
    }
};

bool handleTextZoomKey(QKeyEvent* event, TextFontSettings::Surface surface)
{
    if (!event || !(event->modifiers() & Qt::ControlModifier)) {
        return false;
    }

    switch (event->key()) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        TextFontSettings::changeConfiguredPointSize(surface, 1);
        event->accept();
        return true;
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        TextFontSettings::changeConfiguredPointSize(surface, -1);
        event->accept();
        return true;
    case Qt::Key_0:
        TextFontSettings::resetConfiguredPointSize(surface);
        event->accept();
        return true;
    default:
        return false;
    }
}

bool handleTextZoomGesture(QEvent* event, qreal& accumulatedZoom, TextFontSettings::Surface surface)
{
    if (!event || event->type() != QEvent::NativeGesture) {
        return false;
    }

    auto* gesture = static_cast<QNativeGestureEvent*>(event);
    if (gesture->gestureType() != Qt::ZoomNativeGesture) {
        return false;
    }

    accumulatedZoom += gesture->value();
    if (std::abs(accumulatedZoom) >= 0.08) {
        TextFontSettings::changeConfiguredPointSize(surface, accumulatedZoom > 0.0 ? 1 : -1);
        accumulatedZoom = 0.0;
    }
    event->accept();
    return true;
}
