#include "StatusBarComponent.h"

StatusBarComponent::StatusBarComponent()
{
    // SysEx ID selector
    for (int i = 0; i <= 126; ++i)
        sysExIdCombo_.addItem("ID: " + juce::String(i), i + 1); // JUCE IDs are 1-based
    sysExIdCombo_.setSelectedId(2, juce::dontSendNotification); // default ID 1 → item ID 2
    sysExIdCombo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF313244));
    sysExIdCombo_.setColour(juce::ComboBox::textColourId,       juce::Colour(0xFFCDD6F4));
    sysExIdCombo_.setColour(juce::ComboBox::arrowColourId,      juce::Colour(0xFF9399B2));
    sysExIdCombo_.onChange = [this]()
    {
        const int id = sysExIdCombo_.getSelectedId() - 1; // convert back to 0-based
        if (onSysExIdChanged)
            onSysExIdChanged(id);
    };
    addAndMakeVisible(sysExIdCombo_);

    // Refresh button
    refreshButton_.setButtonText("Refresh");
    refreshButton_.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xFF45475A));
    refreshButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFCDD6F4));
    refreshButton_.onClick = [this]()
    {
        if (onRefreshClicked)
            onRefreshClicked();
    };
    addAndMakeVisible(refreshButton_);

    // Status label
    statusLabel_.setText("Not connected", juce::dontSendNotification);
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF9399B2));
    statusLabel_.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
    statusLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel_);
}

void StatusBarComponent::setSysExId(int id)
{
    sysExIdCombo_.setSelectedId(id + 1, juce::dontSendNotification);
}

void StatusBarComponent::setStatus(const juce::String& text)
{
    statusLabel_.setText(text, juce::dontSendNotification);
}

void StatusBarComponent::setRefreshing(bool refreshing, int loaded, int total)
{
    if (refreshing)
    {
        refreshButton_.setButtonText("Loading " + juce::String(loaded)
                                     + "/" + juce::String(total) + "...");
        refreshButton_.setEnabled(false);
    }
    else
    {
        refreshButton_.setButtonText("Refresh");
        refreshButton_.setEnabled(true);
    }
}

void StatusBarComponent::resized()
{
    const int h = getHeight();
    sysExIdCombo_.setBounds  (4,              2, 90,           h - 4);
    refreshButton_.setBounds (getWidth() - 100, 2, 96,           h - 4);
    statusLabel_.setBounds   (100,            2, getWidth() - 204, h - 4);
}

void StatusBarComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF181825));
}
