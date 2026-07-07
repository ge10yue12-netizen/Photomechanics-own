// tst_core_modules.cpp：SavePathHelper / ImageSaveThread / StageManager 单元测试

#include "../save/SavePathHelper.h"
#include "../save/ImageSaveThread.h"
#include "../stage/StageManager.h"
#include "../recorder/RecorderKit.h"
#include "../recorder/RecorderUiBinder.h"

#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
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
        if (suggested < 8000 || suggested > 50000)
            return false;
        if (recorder::effectiveScreenBitrateKbps(3000, 1920, 1080, 25) != suggested)
            return false;
        if (recorder::effectiveScreenBitrateKbps(20000, 1920, 1080, 25) != 20000)
            return false;
    }
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    // 全程复用同一 QCoreApplication，避免多次 qExec 销毁事件循环导致 QTimer 失效。
    QCoreApplication app(argc, argv);

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
    return status;
}

#include "tst_core_modules.moc"
