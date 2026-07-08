#include "Mp4MfEncoder.h"

#include "../../include/RecorderPresets.h"

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <codecapi.h>
#include <oleauto.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "strmiids.lib")

namespace recorder
{
namespace
{

int evenDim(int v)
{
    return v > 0 ? (v & ~1) : 0;
}

std::wstring utf8ToWide(const std::string &text)
{
    if (text.empty())
        return std::wstring();
    const int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (len <= 0)
        return std::wstring();
    std::wstring wide(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wide[0], len);
    if (!wide.empty() && wide.back() == L'\0')
        wide.pop_back();
    return wide;
}

bool scaleBgraBilinear(const CaptureFrame &src, int dstW, int dstH, std::vector<std::uint8_t> *out)
{
    if (!out || dstW <= 0 || dstH <= 0 || src.width <= 0 || src.height <= 0)
        return false;
    if (src.width == dstW && src.height == dstH)
        return false;

    const int dstStride = dstW * 4;
    out->assign(static_cast<size_t>(dstStride) * static_cast<size_t>(dstH), 0);

    auto sample = [&src](float fx, float fy, int ch) -> std::uint8_t {
        fx = std::max(0.0f, std::min(fx, static_cast<float>(src.width - 1)));
        fy = std::max(0.0f, std::min(fy, static_cast<float>(src.height - 1)));
        const int x0 = static_cast<int>(fx);
        const int y0 = static_cast<int>(fy);
        const int x1 = std::min(x0 + 1, src.width - 1);
        const int y1 = std::min(y0 + 1, src.height - 1);
        const float tx = fx - static_cast<float>(x0);
        const float ty = fy - static_cast<float>(y0);

        const auto at = [&](int x, int y) -> float {
            const std::uint8_t *px =
                src.bgra.data() + static_cast<size_t>(y) * static_cast<size_t>(src.stride) + x * 4;
            return static_cast<float>(px[ch]);
        };

        const float v00 = at(x0, y0);
        const float v10 = at(x1, y0);
        const float v01 = at(x0, y1);
        const float v11 = at(x1, y1);
        const float top = v00 + (v10 - v00) * tx;
        const float bottom = v01 + (v11 - v01) * tx;
        const float v = top + (bottom - top) * ty;
        return static_cast<std::uint8_t>(std::max(0.0f, std::min(v, 255.0f)));
    };

    for (int y = 0; y < dstH; ++y)
    {
        const float srcY = (static_cast<float>(y) + 0.5f) * static_cast<float>(src.height) /
                               static_cast<float>(dstH) -
                           0.5f;
        std::uint8_t *dstRow = out->data() + static_cast<size_t>(y) * static_cast<size_t>(dstStride);
        for (int x = 0; x < dstW; ++x)
        {
            const float srcX = (static_cast<float>(x) + 0.5f) * static_cast<float>(src.width) /
                                   static_cast<float>(dstW) -
                               0.5f;
            std::uint8_t *dp = dstRow + x * 4;
            dp[0] = sample(srcX, srcY, 0);
            dp[1] = sample(srcX, srcY, 1);
            dp[2] = sample(srcX, srcY, 2);
            dp[3] = sample(srcX, srcY, 3);
        }
    }
    return true;
}

bool configureH264Quality(IMFSinkWriter *writer, DWORD streamIndex, int fps, int bitrateKbps,
                          EncodeLevel encodeLevel)
{
    if (!writer)
        return false;

    ICodecAPI *codecApi = nullptr;
    const HRESULT hrSvc = writer->GetServiceForStream(streamIndex, GUID_NULL, IID_ICodecAPI,
                                                      reinterpret_cast<void **>(&codecApi));
    if (FAILED(hrSvc) || !codecApi)
        return false;

    auto setUi4 = [&](const GUID &key, UINT32 value) {
        VARIANT var;
        VariantInit(&var);
        var.vt = VT_UI4;
        var.ulVal = value;
        const HRESULT hr = codecApi->SetValue(&key, &var);
        VariantClear(&var);
        return SUCCEEDED(hr);
    };

    int gopMultiplier = encodeGopMultiplier(encodeLevel);
    const double maxBitrateFactor = encodeMaxBitrateFactor(encodeLevel);

    setUi4(CODECAPI_AVEncCommonRateControlMode,
           static_cast<UINT32>(eAVEncCommonRateControlMode_UnconstrainedVBR));
    setUi4(CODECAPI_AVEncCommonMeanBitRate, static_cast<UINT32>(bitrateKbps) * 1000u);
    setUi4(CODECAPI_AVEncCommonMaxBitRate,
           static_cast<UINT32>(static_cast<double>(bitrateKbps) * 1000.0 * maxBitrateFactor));
    const int gop = encodeGopSize(encodeLevel, fps);
    setUi4(CODECAPI_AVEncMPVGOPSize, static_cast<UINT32>(gop));

    codecApi->Release();
    return true;
}

} // namespace

class Mp4MfEncoder::Impl
{
public:
    ~Impl() { close(nullptr); }

    bool open(const EncoderOpenParams &params, std::string *error)
    {
        close(nullptr);
        if (params.format != VideoFormat::Mp4)
        {
            if (error)
                *error = "MF 编码器仅支持 MP4。";
            return false;
        }

        m_width = evenDim(params.width);
        m_height = evenDim(params.height);
        m_fps = params.fps > 0 ? params.fps : 25;
        m_bitrateKbps = params.bitrateKbps > 0 ? params.bitrateKbps : 8000;
        m_encodeLevel = params.encodeLevel;
        m_frameIndex = 0;

        if (m_width < 100 || m_height < 100)
        {
            if (error)
                *error = "输出分辨率无效。";
            return false;
        }

        const std::wstring wpath = utf8ToWide(params.filePath);
        if (wpath.empty())
        {
            if (error)
                *error = "输出路径无效。";
            return false;
        }

        const HRESULT mfHr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
        if (FAILED(mfHr))
        {
            if (error)
                *error = "Media Foundation 初始化失败。";
            return false;
        }
        m_mfStarted = true;

        IMFAttributes *attrs = nullptr;
        if (FAILED(MFCreateAttributes(&attrs, 2)) || !attrs)
        {
            if (error)
                *error = "创建 MF 属性失败。";
            return false;
        }
        attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

        if (FAILED(MFCreateSinkWriterFromURL(wpath.c_str(), nullptr, attrs, &m_writer)) || !m_writer)
        {
            attrs->Release();
            if (error)
                *error = "创建 MP4 写入器失败，请检查路径与权限。";
            return false;
        }
        attrs->Release();

        IMFMediaType *outType = nullptr;
        IMFMediaType *inType = nullptr;
        if (FAILED(MFCreateMediaType(&outType)) || !outType ||
            FAILED(MFCreateMediaType(&inType)) || !inType)
        {
            if (outType)
                outType->Release();
            if (inType)
                inType->Release();
            if (error)
                *error = "创建媒体类型失败。";
            return false;
        }

        outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        MFSetAttributeSize(outType, MF_MT_FRAME_SIZE, static_cast<UINT32>(m_width), static_cast<UINT32>(m_height));
        MFSetAttributeRatio(outType, MF_MT_FRAME_RATE, static_cast<UINT32>(m_fps), 1);
        MFSetAttributeRatio(outType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        outType->SetUINT32(MF_MT_AVG_BITRATE, static_cast<UINT32>(m_bitrateKbps) * 1000u);
        outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        outType->SetUINT32(MF_MT_MPEG2_PROFILE, static_cast<UINT32>(eAVEncH264VProfile_High));

        inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        MFSetAttributeSize(inType, MF_MT_FRAME_SIZE, static_cast<UINT32>(m_width), static_cast<UINT32>(m_height));
        MFSetAttributeRatio(inType, MF_MT_FRAME_RATE, static_cast<UINT32>(m_fps), 1);
        MFSetAttributeRatio(inType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        inType->SetUINT32(MF_MT_DEFAULT_STRIDE, static_cast<UINT32>(m_width * 4));

        if (FAILED(m_writer->AddStream(outType, &m_streamIndex)) ||
            FAILED(m_writer->SetInputMediaType(m_streamIndex, inType, nullptr)) ||
            FAILED(m_writer->BeginWriting()))
        {
            outType->Release();
            inType->Release();
            if (error)
                *error = "配置 H.264 编码流失败。";
            return false;
        }

        configureH264Quality(m_writer, m_streamIndex, m_fps, m_bitrateKbps, m_encodeLevel);

        outType->Release();
        inType->Release();

        const LONG stride = m_width * 4;
        m_bufferSize = static_cast<DWORD>(static_cast<size_t>(stride) * static_cast<size_t>(m_height));
        if (FAILED(MFCreateMemoryBuffer(m_bufferSize, &m_mediaBuffer)) || !m_mediaBuffer)
        {
            if (error)
                *error = "创建复用媒体缓冲失败。";
            return false;
        }
        if (FAILED(MFCreateSample(&m_sample)) || !m_sample)
        {
            if (error)
                *error = "创建复用媒体样本失败。";
            return false;
        }

        m_open = true;
        m_hasCachedPixels = false;
        return true;
    }

    bool submitSample(std::string *error, int timelineSlots)
    {
        if (timelineSlots < 1)
            timelineSlots = 1;
        if (!m_hasCachedPixels)
        {
            if (error)
                *error = "尚无缓存帧，无法写入时间轴。";
            return false;
        }

        m_sample->RemoveAllBuffers();
        m_sample->AddBuffer(m_mediaBuffer);

        const LONGLONG unit = 10'000'000LL;
        const LONGLONG frameDur = unit / m_fps;
        const LONGLONG timestamp = m_frameIndex * frameDur;
        const LONGLONG duration = frameDur * static_cast<LONGLONG>(timelineSlots);
        m_sample->SetSampleTime(timestamp);
        m_sample->SetSampleDuration(duration);

        const HRESULT hr = m_writer->WriteSample(m_streamIndex, m_sample);
        if (FAILED(hr))
        {
            if (error)
                *error = "写入 MP4 帧失败。";
            return false;
        }

        m_frameIndex += timelineSlots;
        return true;
    }

    bool writeFrame(const CaptureFrame &frame, std::string *error, int timelineSlots)
    {
        if (!m_open || !m_writer || !m_mediaBuffer)
        {
            if (error)
                *error = "MP4 编码器未打开。";
            return false;
        }
        if (timelineSlots < 1)
            timelineSlots = 1;

        const bool sameSize = frame.width == m_width && frame.height == m_height;
        BYTE *data = nullptr;
        if (FAILED(m_mediaBuffer->Lock(&data, nullptr, nullptr)) || !data)
        {
            if (error)
                *error = "锁定媒体缓冲失败。";
            return false;
        }

        if (sameSize)
        {
            const LONG encStride = m_width * 4;
            if (frame.stride == encStride)
            {
                std::memcpy(data, frame.bgra.data(), static_cast<size_t>(m_bufferSize));
            }
            else
            {
                for (int y = 0; y < m_height; ++y)
                {
                    std::memcpy(data + static_cast<size_t>(y) * static_cast<size_t>(encStride),
                                frame.bgra.data() +
                                    static_cast<size_t>(y) * static_cast<size_t>(frame.stride),
                                static_cast<size_t>(encStride));
                }
            }
        }
        else
        {
            if (!scaleBgraBilinear(frame, m_width, m_height, &m_pixels))
            {
                m_mediaBuffer->Unlock();
                if (error)
                    *error = "缩放视频帧失败。";
                return false;
            }
            std::memcpy(data, m_pixels.data(), static_cast<size_t>(m_bufferSize));
        }

        m_mediaBuffer->Unlock();
        m_mediaBuffer->SetCurrentLength(m_bufferSize);
        m_hasCachedPixels = true;
        return submitSample(error, timelineSlots);
    }

    bool writeCachedTimeline(std::string *error, int timelineSlots)
    {
        if (!m_open || !m_writer || !m_mediaBuffer)
        {
            if (error)
                *error = "MP4 编码器未打开。";
            return false;
        }
        return submitSample(error, timelineSlots);
    }

    bool close(std::string *error)
    {
        bool ok = true;
        if (m_writer && m_open)
        {
            const HRESULT hr = m_writer->Finalize();
            if (FAILED(hr))
            {
                ok = false;
                if (error)
                    *error = "Finalize MP4 文件失败。";
            }
        }

        if (m_writer)
        {
            m_writer->Release();
            m_writer = nullptr;
        }
        if (m_mediaBuffer)
        {
            m_mediaBuffer->Release();
            m_mediaBuffer = nullptr;
        }
        if (m_sample)
        {
            m_sample->Release();
            m_sample = nullptr;
        }
        if (m_mfStarted)
        {
            MFShutdown();
            m_mfStarted = false;
        }
        m_open = false;
        m_frameIndex = 0;
        m_hasCachedPixels = false;
        m_pixels.clear();
        return ok;
    }

private:
    IMFSinkWriter *m_writer = nullptr;
    IMFMediaBuffer *m_mediaBuffer = nullptr;
    IMFSample *m_sample = nullptr;
    DWORD m_streamIndex = 0;
    int m_width = 0;
    int m_height = 0;
    int m_fps = 25;
    int m_bitrateKbps = 8000;
    EncodeLevel m_encodeLevel = EncodeLevel::Default;
    DWORD m_bufferSize = 0;
    LONGLONG m_frameIndex = 0;
    bool m_open = false;
    bool m_mfStarted = false;
    bool m_hasCachedPixels = false;
    std::vector<std::uint8_t> m_pixels;
};

Mp4MfEncoder::Mp4MfEncoder()
    : m_impl(new Impl())
{
}

Mp4MfEncoder::~Mp4MfEncoder()
{
    delete m_impl;
}

bool Mp4MfEncoder::open(const EncoderOpenParams &params, std::string *error)
{
    return m_impl->open(params, error);
}

bool Mp4MfEncoder::writeFrame(const CaptureFrame &frame, std::string *error, int timelineSlots)
{
    return m_impl->writeFrame(frame, error, timelineSlots);
}

bool Mp4MfEncoder::writeCachedTimeline(std::string *error, int timelineSlots)
{
    return m_impl->writeCachedTimeline(error, timelineSlots);
}

bool Mp4MfEncoder::close(std::string *error)
{
    return m_impl->close(error);
}

} // namespace recorder
