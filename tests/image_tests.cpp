#include "ImageProcessor.h"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <opencv2/imgproc.hpp>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
bool identical(const cv::Mat& a, const cv::Mat& b)
{
    return a.size() == b.size() && a.type() == b.type() && cv::norm(a, b, cv::NORM_INF) == 0;
}
}

int main()
{
    try {
        using namespace vision;
        check(ruleParameters(10000).reducedSize == cv::Size(1920, 1080), "Full-health dimensions");
        check(ruleParameters(5000).reducedSize == cv::Size(1440, 810), "Half-health dimensions");
        check(ruleParameters(0).reducedSize == cv::Size(960, 540), "Zero-health dimensions");
        check(ruleParameters(9999).reducedSize == cv::Size(1920, 1080), "Nearest-integer rounding");
        check(ruleParameters(1).reducedSize == cv::Size(960, 540), "Low-end rounding");
        check(std::abs(ruleParameters(5000).lambda - 0.75) < 1e-12, "Lambda formula");
        check(ruleParameters(-1).health == 0 && ruleParameters(10001).health == 10000, "Health bounds");

        cv::Mat source(480, 640, CV_8UC3, cv::Scalar(25, 100, 220));
        const cv::Mat baseline = makeBaseline(source);
        check(baseline.size() == cv::Size(1920, 1080), "Fixed baseline dimensions");
        check(cv::countNonZero(baseline.colRange(0, 240).reshape(1)) == 0, "Left letterbox");
        check(cv::countNonZero(baseline.colRange(1680, 1920).reshape(1)) == 0, "Right letterbox");
        check(baseline.at<cv::Vec3b>(0, 240) == cv::Vec3b(25, 100, 220), "No vertical crop");
        check(baseline.at<cv::Vec3b>(1079, 1679) == cv::Vec3b(25, 100, 220), "No stretching");

        cv::Mat random(BaseHeight, BaseWidth, CV_8UC3);
        cv::RNG(12345).fill(random, cv::RNG::UNIFORM, 0, 256);
        check(identical(makeBaseline(random), random), "Native 1080p remains unchanged");
        check(identical(degrade(random, 10000), random), "Full health has no extra filtering");
        for (const int health : {0, 2345, 5000, 7500}) {
            cv::Mat reduced, reference;
            cv::resize(random, reduced, ruleParameters(health).reducedSize, 0, 0, cv::INTER_AREA);
            cv::resize(reduced, reference, random.size(), 0, 0, cv::INTER_LINEAR);
            check(identical(degrade(random, health), reference), "Exact AREA then LINEAR result");
        }
        cv::Mat checker(BaseHeight, BaseWidth, CV_8UC3);
        for (int y = 0; y < checker.rows; ++y)
            for (int x = 0; x < checker.cols; ++x)
                checker.at<cv::Vec3b>(y, x) = cv::Vec3b::all((x + y) % 2 ? 255 : 0);
        const cv::Mat averaged = degrade(checker, 0);
        double minimum, maximum;
        cv::minMaxLoc(averaged.reshape(1), &minimum, &maximum);
        check(minimum >= 127 && maximum <= 128, "Area averaging removes one-pixel checkerboard");

        // Independent reference: each 2x2 block has offsets 0,4,8,12,
        // averaging to base+6. Bilinear reconstruction uses pixel centers.
        cv::Mat analytic(BaseHeight, BaseWidth, CV_8UC3);
        auto areaMean = [](int x, int y, int channel) {
            return ((x * 7 + y * 11 + channel * 3) % 15) * 16 + 6;
        };
        for (int y = 0; y < BaseHeight; ++y)
            for (int x = 0; x < BaseWidth; ++x)
                for (int c = 0; c < 3; ++c)
                    analytic.at<cv::Vec3b>(y, x)[c] = static_cast<unsigned char>(
                        areaMean(x / 2, y / 2, c) - 6 + (y % 2) * 8 + (x % 2) * 4);
        const auto reconstructed = degrade(analytic, 0);
        for (int y = 0; y < BaseHeight; y += 29) {
            const double sourceY = (y + 0.5) / 2.0 - 0.5;
            const int y0 = static_cast<int>(std::floor(sourceY));
            const double fy = sourceY - y0;
            for (int x = 0; x < BaseWidth; x += 37) {
                const double sourceX = (x + 0.5) / 2.0 - 0.5;
                const int x0 = static_cast<int>(std::floor(sourceX));
                const double fx = sourceX - x0;
                for (int c = 0; c < 3; ++c) {
                    double expected = 0;
                    for (int dy = 0; dy <= 1; ++dy)
                        for (int dx = 0; dx <= 1; ++dx)
                            expected += areaMean(std::clamp(x0 + dx, 0, 959),
                                                 std::clamp(y0 + dy, 0, 539), c) *
                                        (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
                    check(std::abs(reconstructed.at<cv::Vec3b>(y, x)[c] - expected) <= 1,
                          "Independent area-mean and bilinear reference");
                }
            }
        }
        bool rejected = false;
        try { makeBaseline({}); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "Empty frame rejected");
        std::cout << "PASS: rule endpoints, rounding, letterbox, unchanged full-health image, exact interpolation, checkerboard, independent analytic reference\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
