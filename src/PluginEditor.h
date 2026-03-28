#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

#include "PluginProcessor.h"
#include "ui/PotComponent.h"
#include "ui/PageSelectorComponent.h"
#include "ui/StatusBarComponent.h"

class NTPerformEditor : public juce::AudioProcessorEditor,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    explicit NTPerformEditor(NTPerformProcessor& proc);
    ~NTPerformEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    NTPerformProcessor& proc_;

    StatusBarComponent    statusBar_;
    PageSelectorComponent pageSelector_;

    enum class ViewMode { Standard, Grid };
    ViewMode viewMode_ = ViewMode::Standard;

    std::array<PotComponent, 30> pots_;
    int focusedPot_ = 0; // absolute item index 0–29

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override; // polls TX/RX activity for LED flashing
    void updatePage();
    void setViewMode(ViewMode mode);

    // Translate device name from combo to identifier, then open ports
    void handleMidiInputSelected(const juce::String& name);
    void handleMidiOutputSelected(const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NTPerformEditor)
};
