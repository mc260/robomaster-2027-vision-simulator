#include "CameraDevices.h"
#include "CameraEngine.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <iostream>
#include <set>
#include <opencv2/core.hpp>

int main(int argc, char** argv)
{
    cv::setNumThreads(4);
    QCoreApplication application(argc, argv);
    QString error;
    const auto devices = vision::enumerateCameras(&error);
    if (devices.isEmpty()) {
        std::cerr << error.toStdString() << '\n';
        return 1;
    }
    vision::CameraEngine engine;
    std::set<int> healthSeen;
    bool originalSeen = false, opened = false, stopped = false, failed = false;
    int frames = 0, session = 0;
    std::uint64_t lastSequence = 0;
    QElapsedTimer elapsed;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, [&] {
        const auto frame = engine.latestPreview();
        if (!frame) return;
        if (frame->image.size() != QSize(1920, 1080)) failed = true;
        if (frame->sourceSequence != lastSequence) { ++frames; lastSequence = frame->sourceSequence; }
        healthSeen.insert(frame->health);
        originalSeen |= frame->original;
    });
    QObject::connect(&engine, &vision::CameraEngine::opened, &application, [&](QSize size, double rate) {
        opened = true;
        std::cout << "Session " << session + 1 << ": " << size.width() << 'x' << size.height()
                  << ", device reports " << rate << " fps; request " << (session == 0 ? 60 : 15) << '\n';
        if (session == 0) {
            elapsed.start();
            QTimer::singleShot(1000, [&] { engine.setHealth(5000); });
            QTimer::singleShot(2000, [&] { engine.setHealth(0); });
            QTimer::singleShot(3000, [&] { engine.setOriginal(true); });
            QTimer::singleShot(4200, [&] { engine.requestStop(); });
        } else {
            QTimer::singleShot(1200, [&] { engine.requestStop(); });
        }
    });
    QObject::connect(&engine, &vision::CameraEngine::failed, &application, [&](const QString& reason) {
        failed = true;
        std::cerr << reason.toStdString() << '\n';
    });
    QObject::connect(&engine, &vision::CameraEngine::finished, &application, [&] {
        engine.joinFinished();
        if (session == 0 && opened && !failed) {
            std::cout << "First session: " << frames << " distinct frames in "
                      << elapsed.elapsed() << " ms; last capture sequence " << lastSequence
                      << "; health states " << healthSeen.size() << '\n';
            ++session;
            lastSequence = 0;
            engine.setHealth(7500);
            engine.setOriginal(false);
            engine.start(devices.front(), 15);
        } else {
            stopped = true;
            application.quit();
        }
    });
    QTimer::singleShot(15000, [&] { failed = true; engine.requestStop(); });
    poll.start(4);
    engine.start(devices.front(), 60);
    application.exec();
    const bool passed = opened && stopped && !failed && session == 1 && frames >= 15 && originalSeen &&
                        healthSeen.count(10000) && healthSeen.count(5000) && healthSeen.count(0);
    std::cout << (passed ? "PASS" : "FAIL") << ": live capture, 60-fps fallback, live health, original toggle, stop/restart at 15 fps\n";
    return passed ? 0 : 1;
}
