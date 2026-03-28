#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "model/PerformPageModel.h"

class NTPerformProcessor;

// One automatable parameter per performance page slot (30 total).
// Name and range are read live from the model so they update after a refresh.
class PerfPageParam : public juce::AudioProcessorParameter
{
public:
    PerfPageParam(NTPerformProcessor& proc, PerformPageModel& model, int index);

    // Called by the host to read the current value (normalised 0..1).
    float getValue() const override;

    // Called by the host during automation playback — sends to hardware.
    void setValue(float newValue) override;

    // Called from the message thread when a CC update changes this slot's value.
    // Notifies the DAW so automation lanes record the change.
    void notifyValueChanged();

    float         getDefaultValue()                      const override { return 0.0f; }
    juce::String  getName(int maxLen)                    const override;
    juce::String  getLabel()                             const override { return {}; }
    float         getValueForText(const juce::String& t) const override { return t.getFloatValue(); }
    bool          isAutomatable()                        const override { return true; }

private:
    NTPerformProcessor& proc_;
    PerformPageModel&   model_;
    int                 index_;
};
