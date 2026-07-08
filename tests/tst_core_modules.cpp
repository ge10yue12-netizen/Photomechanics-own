// tst_core_modules.cpp：SavePathHelper / ImageSaveThread / StageManager 单元测试

#include "../save/SavePathHelper.h"
#include "../save/ImageSaveThread.h"
#include "../stage/StageManager.h"
#include "../recorder/RecorderKit.h"
#include "../recorder/RecorderUiBinder.h"
#include "../recorder/include/RecorderPresets.h"
#include "../recorder/RecorderOutputStore.h"
#include "../recorder/RecorderPathHelper.h"
#include "../recorder/RecorderVideoProbe.h"
#include "../recorder/include/VisualFrameCache.h"
#include "../ui/PreviewWidget.h"
#include "../recorder/src/Capture/FrameCompare.h"

#include <QtTest>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <cstring>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>

// --- SavePathHelper ---

class TestSavePathHelper : public QObject
{
    Q_OBJECT
private slots:
    void stageFolderPathFormat();
    void stagePicContinuesAcrossLoops();
    void stagePicIncrement();
    void picPathRetryBeforeSave();
    void sanitizeIllegalChars();
    void saveLimitReached();
    void resumeCountsLoopDirs();
};

void TestSavePathHelper::stageFolderPathFormat()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    SavePathHelper helper;
    helper.setRootPath(temp.path());
    helper.beginStageCapture();
    helper.setStageContext(1, QStringLiteral("阶段1"));
    bool ok = false;
    const QString path = helper.nextFilePath(&ok);
    QVERIFY(ok);
    QVERIFY(path.endsWith(QStringLiteral("Pic001.bmp")));
    QVERIFY(!path.contains(QStringLiteral("Loop")));
    QVERIFY(path.contains(QStringLiteral("阶段1")));
}

void TestSavePathHelper::stagePicContinuesAcrossLoops()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    SavePathHelper helper;
    helper.setRootPath(temp.path());
    helper.beginStageCapture();
    helper.setStageContext(1, QStringLiteral("阶段1"));
    bool ok = false;
    const QString p1 = helper.nextFilePath(&ok);
    QVERIFY(ok);
    QVERIFY(p1.contains(QStringLiteral("Pic001.bmp")));
    {
        QFile file(p1);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
    }
    helper.onFileSaved();
    helper.setStageContext(2, QStringLiteral("阶段1"));
    bool ok2 = false;
    const QString p2 = helper.nextFilePath(&ok2);
    QVERIFY(ok2);
    QVERIFY(p2.contains(QStringLiteral("Pic002.bmp")));
}

void TestSavePathHelper::stagePicIncrement()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    SavePathHelper helper;
    helper.setRootPath(temp.path());
    helper.beginStageCapture();
    helper.setStageContext(1, QStringLiteral("阶段A"));
    bool ok1 = false, ok2 = false;
    const QString p1 = helper.nextFilePath(&ok1);
    QVERIFY(ok1);
    QVERIFY(p1.contains(QStringLiteral("Pic001.bmp")));
    helper.onFileSaved();
    const QString p2 = helper.nextFilePath(&ok2);
    QVERIFY(ok2);
    QVERIFY(p2.contains(QStringLiteral("Pic002.bmp")));
}

void TestSavePathHelper::picPathRetryBeforeSave()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    SavePathHelper helper;
    helper.setRootPath(temp.path());
    helper.beginStageCapture();
    helper.setStageContext(1, QStringLiteral("retry"));
    bool ok1 = false, ok2 = false;
    const QString p1 = helper.nextFilePath(&ok1);
    const QString p2 = helper.nextFilePath(&ok2);
    QVERIFY(ok1 && ok2);
    QCOMPARE(p1, p2); // 写盘前未递增，入队失败可重试同一路径
    helper.onFileSaved();
    bool ok3 = false;
    const QString p3 = helper.nextFilePath(&ok3);
    QVERIFY(ok3);
    QVERIFY(p3.contains(QStringLiteral("Pic002.bmp")));
}

void TestSavePathHelper::sanitizeIllegalChars()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    SavePathHelper helper;
    helper.setRootPath(temp.path());
    helper.beginStageCapture();
    helper.setStageContext(2, QStringLiteral("a/b:c"));
    bool ok = false;
    const QString path = helper.nextFilePath(&ok);
    QVERIFY(ok);
    QVERIFY(path.contains(QStringLiteral("a_b_c")));
}

void TestSavePathHelper::saveLimitReached()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    SavePathHelper helper;
    helper.setRootPath(temp.path());
    helper.setMaxSaveCount(1);
    helper.beginStageCapture();
    helper.setStageContext(1, QStringLiteral("lim"));
    bool ok = false;
    const QString path = helper.nextFilePath(&ok);
    QVERIFY(ok);
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();
    helper.onFileSaved();
    QVERIFY(helper.isSaveLimitReached());
    bool ok2 = false;
    QVERIFY(helper.nextFilePath(&ok2) == QString());
    QVERIFY(!ok2);
}

void TestSavePathHelper::resumeCountsLoopDirs()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString stageDir = temp.path() + QStringLiteral("/阶段1");
    QVERIFY(QDir().mkpath(stageDir));
    QFile file(stageDir + QStringLiteral("/Pic001.bmp"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();

    SavePathHelper helper;
    helper.setRootPath(temp.path());
    helper.resumeFromDisk();
    QCOMPARE(helper.totalSaved(), 1);
}

// --- ImageSaveThread ---

class TestImageSaveThread : public QObject
{
    Q_OBJECT
private slots:
    void capacityIs48();
    void trySubmitRejectsWhenFull();
    void acceptsTaskWhenNotFull();
};

static SaveTask makeGrayTask(const QString &filePath)
{
    SaveTask task;
    task.filePath = filePath;
    task.image = QImage(4, 4, QImage::Format_Grayscale8);
    task.image.fill(128);
    return task;
}

void TestImageSaveThread::capacityIs48()
{
    ImageSaveThread thread;
    QCOMPARE(thread.capacity(), 48);
}

void TestImageSaveThread::trySubmitRejectsWhenFull()
{
    ImageSaveThread thread;
    QSignalSpy fullSpy(&thread, &ImageSaveThread::queueFull);
    // 先填满队列再启动线程，避免消费者 dequeue 导致用例不稳定。
    for (int i = 0; i < thread.capacity(); ++i)
    {
        SaveTask task = makeGrayTask(QStringLiteral("C:/unused/pic%1.bmp").arg(i));
        QVERIFY(thread.trySubmit(task));
    }
    SaveTask overflow = makeGrayTask(QStringLiteral("C:/unused/overflow.bmp"));
    QVERIFY(!thread.trySubmit(overflow));
    QCOMPARE(fullSpy.count(), 1);
    thread.start();
    thread.requestStopAndWait(5000);
}

void TestImageSaveThread::acceptsTaskWhenNotFull()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    ImageSaveThread thread;
    QSignalSpy finishedSpy(&thread, &ImageSaveThread::saveFinished);
    thread.start();
    const QString bmpPath = temp.filePath(QStringLiteral("Pic001.bmp"));
    SaveTask task = makeGrayTask(bmpPath);
    QVERIFY(thread.trySubmit(task));
    QVERIFY(QTest::qWaitFor([&]() { return finishedSpy.count() >= 1; }, 10000));
    QVERIFY(QFile::exists(bmpPath));
    thread.requestStopAndWait(5000);
}

// --- StageManager ---

class TestStageManager : public QObject
{
    Q_OBJECT
private slots:
    void targetFrameCountRound();
    void stageFinishWithSimulatedSave();
    void zeroFpsSkipsTimer();
};

void TestStageManager::targetFrameCountRound()
{
    StageManager mgr;
    QList<StageItem> stages;
    StageItem st;
    st.name = QStringLiteral("t");
    st.durationSec = 0.5;
    st.fps = 5.0;
    st.saveImage = false;
    stages.append(st);
    mgr.setStages(stages);
    mgr.setLoopCount(1);
    QSignalSpy finishedSpy(&mgr, &StageManager::stageFinished);
    mgr.start();
    QVERIFY(QTest::qWaitFor([&]() { return finishedSpy.count() >= 1; }, 3000));
    const auto args = finishedSpy.takeFirst();
    QCOMPARE(args.at(3).toInt(), 3);
}

void TestStageManager::stageFinishWithSimulatedSave()
{
    StageManager mgr;
    connect(&mgr, &StageManager::saveFrameRequested, &mgr, [&mgr]() {
        mgr.notifySaveEnqueued();
        mgr.notifySaveWriteFinished(true);
    });
    QList<StageItem> stages;
    StageItem st;
    st.name = QStringLiteral("save");
    st.durationSec = 1.0;
    st.fps = 10.0;
    st.saveImage = true;
    stages.append(st);
    mgr.setStages(stages);
    mgr.setLoopCount(1);
    QSignalSpy finishedSpy(&mgr, &StageManager::stageFinished);
    mgr.start();
    QVERIFY(QTest::qWaitFor([&]() { return finishedSpy.count() >= 1; }, 5000));
    const auto args = finishedSpy.takeFirst();
    QCOMPARE(args.at(4).toInt(), 10);
    QCOMPARE(args.at(5).toInt(), 10);
}

void TestStageManager::zeroFpsSkipsTimer()
{
    StageManager mgr;
    QList<StageItem> stages;
    StageItem st;
    st.name = QStringLiteral("zero");
    st.durationSec = 1.0;
    st.fps = 0.0;
    st.saveImage = true;
    stages.append(st);
    mgr.setStages(stages);
    QSignalSpy finishedSpy(&mgr, &StageManager::stageFinished);
    mgr.start();
    QVERIFY(QTest::qWaitFor([&]() { return finishedSpy.count() >= 1; }, 2000));
}

// --- Recorder validateConfig（无 Q_OBJECT，避免破坏 tst_core_modules.moc 流程）---

namespace
{

bool recorderValidateTests()
{
    {
        recorder::RecorderConfig cfg;
        cfg.output.filePath = "D:/tmp/test.avi";
        cfg.video.fps = 0;
        recorder::RecorderErrorCode code = recorder::RecorderErrorCode::None;
        std::string message;
        if (recorder::validateConfig(cfg, &code, &message))
            return false;
        if (code != recorder::RecorderErrorCode::InvalidConfig)
            return false;
    }
    {
        recorder::RecorderConfig cfg;
        cfg.output.filePath = "D:/tmp/test.avi";
        cfg.video.fps = 30;
        recorder::RecorderErrorCode code = recorder::RecorderErrorCode::None;
        std::string message;
        if (!recorder::validateConfig(cfg, &code, &message))
            return false;
    }
    {
        recorder::RecorderConfig cfg;
        cfg.output.format = recorder::VideoFormat::Mp4;
        cfg.output.filePath = "D:/tmp/test.mp4";
        cfg.video.fps = 25;
        recorder::RecorderErrorCode code = recorder::RecorderErrorCode::None;
        std::string message;
        if (!recorder::validateConfig(cfg, &code, &message))
            return false;
        if (!recorder::isFormatSupported(recorder::VideoFormat::Mp4))
            return false;
    }
    {
        recorder::RecorderConfig cfg;
        cfg.mode = recorder::CaptureMode::Region;
        cfg.region.width = 50;
        cfg.region.height = 50;
        cfg.output.filePath = "D:/tmp/test.avi";
        cfg.video.fps = 30;
        recorder::RecorderErrorCode code = recorder::RecorderErrorCode::None;
        std::string message;
        if (recorder::validateConfig(cfg, &code, &message))
            return false;
        if (code != recorder::RecorderErrorCode::InvalidRegion)
            return false;
    }
    if (RecorderUiBinder::previewCaptureMode(recorder::CaptureMode::FullScreen, false) !=
        recorder::CaptureMode::FullScreen)
        return false;
    if (RecorderUiBinder::previewCaptureMode(recorder::CaptureMode::Region, false) !=
        recorder::CaptureMode::FullScreen)
        return false;
    if (RecorderUiBinder::previewCaptureMode(recorder::CaptureMode::Region, true) !=
        recorder::CaptureMode::Region)
        return false;
    if (RecorderUiBinder::previewCaptureMode(recorder::CaptureMode::Window, false) !=
        recorder::CaptureMode::Window)
        return false;
    if (std::strcmp(recorder::captureModeLabel(recorder::CaptureMode::Window), "窗口录制") != 0)
        return false;
    {
        recorder::RecorderConfig cfg;
        cfg.mode = recorder::CaptureMode::Window;
        cfg.output.filePath = "D:/tmp/test.mp4";
        cfg.video.fps = 25;
        recorder::RecorderErrorCode code = recorder::RecorderErrorCode::None;
        std::string message;
        if (recorder::validateConfig(cfg, &code, &message))
            return false;
        if (code != recorder::RecorderErrorCode::InvalidWindowTarget)
            return false;
    }
    {
        recorder::FixedWindowTarget target(0);
        if (target.isAvailable())
            return false;
        recorder::RecorderConfig cfg;
        cfg.output.filePath = "D:/tmp/test.mp4";
        cfg.video.fps = 25;
        recorder::applyWindowTarget(&cfg, &target);
        if (cfg.mode != recorder::CaptureMode::Window)
            return false;
        if (cfg.windowTarget.provider != &target)
            return false;
    }
    if (!recorder::hasVisualConsumerFlag(
            static_cast<int>(recorder::VisualConsumerFlag::LivePreview),
            recorder::VisualConsumerFlag::LivePreview))
        return false;
    if (recorder::hasVisualConsumerFlag(0, recorder::VisualConsumerFlag::Recording))
        return false;
    {
        recorder::Rect region;
        region.width = 801;
        region.height = 601;
        const QSize sz = RecorderUiBinder::captureSourceSize(recorder::CaptureMode::Region, region, true);
        if (sz.width() != 800 || sz.height() != 600)
            return false;
    }
    {
        const int suggested = recorder::suggestScreenBitrateKbps(1920, 1080, 25);
        if (suggested < 2000 || suggested > 50000)
            return false;
        if (recorder::effectiveScreenBitrateKbps(3000, 1920, 1080, 25) != 3000)
            return false;
        if (recorder::effectiveScreenBitrateKbps(20000, 1920, 1080, 25) != 20000)
            return false;
        const int minKbps = recorder::minimumScreenBitrateKbps(1920, 1080, 25);
        if (recorder::effectiveScreenBitrateKbps(800, 1920, 1080, 25) != minKbps)
            return false;
    }
    {
        recorder::CaptureFrame a;
        a.width = 64;
        a.height = 64;
        a.stride = 64 * 4;
        a.bgra.assign(static_cast<size_t>(a.stride) * 64u, 0);
        recorder::CaptureFrame b = a;
        if (recorder::capture::framesVisuallyChanged(a, b))
            return false;
        b.bgra[0] = 255;
        b.bgra[1] = 255;
        b.bgra[2] = 255;
        if (!recorder::capture::framesVisuallyChanged(a, b))
            return false;
    }
    return true;
}

bool recorderVisualCacheTests()
{
    recorder::VisualFrameCache cache;
    if (cache.hasFrame())
        return false;

    recorder::CaptureFrame frame;
    frame.width = 4;
    frame.height = 2;
    frame.stride = 16;
    frame.bgra = {10, 20, 30, 255, 11, 21, 31, 255, 12, 22, 32, 255, 13, 23, 33, 255,
                  14, 24, 34, 255, 15, 25, 35, 255, 16, 26, 36, 255, 17, 27, 37, 255};
    cache.publish(frame);

    recorder::CaptureFrame copy;
    std::uint64_t version = 0;
    if (!cache.copyLatest(&copy, &version))
        return false;
    if (version != 1 || copy.width != 4 || copy.bgra.size() != frame.bgra.size())
        return false;

    frame.bgra[0] = 99;
    cache.publish(std::move(frame));
    if (cache.version() != 2)
        return false;
    if (!cache.copyLatest(&copy) || copy.bgra[0] != 99)
        return false;
    return true;
}

bool previewWidgetSnapshotTests()
{
    PreviewWidget widget;
    widget.resize(320, 240);
    widget.show();
    QGuiApplication::processEvents();

    QImage pattern(160, 120, QImage::Format_Grayscale8);
    for (int y = 0; y < pattern.height(); ++y)
        for (int x = 0; x < pattern.width(); ++x)
            pattern.scanLine(y)[x] = static_cast<uchar>((x + y) & 0xFF);
    widget.setImage(pattern);
    QGuiApplication::processEvents();

    const QImage snap = widget.recorderVisualSnapshot();
    if (snap.isNull() || snap.width() != 320)
        return false;

    int nonBg = 0;
    for (int y = 0; y < snap.height(); ++y)
    {
        const QRgb *row = reinterpret_cast<const QRgb *>(snap.constScanLine(y));
        for (int x = 0; x < snap.width(); ++x)
        {
            const QRgb px = row[x];
            if (qRed(px) > 40 || qGreen(px) > 40 || qBlue(px) > 40)
                ++nonBg;
        }
    }
    return nonBg > 500;
}

bool recorderOutputStoreSyncTests()
{
    QTemporaryDir temp;
    if (!temp.isValid())
        return false;

    RecorderPathHelper::setRecordRootDirOverride(temp.path());

    auto writeVideo = [&](const QString &fileName) -> bool {
        QFile file(temp.filePath(fileName));
        if (!file.open(QIODevice::WriteOnly))
            return false;
        file.write("fake");
        return true;
    };

    if (!writeVideo(QStringLiteral("first.mp4")) || !writeVideo(QStringLiteral("second.mp4")))
    {
        RecorderPathHelper::clearRecordRootDirOverride();
        return false;
    }

    RecorderOutputStore store;
    store.setOutputDirectory(temp.path());
    if (!store.load(nullptr))
    {
        RecorderPathHelper::clearRecordRootDirOverride();
        return false;
    }
    if (store.entries().size() != 2)
    {
        RecorderPathHelper::clearRecordRootDirOverride();
        return false;
    }
    if (QFile::exists(temp.filePath(QStringLiteral("output_history.json"))))
    {
        RecorderPathHelper::clearRecordRootDirOverride();
        return false;
    }

    const QString thirdPath = RecorderPathHelper::uniqueOutputFileInDir(temp.path(), recorder::VideoFormat::Mp4);
    QFile third(thirdPath);
    if (!third.open(QIODevice::WriteOnly))
    {
        RecorderPathHelper::clearRecordRootDirOverride();
        return false;
    }
    third.write("fake3");
    third.close();

    if (!store.rebuildFromDirectory(nullptr) || store.entries().size() != 3)
    {
        RecorderPathHelper::clearRecordRootDirOverride();
        return false;
    }
    if (QFile::exists(temp.filePath(QStringLiteral("output_history.json"))))
    {
        RecorderPathHelper::clearRecordRootDirOverride();
        return false;
    }

    const QString firstPath = store.entries().first().filePath;
    if (!store.removeEntryAt(0, nullptr) || QFile::exists(firstPath))
    {
        RecorderPathHelper::clearRecordRootDirOverride();
        return false;
    }
    if (store.entries().size() != 2)
    {
        RecorderPathHelper::clearRecordRootDirOverride();
        return false;
    }

    const QString externalDeletePath = store.entries().first().filePath;
    if (!QFile::remove(externalDeletePath))
    {
        RecorderPathHelper::clearRecordRootDirOverride();
        return false;
    }
    if (!store.rebuildFromDirectory(nullptr) || store.entries().size() != 1)
    {
        RecorderPathHelper::clearRecordRootDirOverride();
        return false;
    }
    for (const RecorderOutputEntry &entry : store.entries())
    {
        if (entry.filePath == externalDeletePath)
        {
            RecorderPathHelper::clearRecordRootDirOverride();
            return false;
        }
    }

    RecorderPathHelper::clearRecordRootDirOverride();
    return true;
}

bool recorderPresetResolveTests()
{
    using recorder::EncodeLevel;
    using recorder::FrameRatePreset;
    using recorder::QualityLevel;
    using recorder::ResolvedVideoParams;

    const auto resolve = [](QualityLevel q) {
        return recorder::resolveVideoPresets(q,
                                             EncodeLevel::Default,
                                             FrameRatePreset::Standard,
                                             1920,
                                             1080,
                                             recorder::VideoFormat::Mp4);
    };

    const ResolvedVideoParams original = resolve(QualityLevel::Original);
    const ResolvedVideoParams ultra = resolve(QualityLevel::UltraClear);
    const ResolvedVideoParams hd = resolve(QualityLevel::HD);
    const ResolvedVideoParams clear = resolve(QualityLevel::Clear);
    const ResolvedVideoParams low = resolve(QualityLevel::Low);

    if (original.fps != 20 || ultra.fps != 20 || hd.fps != 20 || low.fps != 20)
        return false;
    if (original.outputWidth != 1920 || original.outputHeight != 1080)
        return false;
    if (ultra.outputWidth != 1920 || ultra.outputHeight != 1080)
        return false;
    if (clear.outputWidth < 1920 || clear.outputHeight < 1080)
        return false;
    if (low.outputWidth == 1920 || low.outputHeight == 1080)
        return false;
    if (original.bitrateKbps <= ultra.bitrateKbps)
        return false;
    if (ultra.bitrateKbps <= hd.bitrateKbps)
        return false;
    if (hd.bitrateKbps <= clear.bitrateKbps)
        return false;
    if (clear.bitrateKbps <= low.bitrateKbps)
        return false;
    if (low.bitrateKbps < recorder::minimumScreenBitrateKbps(low.outputWidth, low.outputHeight, 20))
        return false;

    const ResolvedVideoParams smooth =
        recorder::resolveVideoPresets(QualityLevel::HD,
                                    EncodeLevel::Fine,
                                    FrameRatePreset::Smooth,
                                    678,
                                    460,
                                    recorder::VideoFormat::Mp4);
    if (smooth.fps != 15 || smooth.encodeLevel != EncodeLevel::Fine)
        return false;
    if (smooth.outputWidth != 678 || smooth.outputHeight != 460)
        return false;

    recorder::RecorderConfig cfg = RecorderUiBinder::configFromWidgets(
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, {}, 1920, 1080);
    if (cfg.video.fps != 20 || cfg.video.bitrateKbps < 500)
        return false;
    if (cfg.video.outputWidth != 1920 || cfg.video.outputHeight != 1080)
        return false;

    if (recorder::defaultQualityLevel() != QualityLevel::UltraClear)
        return false;
    if (recorder::frameRateFromPreset(FrameRatePreset::High) != 30)
        return false;
    if (QString::fromUtf8(recorder::qualityLevelLabel(QualityLevel::HD)) != QStringLiteral("高清"))
        return false;
    if (recorder::qualityScaleRatio(QualityLevel::Low) != 0.5)
        return false;

    if (recorder::encodeGopSize(EncodeLevel::Default, 20) != 40)
        return false;
    if (recorder::encodeGopSize(EncodeLevel::Fine, 20) != 20)
        return false;
    if (recorder::encodeGopSize(EncodeLevel::General, 20) != 60)
        return false;
    if (recorder::encodeMaxBitrateKbps(EncodeLevel::Default, 3000) != 6000)
        return false;
    if (recorder::encodeMaxBitrateKbps(EncodeLevel::Fine, 3000) != 7500)
        return false;
    if (recorder::encodeMjpegQuality(EncodeLevel::Fine, 5000) != 55)
        return false;

    return true;
}

bool recorderVideoProbeTests()
{
    if (RecorderVideoProbe::durationSeconds(QString()) != 0)
        return false;
    if (RecorderVideoProbe::durationSeconds(QStringLiteral("Z:/no_such_file.mp4")) != 0)
        return false;

    if (RecorderVideoProbe::hundredNanosecondsToRoundedSeconds(0) != 0)
        return false;
    if (RecorderVideoProbe::hundredNanosecondsToRoundedSeconds(10000000ULL) != 1)
        return false;
    if (RecorderVideoProbe::hundredNanosecondsToRoundedSeconds(134000000ULL) != 13)
        return false;
    if (RecorderVideoProbe::hundredNanosecondsToRoundedSeconds(136000000ULL) != 14)
        return false;
    if (RecorderVideoProbe::hundredNanosecondsToRoundedSeconds(135000000ULL) != 14)
        return false;
    if (RecorderVideoProbe::hundredNanosecondsToRoundedSeconds(134999999ULL) != 13)
        return false;

    if (RecorderVideoProbe::durationSecondsWithFallback(QString(), 12) != 12)
        return false;
    if (RecorderVideoProbe::durationSecondsWithFallback(QStringLiteral("Z:/missing.mp4"), 9) != 9)
        return false;
    if (RecorderVideoProbe::durationSecondsWithFallback(QStringLiteral("Z:/missing.mp4"), -1) != 0)
        return false;

    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    // 全程复用同一 QGuiApplication，避免多次 qExec 销毁事件循环导致 QTimer 失效。
    QGuiApplication app(argc, argv);

    int status = 0;
    {
        TestSavePathHelper suite;
        status |= QTest::qExec(&suite, argc, argv);
    }
    {
        TestImageSaveThread suite;
        status |= QTest::qExec(&suite, argc, argv);
    }
    {
        TestStageManager suite;
        status |= QTest::qExec(&suite, argc, argv);
    }
    if (!recorderValidateTests())
        status |= 1;
    if (!recorderVisualCacheTests())
        status |= 1;
    if (!previewWidgetSnapshotTests())
        status |= 1;
    if (!recorderOutputStoreSyncTests())
        status |= 1;
    if (!recorderPresetResolveTests())
        status |= 1;
    if (!recorderVideoProbeTests())
        status |= 1;
    return status;
}

#include "tst_core_modules.moc"
