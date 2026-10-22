#include "ImageProcessor.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <opencv2/imgproc.hpp>

namespace vision {
RuleParameters ruleParameters(int health)
{
    health = std::clamp(health, 0, MaxHealth);
    const double lambda = 0.5 + static_cast<double>(health) / 20000.0;
    return {health, lambda,
        {static_cast<int>(std::lround(BaseWidth * lambda)),
         static_cast<int>(std::lround(BaseHeight * lambda))}};
}

cv::Mat makeBaseline(const cv::Mat& source)
{
    if (source.empty() || source.type() != CV_8UC3)
        throw std::invalid_argument("Expected a nonempty 8-bit BGR camera frame");

    const double scale = std::min(static_cast<double>(BaseWidth) / source.cols,
                                  static_cast<double>(BaseHeight) / source.rows);
    const cv::Size fitted(
        std::clamp(static_cast<int>(std::lround(source.cols * scale)), 1, BaseWidth),
        std::clamp(static_cast<int>(std::lround(source.rows * scale)), 1, BaseHeight));
    cv::Mat baseline(BaseHeight, BaseWidth, CV_8UC3, cv::Scalar::all(0));
    const cv::Rect destination((BaseWidth - fitted.width) / 2,
                               (BaseHeight - fitted.height) / 2,
                               fitted.width, fitted.height);
    cv::resize(source, baseline(destination), fitted, 0, 0,
               scale < 1.0 ? cv::INTER_AREA : cv::INTER_LINEAR);
    return baseline;
}

cv::Mat degrade(const cv::Mat& baseline, int health)
{
    if (baseline.empty() || baseline.type() != CV_8UC3 ||
        baseline.size() != cv::Size(BaseWidth, BaseHeight))
        throw std::invalid_argument("Expected a 1920 x 1080 BGR baseline");
    const auto parameters = ruleParameters(health);
    if (parameters.reducedSize == baseline.size())
        return baseline; // Read-only shared data; full health adds no filtering.

    cv::Mat reduced, result;
    cv::resize(baseline, reduced, parameters.reducedSize, 0, 0, cv::INTER_AREA);
    cv::resize(reduced, result, baseline.size(), 0, 0, cv::INTER_LINEAR);
    return result;
}
} // namespace vision
