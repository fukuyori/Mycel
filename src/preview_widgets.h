#pragma once

// Inline preview widgets embedded in a node's preview frame (text, YouTube, URL thumbnail,
// aspect-locked image, video). Extracted verbatim out of main.cpp.

#include <QtCore/QEvent>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtGui/QKeyEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtGui/QWheelEvent>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimediaWidgets/QVideoWidget>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

#include <limits>

#include "tree_model.h"
#include "text_font_settings.h"

class PreviewText final : public QTextEdit {
public:
    explicit PreviewText(QWidget* parent = nullptr) : QTextEdit(parent)
    {
        TextFontSettings::registerPreview(this);
    }

    ~PreviewText() override
    {
        TextFontSettings::unregisterPreview(this);
    }

protected:
    bool event(QEvent* event) override
    {
        if (handleTextZoomGesture(event, accumulatedZoom_, TextFontSettings::Surface::Preview)) {
            return true;
        }
        return QTextEdit::event(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (handleTextZoomKey(event, TextFontSettings::Surface::Preview)) {
            return;
        }
        QTextEdit::keyPressEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (event->modifiers() & Qt::ControlModifier) {
            const QPoint delta = event->angleDelta();
            const QPoint pixelDelta = event->pixelDelta();
            const int amount = !delta.isNull() ? delta.y() : pixelDelta.y();
            if (amount != 0) {
                TextFontSettings::changeConfiguredPointSize(TextFontSettings::Surface::Preview, amount > 0 ? 1 : -1);
            }
            event->accept();
            return;
        }
        QTextEdit::wheelEvent(event);
    }

private:
    qreal accumulatedZoom_ = 0.0;
};

class YouTubePreview final : public QWidget {
public:
    explicit YouTubePreview(const QString& embedUrl, const QString& thumbnailPath, QWidget* parent = nullptr) : QWidget(parent)
    {
        setProperty("mycelInlinePreview", true);
        setAutoFillBackground(false);
        watchUrl_ = youtubeWatchUrlFromEmbedUrl(embedUrl);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        thumbnailLabel_ = new QLabel(this);
        thumbnailLabel_->setProperty("mycelInlinePreview", true);
        thumbnailLabel_->setAlignment(Qt::AlignCenter);
        thumbnailLabel_->setMinimumSize(1, 1);
        thumbnailLabel_->setStyleSheet(QStringLiteral("background: #111827; color: #e5e7eb;"));
        thumbnailLabel_->setCursor(Qt::PointingHandCursor);
        layout->addWidget(thumbnailLabel_, 1);

        auto* footer = new QWidget(this);
        footer->setProperty("mycelInlinePreview", true);
        auto* footerLayout = new QHBoxLayout(footer);
        footerLayout->setContentsMargins(4, 0, 4, 2);
        footerLayout->addStretch(1);
        auto* openButton = new QPushButton(QStringLiteral("YouTubeで開く"), footer);
        openButton->setCursor(Qt::PointingHandCursor);
        footerLayout->addWidget(openButton);
        layout->addWidget(footer, 0);

        connect(openButton, &QPushButton::clicked, this, [this] {
            QDesktopServices::openUrl(QUrl(watchUrl_));
        });
        thumbnailLabel_->installEventFilter(this);
        originalThumbnail_.load(thumbnailPath);
        if (originalThumbnail_.isNull()) {
            thumbnailLabel_->setText(QStringLiteral("サムネイルがありません"));
        } else {
            updateThumbnailPixmap();
        }
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == thumbnailLabel_ && event->type() == QEvent::MouseButtonRelease) {
            QDesktopServices::openUrl(QUrl(watchUrl_));
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        updateThumbnailPixmap();
    }

private:
    void updateThumbnailPixmap()
    {
        if (!thumbnailLabel_ || originalThumbnail_.isNull()) {
            return;
        }
        const QSize target = thumbnailLabel_->size();
        if (target.isEmpty()) {
            return;
        }
        thumbnailLabel_->setPixmap(originalThumbnail_.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    QLabel* thumbnailLabel_ = nullptr;
    QString watchUrl_;
    QPixmap originalThumbnail_;
};

class UrlThumbnailPreview final : public QWidget {
public:
    explicit UrlThumbnailPreview(const QUrl& url, const QString& thumbnailPath, QWidget* parent = nullptr) : QWidget(parent)
    {
        setProperty("mycelInlinePreview", true);
        setAutoFillBackground(false);
        url_ = url;
        setToolTip(url_.toString());

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        thumbnailLabel_ = new QLabel(this);
        thumbnailLabel_->setProperty("mycelInlinePreview", true);
        thumbnailLabel_->setAlignment(Qt::AlignCenter);
        thumbnailLabel_->setMinimumSize(1, 1);
        thumbnailLabel_->setStyleSheet(QStringLiteral("background: #f8fafc; color: #243036;"));
        thumbnailLabel_->setCursor(Qt::PointingHandCursor);
        thumbnailLabel_->setToolTip(url_.toString());
        layout->addWidget(thumbnailLabel_, 1);

        auto* footer = new QWidget(this);
        footer->setProperty("mycelInlinePreview", true);
        auto* footerLayout = new QHBoxLayout(footer);
        footerLayout->setContentsMargins(4, 0, 4, 2);
        footerLayout->addStretch(1);
        auto* openButton = new QPushButton(QStringLiteral("開く"), footer);
        openButton->setCursor(Qt::PointingHandCursor);
        footerLayout->addWidget(openButton);
        layout->addWidget(footer, 0);

        connect(openButton, &QPushButton::clicked, this, [this] {
            QDesktopServices::openUrl(url_);
        });
        thumbnailLabel_->installEventFilter(this);
        originalThumbnail_.load(thumbnailPath);
        if (originalThumbnail_.isNull()) {
            thumbnailLabel_->setText(QStringLiteral("サムネイルがありません"));
        } else {
            updateThumbnailPixmap();
        }
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == thumbnailLabel_ && event->type() == QEvent::MouseButtonRelease) {
            QDesktopServices::openUrl(url_);
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        updateThumbnailPixmap();
    }

private:
    void updateThumbnailPixmap()
    {
        if (!thumbnailLabel_ || originalThumbnail_.isNull()) {
            return;
        }
        const QSize target = thumbnailLabel_->size();
        if (target.isEmpty()) {
            return;
        }
        thumbnailLabel_->setPixmap(originalThumbnail_.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    QLabel* thumbnailLabel_ = nullptr;
    QUrl url_;
    QPixmap originalThumbnail_;
};

class AspectImagePreview final : public QLabel {
public:
    explicit AspectImagePreview(QWidget* parent = nullptr) : QLabel(parent)
    {
        setAlignment(Qt::AlignCenter);
        setMinimumSize(1, 1);
        setAutoFillBackground(false);
    }

    void setPreviewPixmap(const QPixmap& pixmap)
    {
        pixmap_ = pixmap;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if (pixmap_.isNull()) {
            QLabel::paintEvent(event);
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QSize targetSize = pixmap_.size().scaled(size(), Qt::KeepAspectRatio);
        const QPoint topLeft((width() - targetSize.width()) / 2,
                             (height() - targetSize.height()) / 2);
        painter.drawPixmap(QRect(topLeft, targetSize), pixmap_);
    }

private:
    QPixmap pixmap_;
};

class InlineVideoPreview final : public QWidget {
public:
    explicit InlineVideoPreview(const QString& path, QWidget* parent = nullptr) : QWidget(parent)
    {
        setProperty("mycelInlinePreview", true);
        setAutoFillBackground(false);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        videoWidget_ = new QVideoWidget(this);
        videoWidget_->setMinimumSize(1, 1);
        layout->addWidget(videoWidget_, 1);

        auto* controls = new QWidget(this);
        controls->setProperty("mycelInlinePreview", true);
        auto* controlsLayout = new QHBoxLayout(controls);
        controlsLayout->setContentsMargins(4, 0, 4, 2);
        controlsLayout->setSpacing(6);

        playButton_ = new QPushButton(QStringLiteral("再生"), controls);
        playButton_->setFixedWidth(54);
        positionSlider_ = new QSlider(Qt::Horizontal, controls);
        positionSlider_->setRange(0, 0);
        controlsLayout->addWidget(playButton_);
        controlsLayout->addWidget(positionSlider_, 1);
        layout->addWidget(controls, 0);

        audioOutput_ = new QAudioOutput(this);
        player_ = new QMediaPlayer(this);
        player_->setAudioOutput(audioOutput_);
        player_->setVideoOutput(videoWidget_);
        player_->setSource(QUrl::fromLocalFile(path));

        connect(playButton_, &QPushButton::clicked, this, [this] {
            if (player_->playbackState() == QMediaPlayer::PlayingState) {
                player_->pause();
            } else {
                player_->play();
            }
        });
        connect(player_, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
            playButton_->setText(state == QMediaPlayer::PlayingState ? QStringLiteral("停止") : QStringLiteral("再生"));
        });
        connect(player_, &QMediaPlayer::durationChanged, this, [this](qint64 duration) {
            positionSlider_->setRange(0, static_cast<int>(std::min<qint64>(duration, std::numeric_limits<int>::max())));
        });
        connect(player_, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
            if (!sliderPressed_) {
                positionSlider_->setValue(static_cast<int>(std::min<qint64>(position, std::numeric_limits<int>::max())));
            }
        });
        connect(positionSlider_, &QSlider::sliderPressed, this, [this] { sliderPressed_ = true; });
        connect(positionSlider_, &QSlider::sliderReleased, this, [this] {
            sliderPressed_ = false;
            player_->setPosition(positionSlider_->value());
        });
    }

    ~InlineVideoPreview() override
    {
        if (player_) {
            player_->stop();
            player_->setSource(QUrl());
        }
    }

private:
    QVideoWidget* videoWidget_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QSlider* positionSlider_ = nullptr;
    QAudioOutput* audioOutput_ = nullptr;
    QMediaPlayer* player_ = nullptr;
    bool sliderPressed_ = false;
};
