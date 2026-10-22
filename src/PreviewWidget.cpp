#include "PreviewWidget.h"

#include <QPainter>
#include <QEvent>
#include <cmath>

namespace vision {
PreviewWidget::PreviewWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(320, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setObjectName(QStringLiteral("preview"));
}

void PreviewWidget::setImage(const QImage& image, quint64 sequence)
{
    image_ = image;
    sequence_ = sequence;
    updateCanvasMinimum();
    update();
}

void PreviewWidget::clear(const QString& message)
{
    image_ = {};
    sequence_ = 0;
    paintedSequence_ = 0;
    message_ = message;
    updateCanvasMinimum();
    update();
}

void PreviewWidget::setPixelInspection(bool enabled)
{
    pixelInspection_ = enabled;
    updateCanvasMinimum();
    update();
}

void PreviewWidget::updateCanvasMinimum()
{
    if (pixelInspection_ && !image_.isNull()) {
        const qreal ratio = devicePixelRatioF();
        setMinimumSize(static_cast<int>(std::ceil(image_.width() / ratio)),
                       static_cast<int>(std::ceil(image_.height() / ratio)));
    } else {
        setMinimumSize(320, 120);
    }
}

bool PreviewWidget::event(QEvent* event)
{
    const bool handled = QWidget::event(event);
    if (event->type() == QEvent::ScreenChangeInternal) updateCanvasMinimum();
    return handled;
}

void PreviewWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(QStringLiteral("#070b11")));
    if (image_.isNull()) {
        painter.setPen(QColor(QStringLiteral("#8290a5")));
        QFont font = painter.font();
        font.setPointSize(16);
        painter.setFont(font);
        painter.drawText(rect().adjusted(20, 20, -20, -20),
                         Qt::AlignCenter | Qt::TextWordWrap, message_);
        return;
    }
    QRectF destination;
    if (pixelInspection_) {
        // One image pixel maps to one physical screen pixel, including HiDPI.
        const qreal ratio = devicePixelRatioF();
        const qreal left = std::round((width() * ratio - image_.width()) / 2) / ratio;
        const qreal top = std::round((height() * ratio - image_.height()) / 2) / ratio;
        destination = QRectF(left, top, image_.width() / ratio, image_.height() / ratio);
    } else {
        const QSize fitted = image_.size().scaled(size(), Qt::KeepAspectRatio);
        destination = QRectF((width() - fitted.width()) / 2,
                              (height() - fitted.height()) / 2,
                              fitted.width(), fitted.height());
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, !pixelInspection_);
    painter.drawImage(destination, image_);
    painter.end();
    if (sequence_ != 0 && paintedSequence_ != sequence_) {
        paintedSequence_ = sequence_;
        emit framePainted(sequence_);
    }
}
} // namespace vision
