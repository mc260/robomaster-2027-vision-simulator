#pragma once

#include <opencv2/core.hpp>

namespace vision {
constexpr int BaseWidth = 1920;
constexpr int BaseHeight = 1080;
constexpr int MaxHealth = 10000;

struct RuleParameters {
    int health;
    double lambda;
    cv::Size reducedSize;
};

RuleParameters ruleParameters(int health);
// Input/output are 8-bit BGR. Letterboxing never crops or stretches the image.
cv::Mat makeBaseline(const cv::Mat& source);
cv::Mat degrade(const cv::Mat& baseline, int health);
} // namespace vision
