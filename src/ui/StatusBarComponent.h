#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>

// Two-row status/connection bar.
// Row 1 (connection): MIDI In selector | MIDI Out selector | TX● | RX●
// Row 2 (controls):   SysEx ID | firmware version | Refresh button
class StatusBarComponent : public juce::Component,
                           private juce::Timer
{
public:
    StatusBarComponent();

    // Callbacks
    std::function<void(const juce::String& id)> onMidiInputChanged;
    std::function<void(const juce::String& id)> onMidiOutputChanged;
    std::function<void(int id)>                 onSysExIdChanged;
    std::function<void()>                        onRefreshClicked;

    // Update state from processor
    void setSelectedMidiInput (const juce::String& deviceId);
    void setSelectedMidiOutput(const juce::String& deviceId);
    void setSysExId(int id);
    void setFirmwareVersion(const juce::String& version);
    void setStatus(const juce::String& text);
    void flashTx();
    void flashRx();

    // Rebuild device lists (call after MIDI device changes)
    void refreshDeviceLists();

    static constexpr int kHeight = 64; // total height for both rows (4×16)

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    // Row 1
    juce::Label    midiInLabel_;
    juce::ComboBox midiInCombo_;
    juce::Label    midiOutLabel_;
    juce::ComboBox midiOutCombo_;

    // TX/RX LEDs (drawn in paint)
    std::atomic<int> txBrightness_ { 0 };
    std::atomic<int> rxBrightness_ { 0 };

    // Row 2
    juce::ComboBox   sysExIdCombo_;
    juce::Label      firmwareLabel_;
    juce::TextButton refreshButton_;

    void timerCallback() override; // fades LEDs

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBarComponent)
};
