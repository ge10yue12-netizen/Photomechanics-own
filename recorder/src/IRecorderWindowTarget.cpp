#include "../include/IRecorderWindowTarget.h"

namespace recorder
{

void IRecorderWindowTarget::notifyVisualChanged()
{
    if (hasVisualConsumers())
        refreshVisualCache(true);
}

void IRecorderWindowTarget::setVisualConsumerFlags(int flags)
{
    if (flags < 0)
        flags = 0;
    m_visualConsumerFlags = flags;
}

int IRecorderWindowTarget::visualConsumerFlags() const
{
    return m_visualConsumerFlags;
}

bool IRecorderWindowTarget::hasVisualConsumers() const
{
    return m_visualConsumerFlags != 0;
}

} // namespace recorder
