#include "zoomimagelabel.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

ZoomImageLabel::ZoomImageLabel(QWidget *parent)
    : QLabel(parent)
{
    setAlignment(Qt::AlignCenter);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    setToolTip(tr("Mouse wheel: zoom · Drag: pan · Double-click: fit"));
}

void ZoomImageLabel::setImage(const QImage &image)
{
    sourceImage = image;
    resetView();
}

void ZoomImageLabel::resetView()
{
    zoomFactor = 1.0;
    panOffset = QPointF();
    update();
}

void ZoomImageLabel::focusRegion(const QRect &imageRegion)
{
    if (sourceImage.isNull() || imageRegion.isEmpty())
        return;

    // Leave context around the finding instead of filling the complete panel
    // with a few pixels. The same 30x limit is shared with wheel zoom.
    const double marginFactor = 2.6;
    const double targetScale = std::min(
        double(width()) / (imageRegion.width() * marginFactor),
        double(height()) / (imageRegion.height() * marginFactor));
    zoomFactor = std::clamp(targetScale / fittedScale(), 1.0, 30.0);
    const double scale = fittedScale() * zoomFactor;
    const QPointF imageCentre(sourceImage.width() * 0.5, sourceImage.height() * 0.5);
    panOffset = -(QPointF(imageRegion.center()) - imageCentre) * scale;
    update();
}

double ZoomImageLabel::fittedScale() const
{
    if (sourceImage.isNull() || width() <= 0 || height() <= 0)
        return 1.0;
    return std::min(double(width()) / sourceImage.width(),
                    double(height()) / sourceImage.height());
}

void ZoomImageLabel::paintEvent(QPaintEvent *event)
{
    if (sourceImage.isNull()) {
        QLabel::paintEvent(event);
        return;
    }

    Q_UNUSED(event);
    QPainter painter(this);
    // Interpolation looks cleaner while fitted, but at high magnification it
    // would blur the exact pixels an AOI operator needs to inspect.
    painter.setRenderHint(QPainter::SmoothPixmapTransform, zoomFactor < 5.0);
    const double scale = fittedScale() * zoomFactor;
    const QSizeF drawnSize(sourceImage.width() * scale, sourceImage.height() * scale);
    const QPointF topLeft((width() - drawnSize.width()) * 0.5 + panOffset.x(),
                          (height() - drawnSize.height()) * 0.5 + panOffset.y());
    painter.drawImage(QRectF(topLeft, drawnSize), sourceImage);
}

void ZoomImageLabel::wheelEvent(QWheelEvent *event)
{
    if (sourceImage.isNull()) {
        event->ignore();
        return;
    }

    // Preserve the image point under the mouse cursor while changing scale;
    // zooming therefore feels anchored instead of jumping around the panel.
    const double oldScale = fittedScale() * zoomFactor;
    const QPointF cursor = event->position();
    const QPointF oldCentre(width() * 0.5 + panOffset.x(),
                            height() * 0.5 + panOffset.y());
    const QPointF imagePosition = (cursor - oldCentre) / oldScale;

    const double step = event->angleDelta().y() > 0 ? 1.20 : (1.0 / 1.20);
    zoomFactor = std::clamp(zoomFactor * step, 1.0, 30.0);
    const double newScale = fittedScale() * zoomFactor;
    panOffset = cursor - QPointF(width() * 0.5, height() * 0.5)
                - imagePosition * newScale;
    if (zoomFactor <= 1.0001)
        panOffset = QPointF();
    update();
    event->accept();
}

void ZoomImageLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !sourceImage.isNull()) {
        panning = true;
        lastMousePosition = event->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QLabel::mousePressEvent(event);
}

void ZoomImageLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (panning) {
        const QPoint current = event->position().toPoint();
        panOffset += current - lastMousePosition;
        lastMousePosition = current;
        update();
        event->accept();
        return;
    }
    QLabel::mouseMoveEvent(event);
}

void ZoomImageLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && panning) {
        panning = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}

void ZoomImageLabel::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        resetView();
        event->accept();
        return;
    }
    QLabel::mouseDoubleClickEvent(event);
}
