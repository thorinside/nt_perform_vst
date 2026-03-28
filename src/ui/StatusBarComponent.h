#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class StatusBarComponent : public juce::Component
{
public:
    StatusBarComponent();

    // Called when SysEx ID changes (0-126)
    std::function<void(int id)> onSysExIdChanged;

    // Called when Refresh is clicked
    std::function<void()> onRefreshClicked;

    void setSysExId(int id);
    void setStatus(const juce::String& text);
    void setRefreshing(bool refreshing, int loaded = 0, int total = 30);

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    juce::ComboBox   sysExIdCombo_;
    juce::TextButton refreshButton_;
    juce::Label      statusLabel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBarComponent)
};
