#include "GdiWindowCapture.h"
#include "CaptureCommon.h"

#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace recorder
{
namespace
{

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

} // namespace

class GdiWindowCapture::Impl
{
public:
    void setTargetWindow(std::uintptr_t nativeHandle) { m_handle = nativeHandle; }

    bool open(CaptureMode mode, const Rect &region, std::string *error)
    {
        (void)region;
        close();
        if (mode != CaptureMode::Window)
        {
            if (error)
                *error = "GDI 窗口采集仅支持 Window 模式。";
            return false;
        }
        if (!capture::isValidWindowHandle(m_handle))
        {
            if (error)
                *error = "窗口句柄无效。";
            return false;
        }
        if (!capture::isWindowCapturable(m_handle))
        {
            if (error)
                *error = "目标窗口不可录制（已最小化或不可见）。";
            return false;
        }
        if (!capture::windowClientSize(m_handle, &m_width, &m_height, error))
            return false;

        m_open = true;
        return true;
    }

    bool grab(CaptureFrame *frame, std::string *error)
    {
        if (!m_open || !frame)
        {
            if (error)
                *error = "窗口采集未初始化。";
            return false;
        }
        if (!capture::isValidWindowHandle(m_handle) || !capture::isWindowCapturable(m_handle))
        {
            if (error)
                *error = "目标窗口不可录制。";
            return false;
        }

        int width = 0;
        int height = 0;
        if (!capture::windowClientSize(m_handle, &width, &height, error))
            return false;

        m_width = width;
        m_height = height;

        HWND hwnd = capture::hwndFromHandle(m_handle);
        HDC windowDc = GetDC(hwnd);
        if (!windowDc)
        {
            if (error)
                *error = "获取窗口设备上下文失败。";
            return false;
        }

        HDC memDc = CreateCompatibleDC(windowDc);
        if (!memDc)
        {
            ReleaseDC(hwnd, windowDc);
            if (error)
                *error = "创建兼容设备上下文失败。";
            return false;
        }

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void *dibBits = nullptr;
        HBITMAP bitmap = CreateDIBSection(memDc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
        if (!bitmap || !dibBits)
        {
            DeleteDC(memDc);
            ReleaseDC(hwnd, windowDc);
            if (error)
                *error = "创建 DIB 位图失败。";
            return false;
        }

        HGDIOBJ oldObj = SelectObject(memDc, bitmap);
        BOOL captured = PrintWindow(hwnd, memDc, PW_RENDERFULLCONTENT);
        if (!captured)
            captured = PrintWindow(hwnd, memDc, 0);
        if (!captured)
            captured = BitBlt(memDc, 0, 0, width, height, windowDc, 0, 0, SRCCOPY);

        const int stride = width * 4;
        frame->width = width;
        frame->height = height;
        frame->stride = stride;
        frame->hasNewPixels = true;
        const size_t byteCount = static_cast<size_t>(stride) * static_cast<size_t>(height);
        frame->bgra.resize(byteCount);
        if (captured)
            std::memcpy(frame->bgra.data(), dibBits, byteCount);

        SelectObject(memDc, oldObj);
        DeleteObject(bitmap);
        DeleteDC(memDc);
        ReleaseDC(hwnd, windowDc);

        if (!captured || frame->bgra.empty())
        {
            if (error)
                *error = "窗口抓取失败。";
            return false;
        }
        return true;
    }

    void close() { m_open = false; }

    int captureWidth() const { return m_width; }
    int captureHeight() const { return m_height; }

private:
    std::uintptr_t m_handle = 0;
    int m_width = 0;
    int m_height = 0;
    bool m_open = false;
};

GdiWindowCapture::GdiWindowCapture()
    : m_impl(new Impl())
{
}

GdiWindowCapture::~GdiWindowCapture()
{
    delete m_impl;
}

void GdiWindowCapture::setTargetWindow(std::uintptr_t nativeHandle)
{
    m_impl->setTargetWindow(nativeHandle);
}

bool GdiWindowCapture::open(CaptureMode mode, const Rect &region, std::string *error)
{
    return m_impl->open(mode, region, error);
}

bool GdiWindowCapture::grab(CaptureFrame *frame, std::string *error)
{
    return m_impl->grab(frame, error);
}

void GdiWindowCapture::close()
{
    m_impl->close();
}

int GdiWindowCapture::captureWidth() const
{
    return m_impl->captureWidth();
}

int GdiWindowCapture::captureHeight() const
{
    return m_impl->captureHeight();
}

} // namespace recorder
