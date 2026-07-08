#include "DxgiScreenCapture.h"
#include "CaptureCommon.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace recorder
{
namespace
{

struct OutputCandidate
{
    IDXGIOutput1 *output1 = nullptr;
    IDXGIAdapter *adapter = nullptr;
    Rect desktop;
    int overlapArea = 0;
};

int rectOverlapArea(const Rect &a, const Rect &b)
{
    const int left = std::max(a.x, b.x);
    const int top = std::max(a.y, b.y);
    const int right = std::min(a.x + a.width, b.x + b.width);
    const int bottom = std::min(a.y + a.height, b.y + b.height);
    if (right <= left || bottom <= top)
        return 0;
    return (right - left) * (bottom - top);
}

Rect rectFromDxgi(const DXGI_OUTPUT_DESC &desc)
{
    Rect r;
    r.x = desc.DesktopCoordinates.left;
    r.y = desc.DesktopCoordinates.top;
    r.width = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
    r.height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;
    return r;
}

void releaseOutputCandidate(OutputCandidate &c)
{
    if (c.output1)
    {
        c.output1->Release();
        c.output1 = nullptr;
    }
    if (c.adapter)
    {
        c.adapter->Release();
        c.adapter = nullptr;
    }
}

bool findPrimaryOutput(OutputCandidate *best, std::string *error)
{
    if (!best)
        return false;

    IDXGIFactory1 *factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                  reinterpret_cast<void **>(&factory))) ||
        !factory)
    {
        if (error)
            *error = "创建 DXGI 工厂失败。";
        return false;
    }

    IDXGIAdapter1 *adapter = nullptr;
    if (factory->EnumAdapters1(0, &adapter) != S_OK || !adapter)
    {
        factory->Release();
        if (error)
            *error = "未找到 DXGI 适配器。";
        return false;
    }

    for (UINT oi = 0;; ++oi)
    {
        IDXGIOutput *output = nullptr;
        if (adapter->EnumOutputs(oi, &output) == DXGI_ERROR_NOT_FOUND)
            break;
        if (!output)
            continue;

        DXGI_OUTPUT_DESC desc = {};
        output->GetDesc(&desc);
        if (desc.AttachedToDesktop == FALSE)
        {
            output->Release();
            continue;
        }

        IDXGIOutput1 *output1 = nullptr;
        if (FAILED(output->QueryInterface(__uuidof(IDXGIOutput1),
                                           reinterpret_cast<void **>(&output1))) ||
            !output1)
        {
            output->Release();
            continue;
        }

        best->output1 = output1;
        best->adapter = adapter;
        adapter->AddRef();
        best->desktop = rectFromDxgi(desc);
        best->overlapArea = best->desktop.width * best->desktop.height;
        output->Release();
        factory->Release();
        return true;
    }

    adapter->Release();
    factory->Release();
    if (error)
        *error = "未找到可用显示器输出。";
    return false;
}

bool findBestOutput(const Rect &target, OutputCandidate *best, std::string *error)
{
    if (!best)
        return false;

    IDXGIFactory1 *factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                  reinterpret_cast<void **>(&factory))) ||
        !factory)
    {
        if (error)
            *error = "创建 DXGI 工厂失败。";
        return false;
    }

    best->overlapArea = -1;
    for (UINT ai = 0;; ++ai)
    {
        IDXGIAdapter1 *adapter = nullptr;
        if (factory->EnumAdapters1(ai, &adapter) == DXGI_ERROR_NOT_FOUND)
            break;
        if (!adapter)
            continue;

        for (UINT oi = 0;; ++oi)
        {
            IDXGIOutput *output = nullptr;
            if (adapter->EnumOutputs(oi, &output) == DXGI_ERROR_NOT_FOUND)
                break;
            if (!output)
                continue;

            DXGI_OUTPUT_DESC desc = {};
            output->GetDesc(&desc);
            if (desc.AttachedToDesktop == FALSE)
            {
                output->Release();
                continue;
            }

            IDXGIOutput1 *output1 = nullptr;
            if (FAILED(output->QueryInterface(__uuidof(IDXGIOutput1),
                                               reinterpret_cast<void **>(&output1))) ||
                !output1)
            {
                output->Release();
                continue;
            }

            const Rect desktop = rectFromDxgi(desc);
            const int overlap = rectOverlapArea(target, desktop);
            if (overlap > best->overlapArea)
            {
                releaseOutputCandidate(*best);
                best->output1 = output1;
                best->adapter = adapter;
                adapter->AddRef();
                best->desktop = desktop;
                best->overlapArea = overlap;
            }
            else
            {
                output1->Release();
            }
            output->Release();
        }
        adapter->Release();
    }
    factory->Release();

    if (!best->output1 || best->overlapArea <= 0)
    {
        if (error)
            *error = "未找到与录制区域匹配的显示器输出。";
        return false;
    }
    return true;
}

void copyRowBgra(const std::uint8_t *src,
                 int srcStride,
                 std::uint8_t *dst,
                 int dstStride,
                 int width,
                 int height)
{
    const int copyBytes = width * 4;
    if (srcStride == dstStride)
    {
        std::memcpy(dst, src, static_cast<size_t>(srcStride) * static_cast<size_t>(height));
        return;
    }
    for (int y = 0; y < height; ++y)
    {
        std::memcpy(dst + static_cast<size_t>(y) * static_cast<size_t>(dstStride),
                    src + static_cast<size_t>(y) * static_cast<size_t>(srcStride),
                    static_cast<size_t>(copyBytes));
    }
}

} // namespace

class DxgiScreenCapture::Impl
{
public:
    ~Impl() { close(); }

    bool open(CaptureMode mode, const Rect &region, std::string *error)
    {
        close();

        OutputCandidate picked;
        if (mode == CaptureMode::FullScreen)
        {
            if (!findPrimaryOutput(&picked, error))
                return false;
        }
        else
        {
            if (!capture::validateRegion(region, error))
                return false;
            if (!findBestOutput(region, &picked, error))
                return false;
        }

        const Rect target = (mode == CaptureMode::FullScreen) ? picked.desktop : region;
        m_region = target;
        m_outputDesktop = picked.desktop;

        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
        const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        if (FAILED(D3D11CreateDevice(picked.adapter,
                                     D3D_DRIVER_TYPE_UNKNOWN,
                                     nullptr,
                                     flags,
                                     levels,
                                     2,
                                     D3D11_SDK_VERSION,
                                     &m_device,
                                     &featureLevel,
                                     &m_context)) ||
            !m_device || !m_context)
        {
            releaseOutputCandidate(picked);
            if (error)
                *error = "创建 D3D11 设备失败。";
            return false;
        }

        if (FAILED(picked.output1->DuplicateOutput(m_device, &m_duplication)) || !m_duplication)
        {
            releaseOutputCandidate(picked);
            if (error)
                *error = "DXGI 桌面复制初始化失败（可能已有其他程序占用）。";
            return false;
        }

        picked.output1->Release();
        picked.adapter->Release();

        m_cropX = m_region.x - m_outputDesktop.x;
        m_cropY = m_region.y - m_outputDesktop.y;
        m_open = true;
        m_hasLastFrame = false;
        warmUpInitialFrame();
        return true;
    }

    // 打开后预抓一帧；静止桌面首次 AcquireNextFrame 常超时，避免预览/录制首帧失败。
    void warmUpInitialFrame()
    {
        for (int i = 0; i < 5 && !m_hasLastFrame; ++i)
        {
            CaptureFrame tmp;
            std::string err;
            if (grabFrameOnce(&tmp, &err, 100))
                return;
            Sleep(20);
        }
    }

    bool grab(CaptureFrame *frame, std::string *error)
    {
        return grabFrameOnce(frame, error, 16);
    }

    void close()
    {
        if (m_staging)
        {
            m_staging->Release();
            m_staging = nullptr;
        }
        if (m_duplication)
        {
            m_duplication->Release();
            m_duplication = nullptr;
        }
        if (m_context)
        {
            m_context->Release();
            m_context = nullptr;
        }
        if (m_device)
        {
            m_device->Release();
            m_device = nullptr;
        }
        m_open = false;
        m_hasLastFrame = false;
    }

    int captureWidth() const { return m_region.width; }
    int captureHeight() const { return m_region.height; }

private:
    bool grabFrameOnce(CaptureFrame *frame, std::string *error, UINT timeoutMs)
    {
        if (!m_open || !frame || !m_duplication)
        {
            if (error)
                *error = "DXGI 采集未初始化。";
            return false;
        }

        DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
        IDXGIResource *resource = nullptr;
        const HRESULT hr = m_duplication->AcquireNextFrame(timeoutMs, &frameInfo, &resource);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        {
            if (m_hasLastFrame)
            {
                frame->width = m_region.width;
                frame->height = m_region.height;
                frame->stride = m_region.width * 4;
                frame->hasNewPixels = false;
                return true;
            }
            if (error)
                *error = "等待桌面帧超时。";
            return false;
        }
        if (FAILED(hr))
        {
            if (error)
                *error = "AcquireNextFrame 失败。";
            return false;
        }

        ID3D11Texture2D *desktopTex = nullptr;
        if (FAILED(resource->QueryInterface(__uuidof(ID3D11Texture2D),
                                             reinterpret_cast<void **>(&desktopTex))) ||
            !desktopTex)
        {
            resource->Release();
            m_duplication->ReleaseFrame();
            if (error)
                *error = "获取桌面纹理失败。";
            return false;
        }

        ensureStagingTexture();
        if (!m_staging)
        {
            desktopTex->Release();
            resource->Release();
            m_duplication->ReleaseFrame();
            if (error)
                *error = "创建 staging 纹理失败。";
            return false;
        }

        D3D11_BOX box = {};
        box.left = static_cast<UINT>(m_cropX);
        box.top = static_cast<UINT>(m_cropY);
        box.right = static_cast<UINT>(m_cropX + m_region.width);
        box.bottom = static_cast<UINT>(m_cropY + m_region.height);
        box.front = 0;
        box.back = 1;
        m_context->CopySubresourceRegion(m_staging, 0, 0, 0, 0, desktopTex, 0, &box);

        desktopTex->Release();
        resource->Release();
        m_duplication->ReleaseFrame();

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(m_context->Map(m_staging, 0, D3D11_MAP_READ, 0, &mapped)))
        {
            if (error)
                *error = "映射 staging 纹理失败。";
            return false;
        }

        const int stride = m_region.width * 4;
        frame->width = m_region.width;
        frame->height = m_region.height;
        frame->stride = stride;
        frame->hasNewPixels = true;
        const size_t byteCount =
            static_cast<size_t>(stride) * static_cast<size_t>(m_region.height);
        if (frame->bgra.size() != byteCount)
            frame->bgra.resize(byteCount);
        copyRowBgra(static_cast<const std::uint8_t *>(mapped.pData),
                    static_cast<int>(mapped.RowPitch),
                    frame->bgra.data(),
                    stride,
                    m_region.width,
                    m_region.height);
        m_context->Unmap(m_staging, 0);

        m_hasLastFrame = true;
        return true;
    }

    void ensureStagingTexture()
    {
        if (m_staging)
            return;

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = static_cast<UINT>(m_region.width);
        desc.Height = static_cast<UINT>(m_region.height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        m_device->CreateTexture2D(&desc, nullptr, &m_staging);
    }

    ID3D11Device *m_device = nullptr;
    ID3D11DeviceContext *m_context = nullptr;
    IDXGIOutputDuplication *m_duplication = nullptr;
    ID3D11Texture2D *m_staging = nullptr;
    Rect m_region;
    Rect m_outputDesktop;
    int m_cropX = 0;
    int m_cropY = 0;
    bool m_hasLastFrame = false;
    bool m_open = false;
};

DxgiScreenCapture::DxgiScreenCapture()
    : m_impl(new Impl())
{
}

DxgiScreenCapture::~DxgiScreenCapture()
{
    delete m_impl;
}

bool DxgiScreenCapture::open(CaptureMode mode, const Rect &region, std::string *error)
{
    return m_impl->open(mode, region, error);
}

bool DxgiScreenCapture::grab(CaptureFrame *frame, std::string *error)
{
    return m_impl->grab(frame, error);
}

void DxgiScreenCapture::close()
{
    m_impl->close();
}

int DxgiScreenCapture::captureWidth() const
{
    return m_impl->captureWidth();
}

int DxgiScreenCapture::captureHeight() const
{
    return m_impl->captureHeight();
}

} // namespace recorder
