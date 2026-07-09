#pragma once

// The standalone text editor widget and its dialog wrapper. Extracted verbatim out of main.cpp.

#include <QtCore/QDir>
#include <QtCore/QDateTime>
#include <QtCore/QEvent>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QIODevice>
#include <QtCore/QString>
#include <QtGui/QKeyEvent>
#include <QtGui/QKeySequence>
#include <QtGui/QShortcut>
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
