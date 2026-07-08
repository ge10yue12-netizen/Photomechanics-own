#include "../include/RecorderWindowTarget.h"

#include "Capture/CaptureCommon.h"

namespace recorder
{

void applyWindowTarget(RecorderConfig *config, IRecorderWindowTarget *target)
{
    if (!config)
        return;

    config->mode = CaptureMode::Window;
    config->windowTarget.provider = target;
    config->windowTarget.nativeHandle = target ? target->windowHandle() : 0;
}

FixedWindowTarget::FixedWindowTarget(std::uintptr_t nativeHandle)
    : m_handle(nativeHandle)
{
}

std::uintptr_t FixedWindowTarget::windowHandle() const
{
    return m_handle;
}

bool FixedWindowTarget::isAvailable() const
{
    return capture::isValidWindowHandle(m_handle) && capture::isWindowCapturable(m_handle);
}

} // namespace recorder
