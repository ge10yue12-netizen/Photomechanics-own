#include "../include/ScreenRecorder.h"

#include "Capture/DxgiScreenCapture.h"
#include "Capture/GdiScreenCapture.h"
#include "Capture/CaptureOpen.h"
#include "Encoder/MjpegAviEncoder.h"
#include "Encoder/Mp4MfEncoder.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

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

// 优先 DXGI，失败时回退 GDI（主屏 / 区域录制）。
bool tryOpenCapture(CaptureMode mode,
                    const Rect &region,
                    std::unique_ptr<IScreenCapture> *out,
                    std::string *error,
                    std::string *logHint)
{
    return capture::openScreenCapture(mode, region, out, error, logHint);
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
        if (!tryOpenCapture(config.mode, config.region, &m_capture, &capErr, &capHint))
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

        const int userW = config.video.outputWidth > 0 ? config.video.outputWidth : 0;
        const int userH = config.video.outputHeight > 0 ? config.video.outputHeight : 0;
        if (userW > 0 && userH > 0 && (userW != nativeW || userH != nativeH))
        {
            logMessage("输出分辨率 " + std::to_string(userW) + "x" + std::to_string(userH) +
                       " 与采集 " + std::to_string(nativeW) + "x" + std::to_string(nativeH) +
                       " 不一致，已改为 1:1 原生编码以保持清晰。");
        }

        m_outputWidth = nativeW;
        m_outputHeight = nativeH;
        m_config.video.outputWidth = nativeW;
        m_config.video.outputHeight = nativeH;

        const int effectiveBitrate =
            effectiveScreenBitrateKbps(config.video.bitrateKbps, nativeW, nativeH, config.video.fps);
        if (effectiveBitrate != config.video.bitrateKbps)
        {
            logMessage("码率已从 " + std::to_string(config.video.bitrateKbps) + " 提升至 " +
                       std::to_string(effectiveBitrate) + " kbps（屏幕录制建议下限）。");
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
                   std::to_string(m_config.video.bitrateKbps) + " kbps（1:1 原生像素）");

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

    void setCallback(const RecorderCallback &callback)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callback = callback;
    }

private:
    void workerLoop()
    {
        using clock = std::chrono::steady_clock;
        const auto frameInterval =
            std::chrono::microseconds(1'000'000 / fpsMax(m_config.video.fps));

        while (!m_stopRequested)
        {
            reportWallDuration();

            if (m_paused)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            const auto elapsed = activeElapsed();
            const int targetFrames =
                static_cast<int>(elapsed / frameInterval) + 1;

            if (m_frameCount >= targetFrames)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            CaptureFrame frame;
            std::string capErr;
            if (!m_capture || !m_capture->grab(&frame, &capErr))
            {
                reportError(capErr.empty() ? errorMessage(RecorderErrorCode::CaptureInitFailed) : capErr);
                m_stopRequested = true;
                break;
            }

            std::string encErr;
            if (!m_encoder || !m_encoder->writeFrame(frame, &encErr))
            {
                reportError(encErr.empty() ? errorMessage(RecorderErrorCode::WriteFileFailed) : encErr);
                m_stopRequested = true;
                break;
            }

            ++m_frameCount;
        }

        reportWallDuration();
    }

    std::chrono::steady_clock::duration activeElapsed() const
    {
        using clock = std::chrono::steady_clock;
        const auto now = clock::now();
        if (m_paused)
            return m_wallPauseBegin - m_wallStart - m_wallPausedTotal;
        return now - m_wallStart - m_wallPausedTotal;
    }

    void reportWallDuration()
    {
        const auto elapsed = activeElapsed();
        const int wholeSec =
            static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
        if (wholeSec == m_lastDurationReport)
            return;

        m_lastDurationReport = wholeSec;
        m_recordedSeconds.store(wholeSec);
        notifyDuration(wholeSec);
    }

    void releaseHardware()
    {
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

    void logMessage(const std::string &msg)
    {
        RecorderCallback cb;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            cb = m_callback;
        }
        if (cb.onLog)
            cb.onLog(msg);
    }

    void notifyState(RecorderState st)
    {
        RecorderCallback cb;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            cb = m_callback;
        }
        if (cb.onStateChanged)
            cb.onStateChanged(st);
    }

    void notifyDuration(int seconds)
    {
        RecorderCallback cb;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            cb = m_callback;
        }
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
        RecorderCallback cb;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            cb = m_callback;
        }
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

void ScreenRecorder::setCallback(const RecorderCallback &callback)
{
    m_impl->setCallback(callback);
}

} // namespace recorder
