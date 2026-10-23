#include "CameraDevices.h"
#include "CameraEngine.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <iostream>
#include <algorithm>
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
    if (application.arguments().contains(QStringLiteral("--list"))) {
        for (const auto& device : devices)
            std::cout << (device.backend == vision::CameraBackend::HikrobotMvs ? "[MVS] " : "[System] ")
                      << device.name.toStdString() << '\n';
        return 0;
    }
    const bool hikrobot = application.arguments().contains(QStringLiteral("--hikrobot"));
    const auto selected = hikrobot
        ? std::find_if(devices.cbegin(), devices.cend(), [](const vision::CameraDevice& device) {
              return device.backend == vision::CameraBackend::HikrobotMvs;
          }) : devices.cbegin();
    if (selected == devices.cend()) {
        std::cerr << "No Hikrobot USB camera found. Check MVS SDK/runtime and USB connection.\n";
        return 1;
    }
    const auto device = *selected;
    std::cout << "Testing " << device.name.toStdString() << '\n';
    vision::CameraEngine engine;
    std::set<int> healthSeen;
    bool originalSeen = false, opened = false, stopped = false, failed = false;
    int frames = 0, session = 0, sessionFrames = 0;
    std::uint64_t lastSequence = 0;
    QElapsedTimer elapsed;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, [&] {
        const auto frame = engine.latestPreview();
        if (!frame) return;
        if (frame->image.size() != QSize(1920, 1080)) failed = true;
        if (frame->sourceSequence != lastSequence) { ++frames; ++sessionFrames; lastSequence = frame->sourceSequence; }
        healthSeen.insert(frame->health);
        originalSeen |= frame->original;
    });
    QObject::connect(&engine, &vision::CameraEngine::opened, &application, [&](QSize size, double rate) {
        opened = true;
        std::cout << "Session " << session + 1 << ": " << size.width() << 'x' << size.height()
                  << ", device reports " << rate << " fps; request " << (session == 0 ? 60 : session == 1 ? 15 : 1) << '\n';
        if (session == 0) {
            elapsed.start();
            QTimer::singleShot(1000, [&] { engine.setHealth(5000); });
            QTimer::singleShot(2000, [&] { engine.setHealth(0); });
            QTimer::singleShot(3000, [&] { engine.setOriginal(true); });
            QTimer::singleShot(4200, [&] { engine.requestStop(); });
        } else if (session == 1) {
            QTimer::singleShot(1200, [&] { engine.requestStop(); });
        } else {
            QTimer::singleShot(2500, [&] { engine.requestStop(); });
        }
    });
    QObject::connect(&engine, &vision::CameraEngine::failed, &application, [&](const QString& reason) {
        failed = true;
        std::cerr << reason.toStdString() << '\n';
    });
    QObject::connect(&engine, &vision::CameraEngine::finished, &application, [&] {
        engine.joinFinished();
        std::cout << "Session frames: " << sessionFrames << '\n';
        if (session < (hikrobot ? 2 : 1) && opened && !failed) {
            if (sessionFrames < (session == 0 ? 15 : 2)) failed = true;
            if (session == 0) std::cout << "First session: " << frames << " distinct frames in "
                      << elapsed.elapsed() << " ms; last capture sequence " << lastSequence
                      << "; health states " << healthSeen.size() << '\n';
            ++session;
            sessionFrames = 0;
            lastSequence = 0;
            engine.setHealth(7500);
            engine.setOriginal(false);
            engine.start(device, session == 1 ? 15 : 1);
        } else {
            stopped = true;
            application.quit();
        }
    });
    QTimer::singleShot(25000, [&] { failed = true; engine.requestStop(); });
    poll.start(4);
    engine.start(device, 60);
    application.exec();
    const bool passed = opened && stopped && !failed && session == (hikrobot ? 2 : 1) &&
                        sessionFrames >= 2 && frames >= 15 && originalSeen &&
                        healthSeen.count(10000) && healthSeen.count(5000) && healthSeen.count(0);
    std::cout << (passed ? "PASS" : "FAIL") << ": live capture, 60-fps fallback, live health, original toggle, stop/restart at 15 fps"
              << (hikrobot ? " and 1 fps\n" : "\n");
    return passed ? 0 : 1;
}
