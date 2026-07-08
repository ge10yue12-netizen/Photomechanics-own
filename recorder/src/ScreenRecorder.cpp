#include "../include/ScreenRecorder.h"

#include "Capture/CaptureOpen.h"
#include "Capture/FrameCompare.h"
#include "Encoder/MjpegAviEncoder.h"
#include "Encoder/Mp4MfEncoder.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace recorder
{
namespace
{

int fpsMax(int fps)
{
    return fps > 1 ? fps : 1;
}

std::unique_ptr<IVideoEncoder> createEncoder(VideoFormat format)
{
    if (format == VideoFormat::Mp4)
        return std::unique_ptr<IVideoEncoder>(new Mp4MfEncoder());
    if (format == VideoFormat::Avi)
        return std::unique_ptr<IVideoEncoder>(new MjpegAviEncoder());
    return nullptr;
}

} // namespace

class ScreenRecorder::Impl
{
public:
    ~Impl() { shutdownWorkers(); releaseHardware(); }

    bool init(const RecorderConfig &config)
    {
        shutdownWorkers();
        RecorderErrorCode code = RecorderErrorCode::None;
        if (!validateConfig(config, &code, &m_lastError))
        {
            setState(RecorderState::Error);
            return false;
        }

        m_config = config;
        std::string capErr;
        std::string capHint;
        if (!capture::openScreenCapture(config.mode,
                                        config.region,
                                        config.windowTarget,
                                        &m_capture,
                                        &capErr,
                                        &capHint))
        {
            m_lastError = capErr.empty() ? errorMessage(RecorderErrorCode::CaptureInitFailed)
                                         : capErr;
            m_capture.reset();
            setState(RecorderState::Error);
            return false;
        }
        if (!capHint.empty())
        {
            if (capHint == "DXGI")
                logMessage("屏幕采集：DXGI（录制物理像素 1:1；预览为 GDI 缩略图）");
            else if (capHint == "GDI")
                logMessage("屏幕采集：GDI（DXGI 不可用，与预览同路径）");
            else
                logMessage("屏幕采集：" + capHint);
        }

        const int capW = m_capture->captureWidth();
        const int capH = m_capture->captureHeight();
        logMessage("采集尺寸：" + std::to_string(capW) + "x" + std::to_string(capH) +
                   " 物理像素（默认即当前显示器桌面区域）");
        int nativeW = capW;
        int nativeH = capH;
        if (config.output.format == VideoFormat::Mp4)
        {
            nativeW &= ~1;
            nativeH &= ~1;
        }

        int outW = config.video.outputWidth > 0 ? config.video.outputWidth : nativeW;
        int outH = config.video.outputHeight > 0 ? config.video.outputHeight : nativeH;
        if (config.output.format == VideoFormat::Mp4)
        {
            outW &= ~1;
            outH &= ~1;
        }
        if (outW < 100 || outH < 100)
        {
            outW = nativeW;
            outH = nativeH;
        }

        if (outW != nativeW || outH != nativeH)
        {
            logMessage("输出尺寸 " + std::to_string(outW) + "x" + std::to_string(outH) +
                       "（采集 " + std::to_string(nativeW) + "x" + std::to_string(nativeH) +
                       "，画质档位缩放）。");
        }

        m_outputWidth = outW;
        m_outputHeight = outH;
        m_config.video.outputWidth = outW;
        m_config.video.outputHeight = outH;

        const int effectiveBitrate =
            effectiveScreenBitrateKbps(config.video.bitrateKbps, outW, outH, config.video.fps);
        if (effectiveBitrate != config.video.bitrateKbps)
        {
            logMessage("码率已调整为 " + std::to_string(effectiveBitrate) + " kbps（画质档位安全下限）。");
            m_config.video.bitrateKbps = effectiveBitrate;
        }

        m_encoder = createEncoder(config.output.format);
        if (!m_encoder)
        {
            m_lastError = errorMessage(RecorderErrorCode::UnsupportedFormat);
            m_capture->close();
            m_capture.reset();
            setState(RecorderState::Error);
            return false;
        }

        EncoderOpenParams encParams;
        encParams.filePath = config.output.filePath;
        encParams.format = config.output.format;
        encParams.width = m_outputWidth;
        encParams.height = m_outputHeight;
        encParams.fps = config.video.fps;
        encParams.bitrateKbps = m_config.video.bitrateKbps;
        encParams.encodeLevel = config.video.encodeLevel;

        std::string encErr;
        if (!m_encoder->open(encParams, &encErr))
        {
            m_lastError = encErr.empty() ? errorMessage(RecorderErrorCode::EncoderInitFailed) : encErr;
            m_encoder.reset();
            m_capture->close();
            m_capture.reset();
            setState(RecorderState::Error);
            return false;
        }

        logMessage("编码参数：" + std::to_string(m_outputWidth) + "x" + std::to_string(m_outputHeight) +
                   " @ " + std::to_string(config.video.fps) + " FPS, " +
                   std::to_string(m_config.video.bitrateKbps) + " kbps");

        m_grabBuffer.bgra.clear();
        m_grabBuffer.bgra.reserve(static_cast<size_t>(m_outputWidth) * static_cast<size_t>(m_outputHeight) * 4u);

        m_pendingHoldSlots = 0;
        m_hasEncodedFrame = false;
        m_frameCount = 0;
        m_lastEncodedFrame = CaptureFrame{};
        m_lastError.clear();
        m_recordedSeconds = 0;
        setState(RecorderState::Initialized);
        logMessage("录制模块已初始化。");
        return true;
    }

    bool start()
    {
        const RecorderState st = state();
        if (st != RecorderState::Initialized && st != RecorderState::Stopped)
        {
            failInvalidState();
            return false;
        }
        if (st == RecorderState::Stopped)
        {
            if (!init(m_config))
                return false;
        }

        m_stopRequested = false;
        m_paused = false;
        m_recordedSeconds = 0;
        m_lastDurationReport = -1;
        m_frameCount = 0;
        m_wallStart = std::chrono::steady_clock::now();
        m_wallPausedTotal = std::chrono::steady_clock::duration::zero();
        m_wallPauseBegin = m_wallStart;

        try
        {
            m_worker = std::thread(&Impl::workerLoop, this);
        }
        catch (...)
        {
            m_lastError = errorMessage(RecorderErrorCode::ThreadStartFailed);
            setState(RecorderState::Error);
            return false;
        }

        setState(RecorderState::Recording);
        logMessage("开始录制。");
        return true;
    }

    bool pause()
    {
        if (state() != RecorderState::Recording)
        {
            failInvalidState();
            return false;
        }
        m_paused = true;
        m_wallPauseBegin = std::chrono::steady_clock::now();
        setState(RecorderState::Paused);
        logMessage("录制已暂停。");
        return true;
    }

    bool resume()
    {
        if (state() != RecorderState::Paused)
        {
            failInvalidState();
            return false;
        }
        m_paused = false;
        m_wallPausedTotal += std::chrono::steady_clock::now() - m_wallPauseBegin;
        setState(RecorderState::Recording);
        logMessage("继续录制。");
        return true;
    }

    bool stop()
    {
        const RecorderState st = state();
        if (st == RecorderState::Idle)
            return true;

        if (st == RecorderState::Initialized)
        {
            releaseHardware();
            setState(RecorderState::Stopped);
            return true;
        }

        if (st == RecorderState::Error)
        {
            shutdownWorkers();
            releaseHardware();
            setState(RecorderState::Stopped);
            return true;
        }

        if (st != RecorderState::Recording && st != RecorderState::Paused)
        {
            failInvalidState();
            return false;
        }

        m_stopRequested = true;
        m_paused = false;

        shutdownWorkers();
        releaseHardware();

        m_recordedSeconds.store(elapsedWallSeconds());
        m_lastDurationReport = m_recordedSeconds.load();

        setState(RecorderState::Stopped);
        logMessage("录制已停止，文件已保存。");
        return true;
    }

    RecorderState state() const
    {
        return m_state.load();
    }

    std::string lastError() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lastError;
    }

    int recordedSeconds() const
    {
        return m_recordedSeconds.load();
    }

    int mediaTimelineSeconds() const
    {
        return mediaSecondsFromSlots();
    }

    void setCallback(const RecorderCallback &callback)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callback = callback;
    }

private:
    int mediaSecondsFromSlots() const
    {
        const int fps = fpsMax(m_config.video.fps);
        return fps > 0 ? m_frameCount / fps : 0;
    }

    // 与视频时间轴一致：elapsed 内应完成的帧槽数（首帧立即开始，无 +1 偏差）。
    int targetFrameSlots(const std::chrono::steady_clock::duration &elapsed,
                         const std::chrono::microseconds &frameInterval) const
    {
        const long long slots =
            std::max(1LL, static_cast<long long>(elapsed / frameInterval));
        return static_cast<int>(slots);
    }

    void workerLoop()
    {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

        using clock = std::chrono::steady_clock;
        const auto frameInterval =
            std::chrono::microseconds(1'000'000 / fpsMax(m_config.video.fps));

        while (!m_stopRequested)
        {
            reportMediaDuration();

            if (m_paused)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            const auto elapsed = activeElapsed();
            const int targetFrames = targetFrameSlots(elapsed, frameInterval);

            if (m_frameCount >= targetFrames)
            {
                const auto nextSlotEnd = m_wallStart + m_wallPausedTotal +
                                         frameInterval * static_cast<int64_t>(m_frameCount + 1);
                const auto now = clock::now();
                if (now < nextSlotEnd)
                    std::this_thread::sleep_until(nextSlotEnd);
                continue;
            }

            if (m_frameCount + 1 < targetFrames)
            {
                const int gap = targetFrames - 1 - m_frameCount;
                if (gap > 0)
                {
                    if (m_hasEncodedFrame)
                        m_pendingHoldSlots += gap;
                    m_frameCount += gap;
                }
            }

            m_grabBuffer.hasNewPixels = true;
            std::string capErr;
            if (!m_capture || !m_capture->grab(&m_grabBuffer, &capErr))
            {
                reportError(capErr.empty() ? errorMessage(RecorderErrorCode::CaptureInitFailed) : capErr);
                m_stopRequested = true;
                break;
            }

            bool contentChanged = m_grabBuffer.hasNewPixels;
            if (contentChanged && m_hasEncodedFrame &&
                !capture::framesVisuallyChanged(m_grabBuffer, m_lastEncodedFrame, m_changeParams))
            {
                contentChanged = false;
            }

            if (!contentChanged && m_hasEncodedFrame)
            {
                ++m_pendingHoldSlots;
                ++m_frameCount;
                continue;
            }

            int timelineSlots;
            if (!m_hasEncodedFrame)
                timelineSlots = std::max(1, m_frameCount);
            else
                timelineSlots = m_pendingHoldSlots + 1;
            m_pendingHoldSlots = 0;

            std::string encErr;
            if (!m_encoder || !m_encoder->writeFrame(m_grabBuffer, &encErr, timelineSlots))
            {
                reportError(encErr.empty() ? errorMessage(RecorderErrorCode::WriteFileFailed) : encErr);
                m_stopRequested = true;
                break;
            }

            capture::copyFramePixels(m_grabBuffer, &m_lastEncodedFrame);
            m_hasEncodedFrame = true;
            ++m_frameCount;
        }

        reportMediaDuration();
    }

    void flushPendingTimeline()
    {
        if (m_pendingHoldSlots <= 0 || !m_encoder || !m_hasEncodedFrame)
            return;

        std::string encErr;
        if (!m_encoder->writeCachedTimeline(&encErr, m_pendingHoldSlots) && !encErr.empty())
            m_lastError = encErr;
        m_pendingHoldSlots = 0;
    }

    std::chrono::steady_clock::duration activeElapsed() const
    {
        using clock = std::chrono::steady_clock;
        const auto now = clock::now();
        if (m_paused)
            return m_wallPauseBegin - m_wallStart - m_wallPausedTotal;
        return now - m_wallStart - m_wallPausedTotal;
    }

    int elapsedWallSeconds() const
    {
        const auto elapsed = activeElapsed();
        return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
    }

    void reportMediaDuration()
    {
        if (m_stopRequested.load())
            return;

        const int wholeSec = elapsedWallSeconds();
        if (wholeSec == m_lastDurationReport)
            return;

        m_lastDurationReport = wholeSec;
        m_recordedSeconds.store(wholeSec);
        notifyDuration(wholeSec);
    }

    void releaseHardware()
    {
        flushPendingTimeline();

        std::string encErr;
        if (m_encoder)
        {
            if (!m_encoder->close(&encErr) && !encErr.empty())
                m_lastError = encErr;
            m_encoder.reset();
        }
        if (m_capture)
        {
            m_capture->close();
            m_capture.reset();
        }
    }

    void shutdownWorkers()
    {
        m_stopRequested = true;
        if (m_worker.joinable())
            m_worker.join();
    }

    void setState(RecorderState st)
    {
        m_state = st;
        notifyState(st);
    }

    void failInvalidState()
    {
        m_lastError = errorMessage(RecorderErrorCode::InvalidState);
    }

    RecorderCallback lockedCallback()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_callback;
    }

    void logMessage(const std::string &msg)
    {
        const RecorderCallback cb = lockedCallback();
        if (cb.onLog)
            cb.onLog(msg);
    }

    void notifyState(RecorderState st)
    {
        const RecorderCallback cb = lockedCallback();
        if (cb.onStateChanged)
            cb.onStateChanged(st);
    }

    void notifyDuration(int seconds)
    {
        const RecorderCallback cb = lockedCallback();
        if (cb.onDurationChanged)
            cb.onDurationChanged(seconds);
    }

    void reportError(const std::string &msg)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_lastError = msg;
        }
        notifyError(msg);
        setState(RecorderState::Error);
    }

    void notifyError(const std::string &msg)
    {
        const RecorderCallback cb = lockedCallback();
        if (cb.onError)
            cb.onError(msg);
    }

    std::unique_ptr<IScreenCapture> m_capture;
    std::unique_ptr<IVideoEncoder> m_encoder;
    RecorderConfig m_config;
    RecorderCallback m_callback;
    mutable std::mutex m_mutex;
    std::thread m_worker;
    std::atomic<RecorderState> m_state{RecorderState::Idle};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_paused{false};
    std::atomic<int> m_recordedSeconds{0};
    int m_lastDurationReport = -1;
    int m_frameCount = 0;
    int m_outputWidth = 0;
    int m_outputHeight = 0;
    std::chrono::steady_clock::time_point m_wallStart;
    std::chrono::steady_clock::time_point m_wallPauseBegin;
    std::chrono::steady_clock::duration m_wallPausedTotal;
    std::string m_lastError;
    CaptureFrame m_grabBuffer;
    CaptureFrame m_lastEncodedFrame;
    capture::FrameChangeParams m_changeParams;
    int m_pendingHoldSlots = 0;
    bool m_hasEncodedFrame = false;
};

ScreenRecorder::ScreenRecorder()
    : m_impl(new Impl())
{
}

ScreenRecorder::~ScreenRecorder()
{
    delete m_impl;
}

bool ScreenRecorder::init(const RecorderConfig &config)
{
    return m_impl->init(config);
}

bool ScreenRecorder::start()
{
    return m_impl->start();
}

bool ScreenRecorder::pause()
{
    return m_impl->pause();
}

bool ScreenRecorder::resume()
{
    return m_impl->resume();
}

bool ScreenRecorder::stop()
{
    return m_impl->stop();
}

RecorderState ScreenRecorder::state() const
{
    return m_impl->state();
}

std::string ScreenRecorder::lastError() const
{
    return m_impl->lastError();
}

int ScreenRecorder::recordedSeconds() const
{
    return m_impl->recordedSeconds();
}

int ScreenRecorder::mediaTimelineSeconds() const
{
    return m_impl->mediaTimelineSeconds();
}

void ScreenRecorder::setCallback(const RecorderCallback &callback)
{
    m_impl->setCallback(callback);
}

} // namespace recorder
