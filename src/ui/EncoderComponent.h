#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// Visual encoder — a circle with a marker dot on the rim showing position.
// Left encoder: page navigation.
// Right encoder: adjusts the focused pot.
class EncoderComponent : public juce::Component
{
public:
    enum class Role { PageNav, ValueAdjust };

    EncoderComponent(Role role, const juce::String& label);

    // Page nav encoder: called when page changes (delta = ±1)
    std::function<void(int delta)> onPageDelta;

    // Value adjust encoder: called when value delta changes
    std::function<void(int delta)> onValueDelta;

    // Update the displayed angle (0.0 = min, 1.0 = max)
    void setAngle(float normalised);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override;

private:
    Role         role_;
    juce::String label_;
    float        angle_    = 0.0f; // 0–1 normalised
    int          startY_   = 0;

    void dispatchDelta(int delta);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EncoderComponent)
};
