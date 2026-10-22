#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QStyleFactory>
#include <algorithm>
#include <opencv2/core.hpp>

int main(int argc, char* argv[])
{
    // A small fixed pool avoids oversubscribing CPUs with 1080p resize jobs.
    cv::setNumThreads(std::max(1, std::min(4, cv::getNumberOfCPUs())));
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("BaseVision"));
    application.setApplicationVersion(QStringLiteral("1.0.0"));
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("摄像头实时图传画质模拟"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(application);
    vision::MainWindow window;
    window.show();
    return application.exec();
}
