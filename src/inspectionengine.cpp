#include "inspectionengine.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace {

cv::Mat qImageToBgr(const QImage &source)
{
    const QImage rgb = source.convertToFormat(QImage::Format_RGB888);
    cv::Mat wrapped(rgb.height(), rgb.width(), CV_8UC3,
                    const_cast<uchar *>(rgb.constBits()), rgb.bytesPerLine());
    cv::Mat bgr;
    cv::cvtColor(wrapped, bgr, cv::COLOR_RGB2BGR);
    return bgr;
}

QImage bgrToQImage(const cv::Mat &bgr)
{
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, int(rgb.step),
                  QImage::Format_RGB888).copy();
}

} // namespace

InspectionResult InspectionEngine::inspect(const QImage &referenceImage,
                                           const QImage &candidateImage) const
{
    InspectionResult result;
    if (referenceImage.isNull() || candidateImage.isNull()) {
        result.error = QStringLiteral("Both images must be loaded before inspection.");
        return result;
    }

    cv::Mat reference = qImageToBgr(referenceImage);
    cv::Mat candidate = qImageToBgr(candidateImage);
    if (candidate.size() != reference.size())
        cv::resize(candidate, candidate, reference.size(), 0.0, 0.0, cv::INTER_AREA);

    cv::Mat referenceGray;
    cv::Mat candidateGray;
    cv::cvtColor(reference, referenceGray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(candidate, candidateGray, cv::COLOR_BGR2GRAY);

    // ORB supplies rotation- and scale-tolerant landmarks without patented or
    // non-free algorithms. RANSAC rejects matches that belong to real defects.
    auto orb = cv::ORB::create(2500);
    std::vector<cv::KeyPoint> referenceKeypoints;
    std::vector<cv::KeyPoint> candidateKeypoints;
    cv::Mat referenceDescriptors;
    cv::Mat candidateDescriptors;
    orb->detectAndCompute(referenceGray, cv::noArray(), referenceKeypoints,
                          referenceDescriptors);
    orb->detectAndCompute(candidateGray, cv::noArray(), candidateKeypoints,
                          candidateDescriptors);

    cv::Mat aligned = candidate.clone();
    if (!referenceDescriptors.empty() && !candidateDescriptors.empty()) {
        cv::BFMatcher matcher(cv::NORM_HAMMING);
        std::vector<std::vector<cv::DMatch>> pairs;
        matcher.knnMatch(candidateDescriptors, referenceDescriptors, pairs, 2);

        std::vector<cv::DMatch> goodMatches;
        for (const auto &pair : pairs) {
            if (pair.size() == 2 && pair[0].distance < 0.72F * pair[1].distance)
                goodMatches.push_back(pair[0]);
        }
        result.featureMatches = int(goodMatches.size());

        if (goodMatches.size() >= 12) {
            std::vector<cv::Point2f> candidatePoints;
            std::vector<cv::Point2f> referencePoints;
            candidatePoints.reserve(goodMatches.size());
            referencePoints.reserve(goodMatches.size());
            for (const cv::DMatch &match : goodMatches) {
                candidatePoints.push_back(candidateKeypoints[match.queryIdx].pt);
                referencePoints.push_back(referenceKeypoints[match.trainIdx].pt);
            }
            const cv::Mat homography = cv::findHomography(
                candidatePoints, referencePoints, cv::RANSAC, 3.0);
            if (!homography.empty()) {
                // Resampling an already aligned image creates a halo around every
                // sharp component edge. Measure the four-corner displacement and
                // keep the original pixels when the correction is sub-pixel.
                std::vector<cv::Point2f> corners = {
                    {0.0F, 0.0F}, {float(reference.cols - 1), 0.0F},
                    {float(reference.cols - 1), float(reference.rows - 1)},
                    {0.0F, float(reference.rows - 1)}};
                std::vector<cv::Point2f> transformedCorners;
                cv::perspectiveTransform(corners, transformedCorners, homography);
                double maximumMovement = 0.0;
                for (size_t index = 0; index < corners.size(); ++index)
                    maximumMovement = std::max(
                        maximumMovement,
                        double(cv::norm(corners[index] - transformedCorners[index])));

                // A visible rotation can move the outer corners hundreds of
                // pixels even when the homography is correct. Only reject truly
                // implausible mappings that would move a corner beyond most of
                // the image diagonal.
                const double imageDiagonal = std::hypot(reference.cols,
                                                        reference.rows);
                if (maximumMovement >= 1.25 &&
                    maximumMovement <= imageDiagonal * 0.55)
                    cv::warpPerspective(candidate, aligned, homography, reference.size(),
                                        cv::INTER_LINEAR, cv::BORDER_REPLICATE);
            }
        }
    }

    result.alignedImage = bgrToQImage(aligned);

    // Convert the RGB difference into a clean binary candidate mask. Opening
    // removes isolated sensor noise; closing joins neighbouring pixels that
    // belong to the same physical component.
    cv::Mat referenceSoft;
    cv::Mat alignedSoft;
    cv::GaussianBlur(reference, referenceSoft, cv::Size(3, 3), 0.0);
    cv::GaussianBlur(aligned, alignedSoft, cv::Size(3, 3), 0.0);
    cv::Mat difference;
    cv::absdiff(referenceSoft, alignedSoft, difference);
    cv::Mat differenceGray;
    cv::cvtColor(difference, differenceGray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(differenceGray, differenceGray, cv::Size(3, 3), 0.0);

    cv::Mat mask;
    // A connector reversal creates a broad change, while flux or dirt can be
    // much darker and more diffuse.  The previous value (48) favoured small,
    // high-contrast component defects and discarded the latter.  A moderate
    // threshold keeps both kinds available for the contour quality checks.
    cv::threshold(differenceGray, mask, 32, 255, cv::THRESH_BINARY);

    // Ignore the neutral studio background. Saturated green board pixels and
    // their enclosed components form the only valid inspection area.
    cv::Mat referenceHsv;
    cv::cvtColor(reference, referenceHsv, cv::COLOR_BGR2HSV);
    cv::Mat boardSeed;
    cv::inRange(referenceHsv, cv::Scalar(30, 45, 18),
                cv::Scalar(105, 255, 255), boardSeed);
    std::vector<std::vector<cv::Point>> boardContours;
    cv::findContours(boardSeed, boardContours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);
    cv::Mat boardMask = cv::Mat::zeros(reference.size(), CV_8U);
    if (!boardContours.empty()) {
        const auto largest = std::max_element(
            boardContours.begin(), boardContours.end(),
            [](const auto &left, const auto &right) {
                return cv::contourArea(left) < cv::contourArea(right);
            });
        cv::drawContours(boardMask, boardContours,
                         int(std::distance(boardContours.begin(), largest)),
                         cv::Scalar(255), cv::FILLED);
        cv::erode(boardMask, boardMask,
                  cv::getStructuringElement(cv::MORPH_RECT, cv::Size(9, 9)));
        cv::bitwise_and(mask, boardMask, mask);
    }
    const cv::Mat smallKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                          cv::Size(3, 3));
    const cv::Mat largeKernel = cv::getStructuringElement(cv::MORPH_RECT,
                                                          cv::Size(9, 9));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, smallKernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, largeKernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    for (const auto &contour : contours) {
        const double area = cv::contourArea(contour);
        if (area < 110.0 || area > reference.total() * 0.12)
            continue;

        const cv::Rect box = cv::boundingRect(contour);
        const cv::Scalar meanValue = cv::mean(differenceGray(box), mask(box));
        const double density = double(cv::countNonZero(mask(box))) / box.area();
        if (density < 0.10 || meanValue[0] < 36.0)
            continue;
        InspectionFinding finding;
        finding.bounds = QRect(box.x, box.y, box.width, box.height).adjusted(-7, -7, 7, 7)
                             .intersected(referenceImage.rect());
        finding.changedPixels = cv::countNonZero(mask(box));
        finding.meanDifference = meanValue[0];
        result.findings.append(finding);
    }

    std::sort(result.findings.begin(), result.findings.end(),
              [](const InspectionFinding &left, const InspectionFinding &right) {
                  return left.changedPixels > right.changedPixels;
              });
    if (result.findings.size() > 20)
        result.findings.resize(20);

    result.visualization = result.alignedImage.copy();
    QPainter painter(&result.visualization);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(238, 45, 55), 4);
    painter.setPen(pen);
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(23);
    painter.setFont(font);

    for (int index = 0; index < result.findings.size(); ++index) {
        const QRect box = result.findings.at(index).bounds;
        painter.fillRect(box, QColor(255, 35, 45, 42));
        painter.setPen(pen);
        painter.drawRoundedRect(box, 5, 5);
        const QRect label(box.left(), qMax(0, box.top() - 29), 36, 27);
        painter.fillRect(label, QColor(205, 25, 35));
        painter.setPen(Qt::white);
        painter.drawText(label, Qt::AlignCenter, QString::number(index + 1));
    }
    painter.end();

    result.valid = true;
    return result;
}
