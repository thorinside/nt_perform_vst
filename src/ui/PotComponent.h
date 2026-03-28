#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// Displays a single performance page "knob slot" — matching the
// _KnobSlot widget in nt_helper's hardware_preview_widget.dart.
//
// Shows a gradient fill bar (left-to-right) with upper/lower labels.
// Horizontal drag adjusts the value.
class PotComponent : public juce::Component
{
public:
    struct Data
    {
        bool   enabled      = false;
        bool   isEmpty      = true;
        float  fillFraction = 0.0f; // 0.0 – 1.0
        juce::String upperLabel;
        juce::String lowerLabel;
        juce::String posLabel; // "Pot L", "Pot C", "Pot R"
        int    slotIndex    = 0;
        int    paramNumber  = 0;
        int    rangeMin     = 0;
        int    rangeMax     = 0;
    };

    PotComponent();

    void setData(const Data& d);
    const Data& getData() const { return data_; }

    // Called when the user drags to a new value.
    std::function<void(int slot, int param, int value, bool isDragging)> onValueChange;
    std::function<void()> onDragStart; // called on mouseDown (for gesture begin)
    std::function<void()> onDragEnd;   // called on mouseUp   (for gesture end)

    void paint(juce::Graphics& g) override;
    void resized() override {}

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    Data   data_;
    float  localFillFraction_ = 0.0f;
    bool   isDragging_        = false;
    float  dragStartX_        = 0.0f;
    float  dragStartFill_     = 0.0f;

    int64_t lastThrottleMs_ = 0;
    static constexpr int kThrottleMs = 100;

    int fillFractionToValue(float f) const;
    void notifyChange(bool isDragging);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PotComponent)
};
