#include "PerfPageParam.h"
#include "PluginProcessor.h"

PerfPageParam::PerfPageParam(NTPerformProcessor& proc, PerformPageModel& model, int index)
    : proc_(proc), model_(model), index_(index)
{
}

float PerfPageParam::getValue() const
{
    const float f = model_.getFillFraction(index_);
    return f >= 0.0f ? f : 0.0f;
}

void PerfPageParam::setValue(float newValue)
{
    const auto& item = model_.getItem(index_);
    if (!item.enabled) return;
    const int range = item.max - item.min;
    if (range <= 0) return;
    const int raw = item.min + juce::roundToInt(newValue * static_cast<float>(range));
    proc_.sendParamValue(item.slotIndex, item.parameterNumber, raw);
}

void PerfPageParam::notifyValueChanged()
{
    sendValueChangedMessageToListeners(getValue());
}

juce::String PerfPageParam::getName(int maxLen) const
{
    const auto& item = model_.getItem(index_);
    juce::String name;
    if (item.enabled)
    {
        name = item.upperLabel[0] ? juce::String(item.upperLabel)
                                  : juce::String(item.lowerLabel);
        if (name.isEmpty())
            name = "Slot " + juce::String(index_ + 1);
    }
    else
    {
        name = "Slot " + juce::String(index_ + 1);
    }
    return name.substring(0, maxLen);
}
