#include "CaptureOpen.h"
#include "CaptureCommon.h"
#include "DxgiScreenCapture.h"
#include "GdiScreenCapture.h"
#include "GdiWindowCapture.h"
#include "QtWidgetVisualCapture.h"
#include "../../include/IRecorderWindowTarget.h"
#include "../../include/VisualFrameCache.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
namespace recorder
{
namespace capture
{
namespace
{
constexpr int kPreviewMaxLongEdge = 640;
void previewOutputSize(int srcW, int srcH, int *outW, int *outH)
{
    if (!outW || !outH || srcW <= 0 || srcH <= 0)
        return;
    const int longEdge = std::max(srcW, srcH);
    if (longEdge <= kPreviewMaxLongEdge)
    {
        *outW = srcW;
        *outH = srcH;
        return;
    }
    const double scale = static_cast<double>(kPreviewMaxLongEdge) / static_cast<double>(longEdge);
    *outW = std::max(1, static_cast<int>(srcW * scale + 0.5));
    *outH = std::max(1, static_cast<int>(srcH * scale + 0.5));
}
// 预览专用 GDI：StretchBlt 一次缩到长边 ≤640，避免全屏 BitBlt + Qt 二次缩放。
class GdiPreviewCapture : public IScreenCapture
{
public:
    bool open(CaptureMode mode, const Rect &region, std::string *error) override
    {
        close();
        if (mode == CaptureMode::FullScreen)
            m_srcRegion = capture::primaryScreenRect();
        else
            m_srcRegion = region;
        if (!capture::validateRegion(m_srcRegion, error))
            return false;
        previewOutputSize(m_srcRegion.width, m_srcRegion.height, &m_outW, &m_outH);
        m_open = true;
        return true;
    }
    bool grab(CaptureFrame *frame, std::string *error) override
    {
        if (!m_open || !frame)
        {
            if (error)
                *error = "预览采集未初始化。";
            return false;
        }
        HDC screenDc = GetDC(nullptr);
        if (!screenDc)
        {
            if (error)
                *error = "获取屏幕设备上下文失败。";
            return false;
        }
        HDC memDc = CreateCompatibleDC(screenDc);
        if (!memDc)
        {
            ReleaseDC(nullptr, screenDc);
            if (error)
                *error = "创建兼容设备上下文失败。";
            return false;
        }
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = m_outW;
        bmi.bmiHeader.biHeight = -m_outH;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void *dibBits = nullptr;
        HBITMAP bitmap = CreateDIBSection(memDc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
        if (!bitmap || !dibBits)
        {
            DeleteDC(memDc);
            ReleaseDC(nullptr, screenDc);
            if (error)
                *error = "创建预览 DIB 失败。";
            return false;
        }
        HGDIOBJ oldObj = SelectObject(memDc, bitmap);
        const BOOL bltOk = StretchBlt(memDc,
                                        0,
                                        0,
                                        m_outW,
                                        m_outH,
                                        screenDc,
                                        m_srcRegion.x,
                                        m_srcRegion.y,
                                        m_srcRegion.width,
                                        m_srcRegion.height,
                                        SRCCOPY | CAPTUREBLT);
        const int stride = m_outW * 4;
        frame->width = m_outW;
        frame->height = m_outH;
        frame->stride = stride;
        frame->hasNewPixels = true;
        const size_t byteCount =
            static_cast<size_t>(stride) * static_cast<size_t>(m_outH);
        if (frame->bgra.size() != byteCount)
            frame->bgra.resize(byteCount);
        if (bltOk)
            std::memcpy(frame->bgra.data(), dibBits, byteCount);
        SelectObject(memDc, oldObj);
        DeleteObject(bitmap);
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        if (!bltOk || frame->bgra.empty())
        {
            if (error)
                *error = "预览抓取失败。";
            return false;
        }
        return true;
    }
    void close() override
    {
        m_open = false;
    }
    int captureWidth() const override { return m_outW; }
    int captureHeight() const override { return m_outH; }
private:
    Rect m_srcRegion;
    int m_outW = 0;
    int m_outH = 0;
    bool m_open = false;
};

class GdiWindowPreviewCapture : public IScreenCapture
{
public:
    void setTargetWindow(std::uintptr_t nativeHandle) { m_handle = nativeHandle; }

    bool open(CaptureMode mode, const Rect &region, std::string *error) override
    {
        (void)region;
        close();
        if (mode != CaptureMode::Window)
        {
            if (error)
                *error = "窗口预览仅支持 Window 模式。";
            return false;
        }
        if (!capture::isValidWindowHandle(m_handle))
        {
            if (error)
                *error = "窗口句柄无效。";
            return false;
        }
        if (!capture::windowClientSize(m_handle, &m_srcW, &m_srcH, error))
            return false;

        previewOutputSize(m_srcW, m_srcH, &m_outW, &m_outH);
        m_open = true;
        return true;
    }

    bool grab(CaptureFrame *frame, std::string *error) override
    {
        if (!m_open || !frame)
        {
            if (error)
                *error = "窗口预览未初始化。";
            return false;
        }

        GdiWindowCapture fullCapture;
        fullCapture.setTargetWindow(m_handle);
        if (!fullCapture.open(CaptureMode::Window, {}, error))
            return false;

        CaptureFrame src;
        if (!fullCapture.grab(&src, error))
            return false;

        if (m_outW == src.width && m_outH == src.height)
        {
            *frame = std::move(src);
            return true;
        }

        HDC screenDc = GetDC(nullptr);
        if (!screenDc)
        {
            if (error)
                *error = "获取屏幕设备上下文失败。";
            return false;
        }

        HDC memDc = CreateCompatibleDC(screenDc);
        if (!memDc)
        {
            ReleaseDC(nullptr, screenDc);
            if (error)
                *error = "创建兼容设备上下文失败。";
            return false;
        }

        BITMAPINFO srcBmi = {};
        srcBmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        srcBmi.bmiHeader.biWidth = src.width;
        srcBmi.bmiHeader.biHeight = -src.height;
        srcBmi.bmiHeader.biPlanes = 1;
        srcBmi.bmiHeader.biBitCount = 32;
        srcBmi.bmiHeader.biCompression = BI_RGB;

        void *srcBits = nullptr;
        HBITMAP srcBmp = CreateDIBSection(memDc, &srcBmi, DIB_RGB_COLORS, &srcBits, nullptr, 0);
        if (!srcBmp || !srcBits)
        {
            DeleteDC(memDc);
            ReleaseDC(nullptr, screenDc);
            if (error)
                *error = "创建预览源位图失败。";
            return false;
        }
        std::memcpy(srcBits, src.bgra.data(), src.bgra.size());

        BITMAPINFO dstBmi = srcBmi;
        dstBmi.bmiHeader.biWidth = m_outW;
        dstBmi.bmiHeader.biHeight = -m_outH;

        void *dstBits = nullptr;
        HBITMAP dstBmp = CreateDIBSection(memDc, &dstBmi, DIB_RGB_COLORS, &dstBits, nullptr, 0);
        if (!dstBmp || !dstBits)
        {
            DeleteObject(srcBmp);
            DeleteDC(memDc);
            ReleaseDC(nullptr, screenDc);
            if (error)
                *error = "创建预览目标位图失败。";
            return false;
        }

        HGDIOBJ oldSrc = SelectObject(memDc, srcBmp);
        HDC dstDc = CreateCompatibleDC(screenDc);
        HGDIOBJ oldDst = SelectObject(dstDc, dstBmp);
        const BOOL ok = StretchBlt(dstDc,
                                   0,
                                   0,
                                   m_outW,
                                   m_outH,
                                   memDc,
                                   0,
                                   0,
                                   src.width,
                                   src.height,
                                   SRCCOPY);

        const int stride = m_outW * 4;
        frame->width = m_outW;
        frame->height = m_outH;
        frame->stride = stride;
        frame->hasNewPixels = true;
        const size_t byteCount = static_cast<size_t>(stride) * static_cast<size_t>(m_outH);
        frame->bgra.resize(byteCount);
        if (ok)
            std::memcpy(frame->bgra.data(), dstBits, byteCount);

        SelectObject(memDc, oldSrc);
        SelectObject(dstDc, oldDst);
        DeleteObject(dstBmp);
        DeleteObject(srcBmp);
        DeleteDC(dstDc);
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);

        if (!ok || frame->bgra.empty())
        {
            if (error)
                *error = "窗口预览缩放失败。";
            return false;
        }
        return true;
    }

    void close() override { m_open = false; }

    int captureWidth() const override { return m_outW; }
    int captureHeight() const override { return m_outH; }

private:
    std::uintptr_t m_handle = 0;
    int m_srcW = 0;
    int m_srcH = 0;
    int m_outW = 0;
    int m_outH = 0;
    bool m_open = false;
};

} // namespace

bool copyVisualCacheToPreviewImage(const VisualFrameCache *cache,
                                   int maxLongEdge,
                                   std::vector<std::uint8_t> *bgraOut,
                                   int *widthOut,
                                   int *heightOut,
                                   int *strideOut)
{
    if (!cache || !bgraOut || !widthOut || !heightOut || !strideOut || maxLongEdge < 1)
        return false;

    CaptureFrame src;
    if (!cache->copyLatest(&src))
        return false;

    int outW = src.width;
    int outH = src.height;
    const int longEdge = std::max(src.width, src.height);
    if (longEdge > maxLongEdge)
    {
        const double scale = static_cast<double>(maxLongEdge) / static_cast<double>(longEdge);
        outW = std::max(1, static_cast<int>(src.width * scale + 0.5));
        outH = std::max(1, static_cast<int>(src.height * scale + 0.5));
    }
    if (outW == src.width && outH == src.height)
    {
        *bgraOut = src.bgra;
        *widthOut = src.width;
        *heightOut = src.height;
        *strideOut = src.stride;
        return true;
    }

    HDC screenDc = GetDC(nullptr);
    if (!screenDc)
        return false;

    HDC memDc = CreateCompatibleDC(screenDc);
    if (!memDc)
    {
        ReleaseDC(nullptr, screenDc);
        return false;
    }

    BITMAPINFO srcBmi = {};
    srcBmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    srcBmi.bmiHeader.biWidth = src.width;
    srcBmi.bmiHeader.biHeight = -src.height;
    srcBmi.bmiHeader.biPlanes = 1;
    srcBmi.bmiHeader.biBitCount = 32;
    srcBmi.bmiHeader.biCompression = BI_RGB;

    void *srcBits = nullptr;
    HBITMAP srcBmp = CreateDIBSection(memDc, &srcBmi, DIB_RGB_COLORS, &srcBits, nullptr, 0);
    if (!srcBmp || !srcBits)
    {
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        return false;
    }
    std::memcpy(srcBits, src.bgra.data(), src.bgra.size());

    BITMAPINFO dstBmi = srcBmi;
    dstBmi.bmiHeader.biWidth = outW;
    dstBmi.bmiHeader.biHeight = -outH;

    void *dstBits = nullptr;
    HBITMAP dstBmp = CreateDIBSection(memDc, &dstBmi, DIB_RGB_COLORS, &dstBits, nullptr, 0);
    if (!dstBmp || !dstBits)
    {
        DeleteObject(srcBmp);
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        return false;
    }

    HGDIOBJ oldSrc = SelectObject(memDc, srcBmp);
    HDC dstDc = CreateCompatibleDC(screenDc);
    HGDIOBJ oldDst = SelectObject(dstDc, dstBmp);
    const BOOL ok = StretchBlt(dstDc,
                               0,
                               0,
                               outW,
                               outH,
                               memDc,
                               0,
                               0,
                               src.width,
                               src.height,
                               SRCCOPY);

    const int stride = outW * 4;
    bgraOut->resize(static_cast<size_t>(stride) * static_cast<size_t>(outH));
    if (ok)
        std::memcpy(bgraOut->data(), dstBits, bgraOut->size());

    SelectObject(memDc, oldSrc);
    SelectObject(dstDc, oldDst);
    DeleteObject(dstBmp);
    DeleteObject(srcBmp);
    DeleteDC(dstDc);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);

    if (!ok || bgraOut->empty())
        return false;

    *widthOut = outW;
    *heightOut = outH;
    *strideOut = stride;
    return true;
}

bool openScreenCapture(CaptureMode mode,
                       const Rect &region,
                       const WindowCaptureTarget &windowTarget,
                       std::unique_ptr<IScreenCapture> *out,
                       std::string *error,
                       std::string *backendHint)
{
    if (!out)
        return false;
    out->reset();

    if (mode == CaptureMode::Window)
    {
        IRecorderWindowTarget *provider = windowTarget.provider;
        VisualFrameCache *cache = provider ? provider->visualCache() : nullptr;
        if (cache)
        {
            if (!cache->hasFrame() && provider)
                provider->refreshVisualCache(true);
            if (cache->hasFrame())
            {
                std::unique_ptr<QtWidgetVisualCapture> visualCapture(new QtWidgetVisualCapture(cache));
                if (visualCapture->open(mode, region, error))
                {
                    *out = std::move(visualCapture);
                    if (backendHint)
                        *backendHint = "Qt-VisualCache";
                    return true;
                }
                return false;
            }
        }

        std::unique_ptr<GdiWindowCapture> windowCapture(new GdiWindowCapture());
        windowCapture->setTargetWindow(windowTarget.nativeHandle);
        if (windowCapture->open(mode, region, error))
        {
            *out = std::move(windowCapture);
            if (backendHint)
                *backendHint = "GDI-Window";
            return true;
        }
        return false;
    }

    std::string dxgiErr;
    std::unique_ptr<IScreenCapture> dxgi(new DxgiScreenCapture());
    if (dxgi->open(mode, region, &dxgiErr))
    {
        *out = std::move(dxgi);
        if (backendHint)
            *backendHint = "DXGI";
        return true;
    }
    std::unique_ptr<IScreenCapture> gdi(new GdiScreenCapture());
    if (gdi->open(mode, region, error))
    {
        *out = std::move(gdi);
        if (backendHint)
            *backendHint = "GDI";
        return true;
    }
    if (error && error->empty() && !dxgiErr.empty())
        *error = dxgiErr;
    return false;
}
bool openPreviewCapture(CaptureMode mode,
                        const Rect &region,
                        const WindowCaptureTarget &windowTarget,
                        std::unique_ptr<IScreenCapture> *out,
                        std::string *error)
{
    if (!out)
        return false;
    out->reset();

    if (mode == CaptureMode::Window)
    {
        IRecorderWindowTarget *provider = windowTarget.provider;
        VisualFrameCache *cache = provider ? provider->visualCache() : nullptr;
        if (cache)
        {
            if (!cache->hasFrame() && provider)
                provider->refreshVisualCache(true);

            class VisualCachePreviewCapture : public IScreenCapture
            {
            public:
                explicit VisualCachePreviewCapture(VisualFrameCache *cacheIn)
                    : m_cache(cacheIn)
                {
                }

                bool open(CaptureMode m, const Rect &r, std::string *err) override
                {
                    (void)r;
                    close();
                    if (m != CaptureMode::Window)
                    {
                        if (err)
                            *err = "视觉缓存预览仅支持 Window 模式。";
                        return false;
                    }
                    if (!m_cache || !m_cache->hasFrame())
                    {
                        if (err)
                            *err = "视觉缓存尚无有效帧。";
                        return false;
                    }
                    m_open = true;
                    return true;
                }

                bool grab(CaptureFrame *frame, std::string *err) override
                {
                    if (!m_open || !frame || !m_cache)
                    {
                        if (err)
                            *err = "视觉缓存预览未初始化。";
                        return false;
                    }
                    std::vector<std::uint8_t> bgra;
                    int w = 0;
                    int h = 0;
                    int stride = 0;
                    if (!copyVisualCacheToPreviewImage(m_cache, kPreviewMaxLongEdge, &bgra, &w, &h, &stride))
                    {
                        if (err)
                            *err = "读取视觉缓存预览失败。";
                        return false;
                    }
                    frame->width = w;
                    frame->height = h;
                    frame->stride = stride;
                    frame->bgra = std::move(bgra);
                    frame->hasNewPixels = true;
                    m_outW = w;
                    m_outH = h;
                    return true;
                }

                void close() override { m_open = false; }
                int captureWidth() const override { return m_outW; }
                int captureHeight() const override { return m_outH; }

            private:
                VisualFrameCache *m_cache = nullptr;
                int m_outW = 0;
                int m_outH = 0;
                bool m_open = false;
            };

            if (cache->hasFrame())
            {
                std::unique_ptr<IScreenCapture> preview(new VisualCachePreviewCapture(cache));
                if (preview->open(mode, region, error))
                {
                    *out = std::move(preview);
                    return true;
                }
                return false;
            }
        }

        std::unique_ptr<GdiWindowPreviewCapture> preview(new GdiWindowPreviewCapture());
        preview->setTargetWindow(windowTarget.nativeHandle);
        if (preview->open(mode, region, error))
        {
            *out = std::move(preview);
            return true;
        }
        return false;
    }

    std::unique_ptr<IScreenCapture> preview(new GdiPreviewCapture());
    if (preview->open(mode, region, error))
    {
        *out = std::move(preview);
        return true;
    }
    return false;
}
} // namespace capture
} // namespace recorder
