#pragma once

#include <QImage>
#include <QLabel>
#include <QPointF>
#include <QRect>

// Compact image viewer: wheel zoom, left-button pan and double-click reset.
// It replaces a QLabel without requiring extra toolbars or screen space.
class ZoomImageLabel final : public QLabel
{
    Q_OBJECT

public:
    explicit ZoomImageLabel(QWidget *parent = nullptr);
    void setImage(const QImage &image);
    void resetView();
    void focusRegion(const QRect &imageRegion);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    double fittedScale() const;

    QImage sourceImage;
    double zoomFactor = 1.0;
    QPointF panOffset;
    QPoint lastMousePosition;
    bool panning = false;
};
