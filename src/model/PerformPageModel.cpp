#include "PerformPageModel.h"
#include <algorithm>

void PerformPageModel::setItem(const PerformancePageItem& item)
{
    const int idx = item.itemIndex;
    if (idx < 0 || idx >= kTotalItems)
        return;

    {
        juce::ScopedLock sl(lock_);
        items_[idx]    = item;
        received_[idx] = true;
    }

    if (onChange)
        onChange();
}

void PerformPageModel::setParamValue(int slot, int param, int value)
{
    {
        juce::ScopedLock sl(lock_);
        paramValues_[makeKey(slot, param)] = value;
    }

    if (onChange)
        onChange();
}

int PerformPageModel::getParamValue(int slot, int param) const
{
    juce::ScopedLock sl(lock_);
    auto it = paramValues_.find(makeKey(slot, param));
    return (it != paramValues_.end()) ? it->second : 0;
}

const PerformancePageItem& PerformPageModel::getItem(int index) const
{
    juce::ScopedLock sl(lock_);
    static const PerformancePageItem kEmpty{};
    if (index < 0 || index >= kTotalItems)
        return kEmpty;
    return items_[index];
}

float PerformPageModel::getFillFraction(int index) const
{
    juce::ScopedLock sl(lock_);

    if (index < 0 || index >= kTotalItems)
        return -1.0f;
    if (!received_[index])
        return -1.0f;

    const auto& item = items_[index];
    if (!item.enabled)
        return -1.0f;
    if (item.max <= item.min)
        return -1.0f;

    auto it = paramValues_.find(makeKey(item.slotIndex, item.parameterNumber));
    if (it == paramValues_.end())
        return -1.0f;

    const float fraction = static_cast<float>(it->second - item.min)
                         / static_cast<float>(item.max - item.min);
    return std::clamp(fraction, 0.0f, 1.0f);
}

int PerformPageModel::loadedCount() const
{
    juce::ScopedLock sl(lock_);
    int count = 0;
    for (const bool r : received_)
        if (r) ++count;
    return count;
}
