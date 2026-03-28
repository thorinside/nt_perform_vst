#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

#include "PluginProcessor.h"
#include "ui/PotComponent.h"
#include "ui/EncoderComponent.h"
#include "ui/PageSelectorComponent.h"
#include "ui/StatusBarComponent.h"

class NTPerformEditor : public juce::AudioProcessorEditor,
                        private juce::ChangeListener
{
public:
    explicit NTPerformEditor(NTPerformProcessor& proc);
    ~NTPerformEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    NTPerformProcessor& proc_;

    StatusBarComponent   statusBar_;
    PageSelectorComponent pageSelector_;

    EncoderComponent leftEncoder_  { EncoderComponent::Role::PageNav,    "Page" };
    EncoderComponent rightEncoder_ { EncoderComponent::Role::ValueAdjust, "Adjust" };

    std::array<PotComponent, 3> pots_;

    int focusedPot_ = 0; // 0–2, which pot the right encoder adjusts

    // ChangeListener — called on message thread when model changes
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Rebuild pot display from model for the current page
    void updatePage();

    // Update status bar text
    void updateStatus();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NTPerformEditor)
};
