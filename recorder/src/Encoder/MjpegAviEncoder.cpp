#include "MjpegAviEncoder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")

namespace recorder
{
namespace
{

int clampInt(int value, int lo, int hi)
{
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

struct AviMainHeader
{
    std::uint32_t microSecPerFrame = 0;
    std::uint32_t maxBytesPerSec = 0;
    std::uint32_t paddingGranularity = 0;
    std::uint32_t flags = 0x00000010;
    std::uint32_t totalFrames = 0;
    std::uint32_t initialFrames = 0;
    std::uint32_t streams = 1;
    std::uint32_t suggestedBufferSize = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t reserved[4] = {};
};

struct AviStreamHeader
{
    char fccType[4] = {'v', 'i', 'd', 's'};
    char fccHandler[4] = {'M', 'J', 'P', 'G'};
    std::uint32_t flags = 0;
    std::uint16_t priority = 0;
    std::uint16_t language = 0;
    std::uint32_t initialFrames = 0;
    std::uint32_t scale = 1;
    std::uint32_t rate = 30;
    std::uint32_t start = 0;
    std::uint32_t length = 0;
    std::uint32_t suggestedBufferSize = 0;
    std::uint32_t quality = 0xFFFFFFFFu;
    std::uint32_t sampleSize = 0;
    short rectLeft = 0;
    short rectTop = 0;
    short rectRight = 0;
    short rectBottom = 0;
};

struct BitmapInfoHeader
{
    std::uint32_t size = 40;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint16_t planes = 1;
    std::uint16_t bitCount = 24;
    std::uint32_t compression = 0;
    std::uint32_t sizeImage = 0;
    std::int32_t xPelsPerMeter = 0;
    std::int32_t yPelsPerMeter = 0;
    std::uint32_t clrUsed = 0;
    std::uint32_t clrImportant = 0;
};

void writeFourCC(std::FILE *f, const char *code)
{
    std::fwrite(code, 1, 4, f);
}

void writeU32(std::FILE *f, std::uint32_t v)
{
    std::fwrite(&v, 1, 4, f);
}

std::uint32_t padToWord(std::uint32_t size)
{
    return (size + 1u) & ~1u;
}

bool encodeJpeg(IWICImagingFactory *factory,
                const CaptureFrame &frame,
                int targetW,
                int targetH,
                int quality,
                std::vector<std::uint8_t> *jpeg,
                std::string *error)
{
    if (!factory || !jpeg)
        return false;

    IWICBitmap *srcBitmap = nullptr;
    const std::uint32_t srcW = static_cast<std::uint32_t>(frame.width);
    const std::uint32_t srcH = static_cast<std::uint32_t>(frame.height);
    const std::uint32_t stride = static_cast<std::uint32_t>(frame.stride);
    const HRESULT hrCreate = factory->CreateBitmapFromMemory(srcW,
                                                             srcH,
                                                             GUID_WICPixelFormat32bppBGRA,
                                                             stride,
                                                             stride * srcH,
                                                             const_cast<std::uint8_t *>(frame.bgra.data()),
                                                             &srcBitmap);
    if (FAILED(hrCreate) || !srcBitmap)
    {
        if (error)
            *error = "创建 WIC 位图失败。";
        return false;
    }

    IWICBitmapSource *source = srcBitmap;
    IWICBitmapScaler *scaler = nullptr;
    if (targetW > 0 && targetH > 0 &&
        (targetW != frame.width || targetH != frame.height))
    {
        if (FAILED(factory->CreateBitmapScaler(&scaler)) || !scaler)
        {
            srcBitmap->Release();
            if (error)
                *error = "创建 WIC 缩放器失败。";
            return false;
        }
        if (FAILED(scaler->Initialize(srcBitmap,
                                      static_cast<UINT>(targetW),
                                      static_cast<UINT>(targetH),
                                      WICBitmapInterpolationModeFant)))
        {
            scaler->Release();
            srcBitmap->Release();
            if (error)
                *error = "缩放视频帧失败。";
            return false;
        }
        source = scaler;
    }

    IStream *memStream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &memStream)) || !memStream)
    {
        if (scaler)
            scaler->Release();
        srcBitmap->Release();
        if (error)
            *error = "创建内存流失败。";
        return false;
    }

    IWICStream *wicStream = nullptr;
    if (FAILED(factory->CreateStream(&wicStream)) || !wicStream)
    {
        memStream->Release();
        if (scaler)
            scaler->Release();
        srcBitmap->Release();
        if (error)
            *error = "创建 WIC 流失败。";
        return false;
    }
    wicStream->InitializeFromIStream(memStream);

    IWICBitmapEncoder *encoder = nullptr;
    if (FAILED(factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder)) || !encoder)
    {
        wicStream->Release();
        memStream->Release();
        if (scaler)
            scaler->Release();
        srcBitmap->Release();
        if (error)
            *error = "创建 JPEG 编码器失败。";
        return false;
    }
    encoder->Initialize(wicStream, WICBitmapEncoderNoCache);

    IWICBitmapFrameEncode *frameEncode = nullptr;
    IPropertyBag2 *props = nullptr;
    encoder->CreateNewFrame(&frameEncode, &props);
    if (!frameEncode)
    {
        encoder->Release();
        wicStream->Release();
        memStream->Release();
        if (scaler)
            scaler->Release();
        srcBitmap->Release();
        if (error)
            *error = "创建 JPEG 帧编码器失败。";
        return false;
    }

    if (props)
    {
        PROPBAG2 option = {};
        option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT var;
        VariantInit(&var);
        var.vt = VT_R4;
        var.fltVal = static_cast<float>(clampInt(quality, 1, 100)) / 100.0f;
        props->Write(1, &option, &var);
        VariantClear(&var);
        props->Release();
    }

    frameEncode->Initialize(nullptr);
    UINT w = 0;
    UINT h = 0;
    source->GetSize(&w, &h);
    frameEncode->SetSize(w, h);
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    frameEncode->SetPixelFormat(&format);
    frameEncode->WriteSource(source, nullptr);
    frameEncode->Commit();
    encoder->Commit();

    STATSTG stat = {};
    memStream->Stat(&stat, STATFLAG_NONAME);
    const LARGE_INTEGER zero = {};
    memStream->Seek(zero, STREAM_SEEK_SET, nullptr);
    jpeg->resize(static_cast<size_t>(stat.cbSize.QuadPart));
    ULONG read = 0;
    memStream->Read(jpeg->data(), static_cast<ULONG>(jpeg->size()), &read);

    frameEncode->Release();
    encoder->Release();
    wicStream->Release();
    memStream->Release();
    if (scaler)
        scaler->Release();
    srcBitmap->Release();

    if (jpeg->empty())
    {
        if (error)
            *error = "JPEG 编码结果为空。";
        return false;
    }
    return true;
}

} // namespace

class MjpegAviEncoder::Impl
{
public:
    ~Impl() { close(nullptr); }

    bool open(const EncoderOpenParams &params, std::string *error)
    {
        close(nullptr);
        if (params.format != VideoFormat::Avi)
        {
            if (error)
                *error = "MJPEG 编码器仅支持 AVI。";
            return false;
        }

        m_width = params.width;
        m_height = params.height;
        m_fps = params.fps > 0 ? params.fps : 30;
        m_bitrateKbps = params.bitrateKbps;
        m_filePath = params.filePath;

        const HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (coHr == RPC_E_CHANGED_MODE)
            m_comInited = false;
        else
            m_comInited = SUCCEEDED(coHr) || coHr == S_FALSE;

        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&m_factory))) ||
            !m_factory)
        {
            if (error)
                *error = "WIC 组件初始化失败。";
            return false;
        }

        m_file = std::fopen(m_filePath.c_str(), "wb");
        if (!m_file)
        {
            if (error)
                *error = "无法创建输出文件。";
            return false;
        }

        writeFourCC(m_file, "RIFF");
        m_riffSizePos = std::ftell(m_file);
        writeU32(m_file, 0);
        writeFourCC(m_file, "AVI ");
        writeFourCC(m_file, "LIST");
        m_hdrlSizePos = std::ftell(m_file);
        writeU32(m_file, 0);
        writeFourCC(m_file, "hdrl");

        AviMainHeader avih = {};
        avih.microSecPerFrame = 1'000'000u / static_cast<std::uint32_t>(m_fps);
        avih.width = static_cast<std::uint32_t>(m_width);
        avih.height = static_cast<std::uint32_t>(m_height);
        avih.maxBytesPerSec = static_cast<std::uint32_t>(m_bitrateKbps) * 1000u / 8u;
        m_avihPos = std::ftell(m_file);
        writeFourCC(m_file, "avih");
        writeU32(m_file, sizeof(AviMainHeader));
        std::fwrite(&avih, 1, sizeof(avih), m_file);

        writeFourCC(m_file, "LIST");
        m_strlSizePos = std::ftell(m_file);
        writeU32(m_file, 0);
        writeFourCC(m_file, "strl");

        AviStreamHeader strh = {};
        strh.rate = static_cast<std::uint32_t>(m_fps);
        strh.scale = 1;
        strh.suggestedBufferSize = static_cast<std::uint32_t>(m_width * m_height * 3);
        strh.rectRight = static_cast<short>(m_width);
        strh.rectBottom = static_cast<short>(m_height);
        writeFourCC(m_file, "strh");
        writeU32(m_file, sizeof(AviStreamHeader));
        std::fwrite(&strh, 1, sizeof(strh), m_file);
        m_strhLengthPos = std::ftell(m_file) - 4;

        BitmapInfoHeader strf = {};
        strf.width = m_width;
        strf.height = m_height;
        strf.sizeImage = static_cast<std::uint32_t>(m_width * m_height * 3);
        writeFourCC(m_file, "strf");
        writeU32(m_file, sizeof(BitmapInfoHeader));
        std::fwrite(&strf, 1, sizeof(strf), m_file);

        const long hdrlEnd = std::ftell(m_file);
        const long strlStart = m_strlSizePos + 4;
        std::fseek(m_file, m_strlSizePos, SEEK_SET);
        writeU32(m_file, static_cast<std::uint32_t>(hdrlEnd - strlStart));
        const long hdrlStart = m_hdrlSizePos + 4;
        std::fseek(m_file, m_hdrlSizePos, SEEK_SET);
        writeU32(m_file, static_cast<std::uint32_t>(hdrlEnd - hdrlStart));
        std::fseek(m_file, hdrlEnd, SEEK_SET);

        writeFourCC(m_file, "LIST");
        m_moviSizePos = std::ftell(m_file);
        writeU32(m_file, 0);
        writeFourCC(m_file, "movi");
        m_moviStart = std::ftell(m_file);
        m_open = true;
        return true;
    }

    bool writeFrame(const CaptureFrame &frame, std::string *error)
    {
        if (!m_open || !m_file)
        {
            if (error)
                *error = "编码器未打开。";
            return false;
        }

        std::vector<std::uint8_t> jpeg;
        const int quality = clampInt(m_bitrateKbps / 100, 50, 95);
        if (!encodeJpeg(m_factory, frame, m_width, m_height, quality, &jpeg, error))
            return false;

        const std::uint32_t chunkSize = static_cast<std::uint32_t>(jpeg.size());
        const std::uint32_t padded = padToWord(chunkSize);
        const long offset = std::ftell(m_file) - m_moviStart + 4;

        writeFourCC(m_file, "00dc");
        writeU32(m_file, chunkSize);
        std::fwrite(jpeg.data(), 1, jpeg.size(), m_file);
        if (padded > chunkSize)
        {
            const char pad = 0;
            std::fwrite(&pad, 1, padded - chunkSize, m_file);
        }

        IndexEntry entry;
        entry.offset = static_cast<std::uint32_t>(offset);
        entry.size = chunkSize;
        m_index.push_back(entry);
        ++m_frameCount;
        return true;
    }

    bool close(std::string *error)
    {
        (void)error;
        if (!m_open || !m_file)
        {
            releaseFactory();
            return true;
        }

        const long fileEnd = std::ftell(m_file);
        std::fseek(m_file, m_moviSizePos, SEEK_SET);
        writeU32(m_file, static_cast<std::uint32_t>(fileEnd - m_moviStart + 4));

        AviMainHeader avih = {};
        avih.microSecPerFrame = 1'000'000u / static_cast<std::uint32_t>(m_fps);
        avih.flags = 0x00000010;
        avih.totalFrames = m_frameCount;
        avih.streams = 1;
        avih.width = static_cast<std::uint32_t>(m_width);
        avih.height = static_cast<std::uint32_t>(m_height);
        avih.maxBytesPerSec = static_cast<std::uint32_t>(m_bitrateKbps) * 1000u / 8u;
        std::fseek(m_file, m_avihPos + 8, SEEK_SET);
        std::fwrite(&avih.microSecPerFrame, 1, sizeof(AviMainHeader) - offsetof(AviMainHeader, microSecPerFrame), m_file);

        std::fseek(m_file, m_strhLengthPos, SEEK_SET);
        writeU32(m_file, m_frameCount);

        std::fseek(m_file, fileEnd, SEEK_SET);
        writeFourCC(m_file, "idx1");
        writeU32(m_file, static_cast<std::uint32_t>(m_index.size() * 16u));
        for (const IndexEntry &e : m_index)
        {
            writeFourCC(m_file, "00dc");
            writeU32(m_file, 0x00000010u);
            writeU32(m_file, e.offset);
            writeU32(m_file, e.size);
        }

        const long finalEnd = std::ftell(m_file);
        std::fseek(m_file, m_riffSizePos, SEEK_SET);
        writeU32(m_file, static_cast<std::uint32_t>(finalEnd - m_riffSizePos - 4));

        std::fclose(m_file);
        m_file = nullptr;
        m_open = false;
        m_index.clear();
        releaseFactory();
        return true;
    }

private:
    struct IndexEntry
    {
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
    };

    void releaseFactory()
    {
        if (m_factory)
        {
            m_factory->Release();
            m_factory = nullptr;
        }
        if (m_comInited)
        {
            CoUninitialize();
            m_comInited = false;
        }
    }

    IWICImagingFactory *m_factory = nullptr;
    std::FILE *m_file = nullptr;
    std::string m_filePath;
    int m_width = 0;
    int m_height = 0;
    int m_fps = 30;
    int m_bitrateKbps = 4000;
    long m_riffSizePos = 0;
    long m_hdrlSizePos = 0;
    long m_strlSizePos = 0;
    long m_moviSizePos = 0;
    long m_moviStart = 0;
    long m_avihPos = 0;
    long m_strhLengthPos = 0;
    std::uint32_t m_frameCount = 0;
    std::vector<IndexEntry> m_index;
    bool m_open = false;
    bool m_comInited = false;
};

MjpegAviEncoder::MjpegAviEncoder()
    : m_impl(new Impl())
{
}

MjpegAviEncoder::~MjpegAviEncoder()
{
    delete m_impl;
}

bool MjpegAviEncoder::open(const EncoderOpenParams &params, std::string *error)
{
    return m_impl->open(params, error);
}

bool MjpegAviEncoder::writeFrame(const CaptureFrame &frame, std::string *error)
{
    return m_impl->writeFrame(frame, error);
}

bool MjpegAviEncoder::close(std::string *error)
{
    return m_impl->close(error);
}

} // namespace recorder
