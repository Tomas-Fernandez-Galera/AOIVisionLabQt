#pragma once

#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>

struct InspectionFinding
{
    QRect bounds;
    int changedPixels = 0;
    double meanDifference = 0.0;
};

struct InspectionResult
{
    bool valid = false;
    QString error;
    int featureMatches = 0;
    QImage alignedImage;
    QImage visualization;
    QVector<InspectionFinding> findings;
};

// Small OpenCV inspection engine for the public demonstration. It deliberately
// exposes understandable stages rather than production-specific AOI knowledge.
class InspectionEngine
{
public:
    InspectionResult inspect(const QImage &reference, const QImage &candidate) const;
};

