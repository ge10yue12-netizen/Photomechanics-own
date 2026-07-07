#include "GdiScreenCapture.h"
#include "CaptureCommon.h"

#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace recorder
{

class GdiScreenCapture::Impl
{
public:
    bool open(CaptureMode mode, const Rect &region, std::string *error)
    {
        close();
        if (mode == CaptureMode::FullScreen)
            m_region = capture::primaryScreenRect();
        else
            m_region = region;

        if (!capture::validateRegion(m_region, error))
            return false;

        m_open = true;
        return true;
    }

    bool grab(CaptureFrame *frame, std::string *error)
    {
        if (!m_open || !frame)
        {
            if (error)
                *error = "屏幕采集未初始化。";
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
        bmi.bmiHeader.biWidth = m_region.width;
        bmi.bmiHeader.biHeight = -m_region.height;
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
                *error = "创建 DIB 位图失败。";
            return false;
        }

        HGDIOBJ oldObj = SelectObject(memDc, bitmap);
        const BOOL bltOk = BitBlt(memDc,
                                  0,
                                  0,
                                  m_region.width,
                                  m_region.height,
                                  screenDc,
                                  m_region.x,
                                  m_region.y,
                                  SRCCOPY | CAPTUREBLT);

        const int stride = m_region.width * 4;
        frame->width = m_region.width;
        frame->height = m_region.height;
        frame->stride = stride;
        const size_t byteCount =
            static_cast<size_t>(stride) * static_cast<size_t>(m_region.height);
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
                *error = "屏幕抓取失败。";
            return false;
        }
        return true;
    }

    void close()
    {
        m_open = false;
    }

    int captureWidth() const { return m_region.width; }
    int captureHeight() const { return m_region.height; }

private:
    Rect m_region;
    bool m_open = false;
};

GdiScreenCapture::GdiScreenCapture()
    : m_impl(new Impl())
{
}

GdiScreenCapture::~GdiScreenCapture()
{
    delete m_impl;
}

bool GdiScreenCapture::open(CaptureMode mode, const Rect &region, std::string *error)
{
    return m_impl->open(mode, region, error);
}

bool GdiScreenCapture::grab(CaptureFrame *frame, std::string *error)
{
    return m_impl->grab(frame, error);
}

void GdiScreenCapture::close()
{
    m_impl->close();
}

int GdiScreenCapture::captureWidth() const
{
    return m_impl->captureWidth();
}

int GdiScreenCapture::captureHeight() const
{
    return m_impl->captureHeight();
}

} // namespace recorder
