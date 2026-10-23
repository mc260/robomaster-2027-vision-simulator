#include "MainWindow.h"
#include "PreviewWidget.h"

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QTest>
#include <opencv2/core.hpp>

class DesktopTests : public QObject {
    Q_OBJECT
private slots:
    void controlsAndFormula()
    {
        vision::MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* health = window.findChild<QSpinBox*>("healthSpin");
        auto* slider = window.findChild<QSlider*>("healthSlider");
        auto* fps = window.findChild<QSpinBox*>("fpsSpin");
        QVERIFY(health && slider && fps);
        QCOMPARE(health->value(), 10000);
        QCOMPARE(slider->singleStep(), 1);
        QCOMPARE(fps->value(), 60);
        QCOMPARE(fps->minimum(), 1);
        QCOMPARE(fps->maximum(), 240);
        health->setValue(5000);
        QCOMPARE(slider->value(), 5000);
        QCOMPARE(window.findChild<QLabel*>("lambdaValue")->text(), QString("0.75000"));
        QCOMPARE(window.findChild<QLabel*>("resolutionValue")->text(), QString::fromUtf8("1440 × 810"));
        slider->setValue(0);
        QCOMPARE(health->value(), 0);
        QCOMPARE(window.findChild<QLabel*>("resolutionValue")->text(), QString::fromUtf8("960 × 540"));
        slider->setFocus();
        QTest::keyClick(slider, Qt::Key_Right);
        QCOMPARE(health->value(), 1);
        QVERIFY(!window.findChild<QPushButton*>("stopButton")->isEnabled());
        QVERIFY(fps->isEnabled());
        auto* original = window.findChild<QPushButton*>("originalButton");
        QTest::mouseClick(original, Qt::LeftButton);
        QVERIFY(original->isChecked());
        QVERIFY(window.findChild<QLabel*>("modeLabel")->text().contains(QString::fromUtf8("未降质")));
        QVERIFY(!window.findChild<vision::PreviewWidget*>("preview")->hasImage());
    }

    void paintCountsOnlyDistinctFrames()
    {
        vision::PreviewWidget preview;
        preview.resize(640, 360);
        preview.show();
        QVERIFY(QTest::qWaitForWindowExposed(&preview));
        QSignalSpy paints(&preview, &vision::PreviewWidget::framePainted);
        QImage image(1920, 1080, QImage::Format_RGB32);
        image.fill(Qt::darkCyan);
        preview.setImage(image, 1);
        QTRY_COMPARE(paints.count(), 1);
        preview.setImage(image, 1);
        preview.repaint();
        QCOMPARE(paints.count(), 1);
        preview.setImage(image, 2);
        QTRY_COMPARE(paints.count(), 2);
        preview.clear();
        QVERIFY(!preview.hasImage());
    }

    void failedStartCanBeRetried()
    {
        vision::CameraEngine engine;
        QSignalSpy failed(&engine, &vision::CameraEngine::failed);
        QSignalSpy finished(&engine, &vision::CameraEngine::finished);
        QSignalSpy opened(&engine, &vision::CameraEngine::opened);
        const vision::CameraDevice missing{QStringLiteral("/dev/basevision-device-does-not-exist"),
                                            QStringLiteral("missing"), -1};
        for (int attempt = 1; attempt <= 2; ++attempt) {
            engine.start(missing, 60);
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), attempt, 5000);
            engine.joinFinished();
            QCOMPARE(failed.count(), attempt);
            QCOMPARE(opened.count(), 0);
        }
    }

    void pixelInspectionPreservesPixels()
    {
        vision::PreviewWidget preview;
        QImage image(1920, 1080, QImage::Format_RGB32);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                image.setPixel(x, y, (x + y) % 2 ? qRgb(255, 255, 255) : qRgb(0, 0, 0));
        preview.setPixelInspection(true);
        preview.setImage(image);
        preview.resize(preview.minimumSize());
        preview.show();
        QVERIFY(QTest::qWaitForWindowExposed(&preview));
        QImage rendered = preview.grab().toImage().convertToFormat(QImage::Format_RGB32);
        rendered.setDevicePixelRatio(1.0); // Compare physical pixel data, not DPI metadata.
        QCOMPARE(rendered.size(), image.size());
        QCOMPARE(rendered, image); // Exact display pixels, also checked at DPR=2.
    }

    void missingHikrobotCanBeRetried()
    {
        vision::CameraEngine engine;
        QSignalSpy failed(&engine, &vision::CameraEngine::failed);
        QSignalSpy finished(&engine, &vision::CameraEngine::finished);
        QSignalSpy opened(&engine, &vision::CameraEngine::opened);
        const vision::CameraDevice missing{QStringLiteral("mvs:usb:basevision-missing-device"),
            QStringLiteral("missing Hikrobot"), -1, vision::CameraBackend::HikrobotMvs};
        for (int attempt = 1; attempt <= 2; ++attempt) {
            engine.start(missing, 60);
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), attempt, 8000);
            engine.joinFinished();
            QCOMPARE(failed.count(), attempt);
            QCOMPARE(opened.count(), 0);
            QVERIFY(!engine.latestPreview());
        }
    }

    void pixelInspectionScrollsAndReturnsToFit()
    {
        vision::MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* preview = window.findChild<vision::PreviewWidget*>("preview");
        auto* button = window.findChild<QPushButton*>("pixelButton");
        auto* area = window.findChild<QScrollArea*>("previewScrollArea");
        QImage image(1920, 1080, QImage::Format_RGB32);
        image.fill(Qt::darkCyan);
        preview->setImage(image);
        QVERIFY(!preview->pixelInspection());
        QTest::mouseClick(button, Qt::LeftButton);
        QVERIFY(preview->pixelInspection());
        QTRY_VERIFY(area->horizontalScrollBar()->maximum() > 0);
        QTest::mouseClick(button, Qt::LeftButton);
        QVERIFY(!preview->pixelInspection());
        QTRY_COMPARE(area->horizontalScrollBar()->maximum(), 0);
    }

    void liveCameraControls()
    {
        if (qgetenv("BASEVISION_TEST_CAMERA") != "1")
            QSKIP("Opt-in hardware test; set BASEVISION_TEST_CAMERA=1 to open the camera");
        vision::MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* start = window.findChild<QPushButton*>("startButton");
        auto* stop = window.findChild<QPushButton*>("stopButton");
        auto* fps = window.findChild<QSpinBox*>("fpsSpin");
        auto* devices = window.findChild<QComboBox*>("deviceBox");
        if (qgetenv("BASEVISION_TEST_CAMERA_BACKEND") == "mvs") {
            int index = -1;
            for (int row = 0; row < devices->count(); ++row)
                if (devices->itemData(row).toString().startsWith(QStringLiteral("mvs:usb:"))) { index = row; break; }
            QVERIFY2(index >= 0, "No Hikrobot camera in the device dropdown");
            devices->setCurrentIndex(index);
        }
        auto* health = window.findChild<QSpinBox*>("healthSpin");
        auto* preview = window.findChild<vision::PreviewWidget*>("preview");
        QVERIFY(start->isEnabled());
        QTest::mouseClick(start, Qt::LeftButton);
        QVERIFY(!fps->isEnabled());
        QVERIFY(!devices->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(preview->hasImage(), 8000);
        health->setValue(5000);
        QTest::qWait(1200);
        QTest::mouseClick(window.findChild<QPushButton*>("pixelButton"), Qt::LeftButton);
        QVERIFY(preview->pixelInspection());
        QTest::qWait(200);
        QVERIFY(!window.findChild<QLabel*>("fpsValue")->text().startsWith(QString::fromUtf8("—")));
        qInfo().noquote() << "Live UI preview FPS / target:" << window.findChild<QLabel*>("fpsValue")->text();
        QTest::mouseClick(stop, Qt::LeftButton);
        QVERIFY(!preview->hasImage());
        QTRY_VERIFY_WITH_TIMEOUT(fps->isEnabled(), 8000);
        QVERIFY(devices->isEnabled());
        QCOMPARE(health->value(), 5000);
        fps->setValue(15);
        QTest::mouseClick(start, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(preview->hasImage(), 8000);
        QTest::mouseClick(window.findChild<QPushButton*>("originalButton"), Qt::LeftButton);
        health->setValue(0);
        QTest::qWait(300);
        QTest::mouseClick(stop, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(start->isEnabled(), 8000);
        QVERIFY(!preview->hasImage());
    }
};

int main(int argc, char** argv)
{
    cv::setNumThreads(2);
    QApplication application(argc, argv);
    const QByteArray screenshot = qgetenv("BASEVISION_UI_SCREENSHOT");
    if (!screenshot.isEmpty()) {
        vision::MainWindow window;
        window.show();
        // A synthetic scene only: QA screenshots never save webcam imagery.
        QImage image;
        const auto demo = qgetenv("BASEVISION_DEMO_IMAGE");
        if (!demo.isEmpty()) image.load(QString::fromLocal8Bit(demo));
        if (image.isNull()) {
            image = QImage(1920, 1080, QImage::Format_RGB32);
            for (int y = 0; y < image.height(); ++y) {
                auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
                for (int x = 0; x < image.width(); ++x)
                    row[x] = qRgb(24 + x * 35 / 1920, 40 + y * 50 / 1080, 60 + x * 60 / 1920);
            }
        }
        window.findChild<QSpinBox*>("healthSpin")->setValue(5000);
        window.findChild<vision::PreviewWidget*>("preview")->setImage(image);
        window.findChild<QComboBox*>("deviceBox")->clear();
        window.findChild<QComboBox*>("deviceBox")->addItem(QString::fromUtf8("合成规则测试图（演示）"));
        window.findChild<QLabel*>("statusLabel")->setText(QString::fromUtf8("合成测试图演示 · 未启用摄像头"));
        QTest::qWait(150);
        if (!window.grab().save(QString::fromLocal8Bit(screenshot))) return 2;
    }
    DesktopTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "ui_tests.moc"
