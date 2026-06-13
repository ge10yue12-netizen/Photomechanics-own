// tst_core_modules.cpp：SavePathHelper / ImageSaveThread / StageManager 单元测试

#include "../save/SavePathHelper.h"
#include "../save/ImageSaveThread.h"
#include "../stage/StageManager.h"

#include <QtTest>
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
    void stagePicIncrement();
    void picPathRetryBeforeSave();
    void sanitizeIllegalChars();
    void saveLimitReached();
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
    QVERIFY(path.contains(QStringLiteral("Loop001")));
    QVERIFY(path.contains(QStringLiteral("阶段1")));
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
    helper.nextFilePath(&ok);
    QVERIFY(ok);
    helper.onFileSaved();
    QVERIFY(helper.isSaveLimitReached());
    bool ok2 = false;
    QVERIFY(helper.nextFilePath(&ok2) == QString());
    QVERIFY(!ok2);
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
    thread.start();
    QSignalSpy fullSpy(&thread, &ImageSaveThread::queueFull);
    for (int i = 0; i < thread.capacity(); ++i)
    {
        SaveTask task = makeGrayTask(QStringLiteral("C:/unused/pic%1.bmp").arg(i));
        QVERIFY(thread.trySubmit(task));
    }
    SaveTask overflow = makeGrayTask(QStringLiteral("C:/unused/overflow.bmp"));
    QVERIFY(!thread.trySubmit(overflow));
    QCOMPARE(fullSpy.count(), 1);
    thread.requestStopAndWait(5000);
}

void TestImageSaveThread::acceptsTaskWhenNotFull()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    ImageSaveThread thread;
    thread.start();
    const QString bmpPath = temp.filePath(QStringLiteral("Pic001.bmp"));
    SaveTask task = makeGrayTask(bmpPath);
    QVERIFY(thread.trySubmit(task));
    thread.waitUntilEmpty(10000);
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

int main(int argc, char *argv[])
{
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
    return status;
}

#include "tst_core_modules.moc"
