// CameraController.cpp：Basler Pylon 封装（Impl、grabLoopWorker）

#include "CameraController.h"

#include <atomic>
#include <cstring>
#include <thread>
#include <utility>

#include <QDebug>
#include <QImage>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>

#ifdef QT_PROJECT_USE_PYLON
// Pylon/GenApi 为第三方头文件，屏蔽编译警告，请勿修改 SDK 源码
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26451 26495 26812)
#endif
#include <pylon/PylonIncludes.h>
#include <pylon/ImageFormatConverter.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
using namespace Pylon;
using namespace GenApi;
#endif

// Pylon 相关成员集中在此，头文件不暴露 SDK 类型
struct CameraController::Impl
{
#ifdef QT_PROJECT_USE_PYLON
    CInstantCamera camera;              // Basler 设备实例
    CImageFormatConverter converter;    // 原始格式 → Mono8（失败时回退 RGB8）
    enum class CapturePixelLayout { Grayscale8, Rgb888 };
    CapturePixelLayout captureLayout = CapturePixelLayout::Grayscale8; // 当前抓图与 QImage 格式
    QMutex imageMutex;                  // 保护 latestImage 与抓图线程的读写
    QImage latestImage;                 // 最近一帧，供 UI 预览与存图
    std::atomic<quint64> frameSequence{0}; // 抓图线程每更新一帧递增，供阶段存图去重
    std::thread grabThread;             // 执行 grabLoopWorker 的专用线程
    std::atomic<bool> grabStop{false};  // 为 true 时 grabLoopWorker 退出循环
#endif
};

// 构造 Impl 并设置未连接相机时的默认参数范围
CameraController::CameraController(QObject *parent)
    : QObject(parent)
    , m_impl(new Impl)
{
    // 未 open 前的默认范围，open 后由 readParamLimits 覆盖
    m_limits.exposureMinUs = 100.0;
    m_limits.exposureMaxUs = 500000.0;
    m_limits.gainMinDb = 0.0;
    m_limits.gainMaxDb = 24.0;
    m_limits.fpsMin = 1.0;
    m_limits.fpsMax = 60.0;
}

// 若仍在运行则先停采关设备，再释放 Impl
CameraController::~CameraController()
{
    // shutdownPylon 可能已在主窗口退出流程中重建 Impl；析构阶段仅释放 Impl 堆对象
    if (m_pylonInitialized)
    {
        stopGrab();
        close();
    }
    delete m_impl;
    m_impl = nullptr;
}

// 进程级 Pylon 运行时初始化，失败时通过 errorOccurred 上报
bool CameraController::initPylon()
{
#ifdef QT_PROJECT_USE_PYLON
    try
    {
        PylonInitialize();
        m_pylonInitialized = true;
        return true;
    }
    catch (const GenericException &e)
    {
        emit errorOccurred(QString::fromLocal8Bit(e.GetDescription()));
        return false;
    }
#else
    emit errorOccurred(QStringLiteral("未启用 Pylon SDK，请在工程中定义 QT_PROJECT_USE_PYLON。"));
    return false;
#endif
}

// 进程退出：停止采集、关闭设备、重建 Impl、调用 PylonTerminate，避免 SDK 对象泄漏
void CameraController::shutdownPylon()
{
#ifdef QT_PROJECT_USE_PYLON
    if (!m_pylonInitialized)
        return;

    stopGrab();
    close();

    // 须在 PylonTerminate 之前释放 CInstantCamera / Converter，否则可能触发访问冲突
    delete m_impl;
    m_impl = new Impl;

    try
    {
        PylonTerminate();
    }
    catch (...)
    {
        // 退出流程中捕获异常并记录，不再向调用方传播
        qWarning("CameraController::shutdownPylon: PylonTerminate 异常已忽略。");
    }
    m_pylonInitialized = false;
    m_open = false;
    m_grabbing = false;
#endif
}

#ifdef QT_PROJECT_USE_PYLON
namespace
{
// 将 Pylon 转换结果按行拷贝至 QImage；stride 不一致时逐行 memcpy
bool copyPylonImageToQImage(const CPylonImage &target, QImage &out, int bytesPerPixel, QImage::Format format)
{
    const int w = static_cast<int>(target.GetWidth());
    const int h = static_cast<int>(target.GetHeight());
    if (w <= 0 || h <= 0 || bytesPerPixel <= 0)
        return false;

    out = QImage(w, h, format);
    const uchar *src = static_cast<const uchar *>(target.GetBuffer());
    size_t srcStrideBytes = static_cast<size_t>(w) * static_cast<size_t>(bytesPerPixel);
    if (!target.GetStride(srcStrideBytes) || srcStrideBytes == 0)
        srcStrideBytes = static_cast<size_t>(w) * static_cast<size_t>(bytesPerPixel);

    const int dstStride = out.bytesPerLine();
    const size_t copyBytes = static_cast<size_t>(w) * static_cast<size_t>(bytesPerPixel);
    for (int y = 0; y < h; ++y)
    {
        const size_t dstOff = static_cast<size_t>(y) * static_cast<size_t>(dstStride);
        const size_t srcOff = static_cast<size_t>(y) * srcStrideBytes;
        memcpy(out.bits() + dstOff, src + srcOff, copyBytes);
    }
    return true;
}
}
#endif

// 打开设备：当前固定 CreateFirstDevice；预留由界面 cameraSelectCombo 指定序列号/索引
bool CameraController::open()
{
#ifdef QT_PROJECT_USE_PYLON
    if (!m_pylonInitialized)
        return false;

    if (m_open)
        return true;

    try
    {
        CTlFactory &factory = CTlFactory::GetInstance();
        IPylonDevice *device = factory.CreateFirstDevice();
        if (!device)
        {
            emit errorOccurred(QStringLiteral("未找到 Basler 相机。"));
            return false;
        }

        m_impl->camera.Attach(device);
        m_impl->camera.Open();

        if (!applyGrayscaleCapture())
            qWarning("CameraController::open: Mono8 未生效，已回退 RGB888 采集。");

        if (!applyResolution())
        {
            close(); // 分辨率设置失败时设备可能已 Open，需释放后再重试
            return false;
        }

        readParamLimits();
        m_open = true;
        return true;
    }
    catch (const GenericException &e)
    {
        emit errorOccurred(QString::fromLocal8Bit(e.GetDescription()));
        close();
        return false;
    }
#else
    Q_UNUSED(m_impl);
    emit errorOccurred(QStringLiteral("未配置 Pylon SDK。"));
    return false;
#endif
}

// 关闭设备并清空 latestImage；须先 stopGrab 再 Close/DestroyDevice
void CameraController::close()
{
    stopGrab();
#ifdef QT_PROJECT_USE_PYLON
    if (!m_pylonInitialized)
    {
        m_open = false;
        return;
    }

    try
    {
        if (m_impl->camera.IsOpen())
            m_impl->camera.Close();
        m_impl->camera.DestroyDevice();
    }
    catch (...)
    {
        qWarning("CameraController::close: 关闭相机设备异常已忽略。");
    }
    {
        QMutexLocker lock(&m_impl->imageMutex);
        m_impl->latestImage = QImage();
    }
#endif
    m_open = false;
}

bool CameraController::isOpen() const
{
    return m_open;
}

// 启动连续采集：Pylon LatestImageOnly + 独立 grabLoopWorker 线程取帧
bool CameraController::startGrab()
{
#ifdef QT_PROJECT_USE_PYLON
    if (!m_pylonInitialized || !m_open || m_grabbing)
        return false;

    try
    {
        {
            QMutexLocker lock(&m_impl->imageMutex);
            m_impl->latestImage = QImage();
        }
        m_impl->frameSequence.store(0, std::memory_order_relaxed);

        // 仅用户线程（grabLoopWorker）调用 RetrieveResult，避免与 Pylon 内置抓图循环冲突
        m_impl->camera.StartGrabbing(GrabStrategy_LatestImageOnly, GrabLoop_ProvidedByUser);
        m_impl->grabStop = false;
        m_impl->grabThread = std::thread(&CameraController::grabLoopWorker, this);
        m_grabbing = true;
        return true;
    }
    catch (const GenericException &e)
    {
        emit errorOccurred(QString::fromLocal8Bit(e.GetDescription()));
        return false;
    }
#else
    return false;
#endif
}

// 通知抓图线程退出，StopGrabbing 后 join，并清空 latestImage
void CameraController::stopGrab()
{
#ifdef QT_PROJECT_USE_PYLON
    if (!m_pylonInitialized || !m_grabbing)
        return;

    m_impl->grabStop = true;
    try
    {
        if (m_impl->camera.IsGrabbing())
            m_impl->camera.StopGrabbing();
    }
    catch (...)
    {
        qWarning("CameraController::stopGrab: StopGrabbing 异常已忽略。");
    }

    if (m_impl->grabThread.joinable())
        m_impl->grabThread.join();

    {
        QMutexLocker lock(&m_impl->imageMutex);
        m_impl->latestImage = QImage();
    }
#endif
    m_grabbing = false;
}

// 抓图线程主循环：100ms 超时取帧 → Grayscale8/RGB888 → 加锁更新 latestImage
void CameraController::grabLoopWorker()
{
#ifdef QT_PROJECT_USE_PYLON
    const bool mono = m_impl->captureLayout == Impl::CapturePixelLayout::Grayscale8;
    const int bytesPerPixel = mono ? 1 : 3;
    const QImage::Format qFormat = mono ? QImage::Format_Grayscale8 : QImage::Format_RGB888;

    while (!m_impl->grabStop.load(std::memory_order_relaxed) && m_impl->camera.IsGrabbing())
    {
        try
        {
            CGrabResultPtr grabResult;
            if (!m_impl->camera.RetrieveResult(100, grabResult, TimeoutHandling_Return))
                continue;
            if (!grabResult || !grabResult->GrabSucceeded())
                continue;

            CPylonImage target;
            m_impl->converter.Convert(target, grabResult);

            QImage img;
            if (!copyPylonImageToQImage(target, img, bytesPerPixel, qFormat))
                continue;

            QMutexLocker lock(&m_impl->imageMutex);
            m_impl->latestImage = std::move(img);
            m_impl->frameSequence.fetch_add(1, std::memory_order_relaxed);
        }
        catch (const GenericException &e)
        {
            // 抓图线程内以 QueuedConnection 投递 errorOccurred，避免目标对象已销毁时在同线程调用槽函数
            const QString msg = QString::fromLocal8Bit(e.GetDescription());
            QMetaObject::invokeMethod(this, "errorOccurred", Qt::QueuedConnection,
                                    Q_ARG(QString, msg));
            break;
        }
    }
#endif
}

// 写 ExposureTime 节点（微秒），值会被钳到相机合法范围
bool CameraController::setExposureUs(double us)
{
    return setFloatNode("ExposureTime", us);
}

// 优先写 Gain，部分型号只有 GainRaw
bool CameraController::setGainDb(double db)
{
    if (setFloatNode("Gain", db))
        return true;
    return setFloatNode("GainRaw", db);
}

// 开启 AcquisitionFrameRateEnable 后写入目标帧率
bool CameraController::setFps(double fps)
{
#ifdef QT_PROJECT_USE_PYLON
    if (!m_open)
        return false;
    try
    {
        CBooleanPtr enable(m_impl->camera.GetNodeMap().GetNode("AcquisitionFrameRateEnable"));
        if (enable && IsWritable(enable))
            enable->SetValue(true);
        return setFloatNode("AcquisitionFrameRate", fps);
    }
    catch (const GenericException &e)
    {
        emit errorOccurred(QString::fromLocal8Bit(e.GetDescription()));
        return false;
    }
#else
    Q_UNUSED(fps);
    return false;
#endif
}

CamParamLimits CameraController::paramLimits() const
{
    return m_limits;
}

// 主线程调用：在 imageMutex 下复制 latestImage，未采集或无帧返回 false
bool CameraController::copyLatestImage(QImage &out, quint64 *frameSeq)
{
#ifdef QT_PROJECT_USE_PYLON
    if (!m_grabbing)
        return false;

    QMutexLocker lock(&m_impl->imageMutex);
    if (m_impl->latestImage.isNull())
        return false;
    out = m_impl->latestImage.copy(); // 深拷贝，避免抓图线程更新缓冲区时与调用方发生读写冲突
    if (frameSeq)
        *frameSeq = m_impl->frameSequence.load(std::memory_order_relaxed);
    return true;
#else
    Q_UNUSED(out);
    Q_UNUSED(frameSeq);
    return false;
#endif
}

// 设置 Mono8 像素格式与 Pylon 转换器；设备不支持时回退 RGB888
bool CameraController::applyGrayscaleCapture()
{
#ifdef QT_PROJECT_USE_PYLON
    try
    {
        INodeMap &nodemap = m_impl->camera.GetNodeMap();
        CEnumerationPtr pixelFormat(nodemap.GetNode("PixelFormat"));
        if (pixelFormat && IsWritable(pixelFormat))
            pixelFormat->FromString("Mono8");

        m_impl->converter.OutputPixelFormat = PixelType_Mono8;
        m_impl->captureLayout = Impl::CapturePixelLayout::Grayscale8;
        return true;
    }
    catch (const GenericException &e)
    {
        qWarning("CameraController::applyGrayscaleCapture: Mono8 失败 (%s)，回退 RGB888。",
                 e.GetDescription());
    }
    catch (...)
    {
        qWarning("CameraController::applyGrayscaleCapture: Mono8 失败，回退 RGB888。");
    }

    try
    {
        m_impl->converter.OutputPixelFormat = PixelType_RGB8packed;
        m_impl->captureLayout = Impl::CapturePixelLayout::Rgb888;
    }
    catch (...)
    {
        return false;
    }
    return false;
#else
    return false;
#endif
}

// 将 Width/Height 设为 kImageWidth×kImageHeight，超出相机上限则取 max
bool CameraController::applyResolution()
{
#ifdef QT_PROJECT_USE_PYLON
    try
    {
        INodeMap &nodemap = m_impl->camera.GetNodeMap();

        CIntegerPtr width(nodemap.GetNode("Width"));
        CIntegerPtr height(nodemap.GetNode("Height"));
        if (width && IsWritable(width))
        {
            const int64_t maxW = width->GetMax();
            const int64_t wantW = kImageWidth;
            width->SetValue(wantW > maxW ? maxW : wantW);
        }
        if (height && IsWritable(height))
        {
            const int64_t maxH = height->GetMax();
            const int64_t wantH = kImageHeight;
            height->SetValue(wantH > maxH ? maxH : wantH);
        }
        return true;
    }
    catch (const GenericException &e)
    {
        emit errorOccurred(QString::fromLocal8Bit(e.GetDescription()));
        return false;
    }
#else
    return false;
#endif
}

// 从相机 GenICam 节点读回曝光/增益/帧率的 min/max，刷新界面 spin 范围
void CameraController::readParamLimits()
{
#ifdef QT_PROJECT_USE_PYLON
    try
    {
        INodeMap &nodemap = m_impl->camera.GetNodeMap();

        CFloatPtr exposure(nodemap.GetNode("ExposureTime"));
        if (exposure && IsReadable(exposure))
        {
            m_limits.exposureMinUs = exposure->GetMin();
            m_limits.exposureMaxUs = exposure->GetMax();
        }

        CFloatPtr gain(nodemap.GetNode("Gain"));
        if (!gain || !IsReadable(gain))
            gain = nodemap.GetNode("GainRaw");
        if (gain && IsReadable(gain))
        {
            m_limits.gainMinDb = gain->GetMin();
            m_limits.gainMaxDb = gain->GetMax();
        }

        CFloatPtr fps(nodemap.GetNode("AcquisitionFrameRate"));
        if (fps && IsReadable(fps))
        {
            m_limits.fpsMin = fps->GetMin();
            m_limits.fpsMax = fps->GetMax();
        }
    }
    catch (const GenericException &e)
    {
        emit errorOccurred(QString::fromLocal8Bit(e.GetDescription()));
    }
#endif
}

// 通用浮点节点写入：检查存在与可写，值钳到 [min,max]，失败时 errorOccurred
bool CameraController::setFloatNode(const char *nodeName, double value)
{
#ifdef QT_PROJECT_USE_PYLON
    if (!m_open)
        return false;
    try
    {
        CFloatPtr node(m_impl->camera.GetNodeMap().GetNode(nodeName));
        if (!node)
        {
            emit errorOccurred(QStringLiteral("相机节点不存在: %1")
                                   .arg(QString::fromLatin1(nodeName)));
            return false;
        }
        if (!IsWritable(node))
        {
            emit errorOccurred(QStringLiteral("相机节点不可写: %1")
                                   .arg(QString::fromLatin1(nodeName)));
            return false;
        }
        const double v = value < node->GetMin() ? node->GetMin()
                         : value > node->GetMax() ? node->GetMax()
                                                  : value;
        node->SetValue(v);
        return true;
    }
    catch (const GenericException &e)
    {
        emit errorOccurred(QString::fromLocal8Bit(e.GetDescription()));
        return false;
    }
#else
    Q_UNUSED(nodeName);
    Q_UNUSED(value);
    return false;
#endif
}
