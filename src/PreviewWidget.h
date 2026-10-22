#pragma once

#include <QImage>
#include <QWidget>

namespace vision {
class PreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit PreviewWidget(QWidget* parent = nullptr);
    void setImage(const QImage& image, quint64 sequence = 0);
    void clear(const QString& message = QStringLiteral("摄像头未启动"));
    bool hasImage() const { return !image_.isNull(); }
    void setPixelInspection(bool enabled);
    bool pixelInspection() const { return pixelInspection_; }

signals:
    void framePainted(quint64 sequence);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent*) override;

private:
    void updateCanvasMinimum();
    QImage image_;
    bool pixelInspection_ = false;
    quint64 sequence_ = 0;
    quint64 paintedSequence_ = 0;
    QString message_ = QStringLiteral("摄像头未启动");
};
} // namespace vision
