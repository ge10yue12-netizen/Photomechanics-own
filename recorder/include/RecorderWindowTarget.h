#pragma once

#include "IRecorderWindowTarget.h"
#include "RecorderTypes.h"

namespace recorder
{

void applyWindowTarget(RecorderConfig *config, IRecorderWindowTarget *target);

class FixedWindowTarget final : public IRecorderWindowTarget
{
public:
    explicit FixedWindowTarget(std::uintptr_t nativeHandle);

    std::uintptr_t windowHandle() const override;
    bool isAvailable() const override;

private:
    std::uintptr_t m_handle;
};

} // namespace recorder
