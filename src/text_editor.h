#pragma once

// The standalone text editor widget and its dialog wrapper. Extracted verbatim out of main.cpp.

#include <QtCore/QDir>
#include <QtCore/QDateTime>
#include <QtCore/QEvent>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QIODevice>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtGui/QKeyEvent>
#include <QtGui/QKeySequence>
#include <QtGui/QShortcut>
#include <QtGui/QTextBlock>
#include <QtGui/QTextCursor>
#include <QtGui/QWheelEvent>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "text_font_settings.h"

class TextEditor final : public QPlainTextEdit {
public:
    explicit TextEditor(QWidget* parent = nullptr) : QPlainTextEdit(parent)
    {
        TextFontSettings::registerEditor(this);
    }

    ~TextEditor() override
    {
        TextFontSettings::unregisterEditor(this);
    }

protected:
    bool event(QEvent* event) override
    {
        if (handleTextZoomGesture(event, accumulatedZoom_, TextFontSettings::Surface::Editor)) {
            return true;
        }
        return QPlainTextEdit::event(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (handleTextZoomKey(event, TextFontSettings::Surface::Editor)) {
            return;
        }
        if (!isReadOnly()) {
            const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
            if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
                (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier)) {
                // Shift+Enter is the escape hatch: plain newline with indent, no list continuation.
                insertNewlineWithIndent(modifiers == Qt::NoModifier);
                return;
            }
            if (event->key() == Qt::Key_Backtab) {
                changeLineIndent(false);
                return;
            }
            if (event->key() == Qt::Key_Tab && modifiers == Qt::NoModifier && selectionSpansMultipleLines()) {
                changeLineIndent(true);
                return;
            }
        }
        QPlainTextEdit::keyPressEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (event->modifiers() & Qt::ControlModifier) {
            const QPoint delta = event->angleDelta();
            const QPoint pixelDelta = event->pixelDelta();
            const int amount = !delta.isNull() ? delta.y() : pixelDelta.y();
            if (amount != 0) {
                TextFontSettings::changeConfiguredPointSize(TextFontSettings::Surface::Editor, amount > 0 ? 1 : -1);
            }
            event->accept();
            return;
        }
        QPlainTextEdit::wheelEvent(event);
    }

private:
    // Enter: keep the current line's indentation and continue Markdown-style list markers
    // (bullet, checkbox, numbered, quote). Enter on an empty item ends the list instead.
    void insertNewlineWithIndent(bool continueMarkers)
    {
        QTextCursor cursor = textCursor();
        const QString line = cursor.block().text();
        QString indent;
        for (const QChar ch : line) {
            if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t')) {
                break;
            }
            indent += ch;
        }

        QString marker;
        bool emptyItem = false;
        int contentStart = -1;
        if (continueMarkers) {
            static const QRegularExpression checkboxRe(
                QStringLiteral(R"(^[ \t]*([-*+])(\s+)\[[ xX]\]([ \t]*)(.*)$)"));
            static const QRegularExpression numberRe(QStringLiteral(R"(^[ \t]*(\d+)([.)])(\s+)(.*)$)"));
            static const QRegularExpression bulletRe(QStringLiteral(R"(^[ \t]*([-*+])(\s+)(.*)$)"));
            static const QRegularExpression quoteRe(QStringLiteral(R"(^[ \t]*(>)([ \t]*)(.*)$)"));
            QRegularExpressionMatch match;
            if ((match = checkboxRe.match(line)).hasMatch()) {
                marker = match.captured(1) + match.captured(2) + QStringLiteral("[ ]") +
                         (match.captured(3).isEmpty() ? QStringLiteral(" ") : match.captured(3));
                emptyItem = match.captured(4).trimmed().isEmpty();
                contentStart = match.capturedStart(4);
            } else if ((match = numberRe.match(line)).hasMatch()) {
                marker = QString::number(match.captured(1).toLongLong() + 1) + match.captured(2) + match.captured(3);
                emptyItem = match.captured(4).trimmed().isEmpty();
                contentStart = match.capturedStart(4);
            } else if ((match = bulletRe.match(line)).hasMatch()) {
                marker = match.captured(1) + match.captured(2);
                emptyItem = match.captured(3).trimmed().isEmpty();
                contentStart = match.capturedStart(3);
            } else if ((match = quoteRe.match(line)).hasMatch()) {
                marker = match.captured(1) +
                         (match.captured(2).isEmpty() ? QStringLiteral(" ") : match.captured(2));
                emptyItem = match.captured(3).trimmed().isEmpty();
                contentStart = match.capturedStart(3);
            }
            if (!marker.isEmpty() && cursor.positionInBlock() < contentStart) {
                marker.clear();  // splitting before the marker: plain indented newline
                emptyItem = false;
            }
        }

        cursor.beginEditBlock();
        if (!marker.isEmpty() && emptyItem) {
            // Enter on an empty list item ends the list: clear the line instead of continuing.
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
        } else {
            cursor.insertText(QStringLiteral("\n") + indent + marker);
        }
        cursor.endEditBlock();
        setTextCursor(cursor);
        ensureCursorVisible();
    }

    bool selectionSpansMultipleLines() const
    {
        const QTextCursor cursor = textCursor();
        return cursor.hasSelection() &&
               document()->findBlock(cursor.selectionStart()).blockNumber() !=
                   document()->findBlock(cursor.selectionEnd()).blockNumber();
    }

    // Tab with a multi-line selection / Shift+Tab: indent or outdent whole lines by one tab
    // (outdent also eats up to 4 leading spaces so space-indented lists work).
    void changeLineIndent(bool increase)
    {
        QTextCursor cursor = textCursor();
        const int selectionStart = cursor.selectionStart();
        const int selectionEnd = cursor.selectionEnd();
        const QTextBlock firstBlock = document()->findBlock(selectionStart);
        QTextBlock endBlock = document()->findBlock(selectionEnd);
        if (cursor.hasSelection() && endBlock != firstBlock && endBlock.position() == selectionEnd) {
            endBlock = endBlock.previous();  // a selection ending at a line start excludes that line
        }

        cursor.beginEditBlock();
        for (QTextBlock block = firstBlock; block.isValid(); block = block.next()) {
            QTextCursor lineCursor(block);
            if (increase) {
                if (!block.text().isEmpty()) {
                    lineCursor.insertText(QStringLiteral("\t"));
                }
            } else {
                const QString text = block.text();
                int remove = 0;
                if (text.startsWith(QLatin1Char('\t'))) {
                    remove = 1;
                } else {
                    while (remove < 4 && remove < text.size() && text.at(remove) == QLatin1Char(' ')) {
                        ++remove;
                    }
                }
                if (remove > 0) {
                    lineCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, remove);
                    lineCursor.removeSelectedText();
                }
            }
            if (block == endBlock) {
                break;
            }
        }
        cursor.endEditBlock();
    }

    qreal accumulatedZoom_ = 0.0;
};

class TextEditorDialog final : public QDialog {
public:
    explicit TextEditorDialog(const QString& filePath, QWidget* parent = nullptr)
        : QDialog(parent), filePath_(filePath)
    {
        // The trailing [*] placeholder is required by setWindowModified() (called from
        // updateStatus()); Qt substitutes it with the platform's unsaved-change marker.
        setWindowTitle(QStringLiteral("Mycel Editor - %1[*]").arg(QFileInfo(filePath_).fileName()));
        resize(860, 620);

        auto* layout = new QVBoxLayout(this);
        pathLabel_ = new QLabel(QDir::toNativeSeparators(filePath_), this);
        pathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(pathLabel_);

        editor_ = new TextEditor(this);
        layout->addWidget(editor_, 1);

        auto* buttonRow = new QHBoxLayout();
        statusLabel_ = new QLabel(this);
        buttonRow->addWidget(statusLabel_, 1);

        saveButton_ = new QPushButton(QStringLiteral("保存"), this);
        closeButton_ = new QPushButton(QStringLiteral("閉じる"), this);
        buttonRow->addWidget(saveButton_);
        buttonRow->addWidget(closeButton_);
        layout->addLayout(buttonRow);

        if (!loadFile()) {
            editor_->setReadOnly(true);
            saveButton_->setEnabled(false);
        }

        connect(editor_, &QPlainTextEdit::textChanged, this, [this] {
            if (loading_) {
                return;
            }
            modified_ = true;
            updateStatus();
        });
        connect(saveButton_, &QPushButton::clicked, this, [this] { saveFile(); });
        connect(closeButton_, &QPushButton::clicked, this, &QWidget::close);

        auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
        connect(saveShortcut, &QShortcut::activated, this, [this] { saveFile(); });

        auto* closeShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_W), this);
        connect(closeShortcut, &QShortcut::activated, this, [this] { close(); });

        // Ctrl+Enter saves and closes; both the main Return key and the keypad Enter count.
        auto* saveCloseShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
        connect(saveCloseShortcut, &QShortcut::activated, this, [this] { saveAndClose(); });
        auto* saveCloseEnterShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), this);
        connect(saveCloseEnterShortcut, &QShortcut::activated, this, [this] { saveAndClose(); });

        updateStatus();
    }

    bool wasSaved() const
    {
        return saved_;
    }

protected:
    void reject() override
    {
        close();
    }

    void closeEvent(QCloseEvent* event) override
    {
        if (!modified_) {
            event->accept();
            return;
        }

        const QMessageBox::StandardButton result = QMessageBox::question(
            this,
            QStringLiteral("Mycel"),
            QStringLiteral("変更を保存しますか？"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if (result == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if (result == QMessageBox::Save && !saveFile()) {
            event->ignore();
            return;
        }
        event->accept();
    }

private:
    bool loadFile()
    {
        QFile file(filePath_);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("ファイルを開けませんでした。"));
            return false;
        }

        loading_ = true;
        editor_->setPlainText(QString::fromUtf8(file.readAll()));
        loading_ = false;
        modified_ = false;
        loadedLastModified_ = QFileInfo(filePath_).lastModified();
        return true;
    }

    bool saveFile()
    {
        const QFileInfo info(filePath_);
        if (loadedLastModified_.isValid() && info.exists() && info.lastModified() > loadedLastModified_) {
            const QMessageBox::StandardButton result = QMessageBox::question(
                this,
                QStringLiteral("Mycel"),
                QStringLiteral("ファイルが外部で変更されています。上書きしますか？"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (result != QMessageBox::Yes) {
                return false;
            }
        }

        QFile file(filePath_);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QMessageBox::warning(this, QStringLiteral("Mycel"), QStringLiteral("ファイルを保存できませんでした。"));
            return false;
        }

        file.write(editor_->toPlainText().toUtf8());
        file.close();
        loadedLastModified_ = QFileInfo(filePath_).lastModified();
        modified_ = false;
        saved_ = true;
        updateStatus();
        return true;
    }

    void saveAndClose()
    {
        if (!editor_->isReadOnly() && modified_ && !saveFile()) {
            return;  // save failed or was declined: keep the dialog open
        }
        close();
    }

    void updateStatus()
    {
        statusLabel_->setText(modified_ ? QStringLiteral("未保存") : QStringLiteral("保存済み"));
        saveButton_->setEnabled(!editor_->isReadOnly() && modified_);
        setWindowModified(modified_);
    }

    QString filePath_;
    QLabel* pathLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    TextEditor* editor_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    QPushButton* closeButton_ = nullptr;
    QDateTime loadedLastModified_;
    bool loading_ = false;
    bool modified_ = false;
    bool saved_ = false;
};
