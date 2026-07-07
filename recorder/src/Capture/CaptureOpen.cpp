#include "CaptureOpen.h"

#include "DxgiScreenCapture.h"
#include "GdiScreenCapture.h"

namespace recorder
{
namespace capture
{

bool openScreenCapture(CaptureMode mode,
                       const Rect &region,
                       std::unique_ptr<IScreenCapture> *out,
                       std::string *error,
                       std::string *backendHint)
{
    if (!out)
        return false;

    out->reset();

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
                        std::unique_ptr<IScreenCapture> *out,
                        std::string *error)
{
    if (!out)
        return false;

    out->reset();
    std::unique_ptr<IScreenCapture> gdi(new GdiScreenCapture());
    if (gdi->open(mode, region, error))
    {
        *out = std::move(gdi);
        return true;
    }
    return false;
}

} // namespace capture
} // namespace recorder
