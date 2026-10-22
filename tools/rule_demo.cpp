#include "ImageProcessor.h"

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <iostream>
#include <opencv2/imgproc.hpp>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    cv::setNumThreads(2);
    const QString destination = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("rule-demo");
    if (!QDir().mkpath(destination)) return 1;
    cv::Mat baseline(vision::BaseHeight, vision::BaseWidth, CV_8UC3, cv::Scalar::all(235));
    const int cells[] = {6, 3, 1};
    for (int row = 0; row < 3; ++row) {
        const int yStart = 120 + row * 320;
        cv::putText(baseline, std::to_string(cells[row]) + " px checkerboard",
                    {60, yStart - 30}, cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar::all(30), 2);
        for (int y = yStart; y < yStart + 200; ++y)
            for (int x = 60; x < 1860; ++x)
                baseline.at<cv::Vec3b>(y, x) = cv::Vec3b::all(
                    (((x - 60) / cells[row] + (y - yStart) / cells[row]) % 2) ? 255 : 0);
    }
    for (const int health : {10000, 5000, 0}) {
        const auto result = vision::degrade(baseline, health);
        const QImage image(result.data, result.cols, result.rows,
                            static_cast<qsizetype>(result.step), QImage::Format_BGR888);
        const QString path = QDir(destination).filePath(QStringLiteral("hp-%1.png").arg(health));
        if (!image.save(path)) return 2;
        const auto parameters = vision::ruleParameters(health);
        std::cout << health << ',' << parameters.lambda << ','
                  << parameters.reducedSize.width << 'x' << parameters.reducedSize.height << '\n';
    }
}
