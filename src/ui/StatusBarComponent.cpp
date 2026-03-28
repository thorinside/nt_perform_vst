#include "StatusBarComponent.h"

static void styleCombo(juce::ComboBox& c)
{
    c.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF313244));
    c.setColour(juce::ComboBox::textColourId,       juce::Colour(0xFFCDD6F4));
    c.setColour(juce::ComboBox::arrowColourId,      juce::Colour(0xFF9399B2));
    c.setColour(juce::ComboBox::outlineColourId,    juce::Colour(0xFF45475A));
}

static void styleLabel(juce::Label& l, const juce::String& text)
{
    l.setText(text, juce::dontSendNotification);
    l.setColour(juce::Label::textColourId, juce::Colour(0xFF9399B2));
    l.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
    l.setJustificationType(juce::Justification::centredRight);
}

StatusBarComponent::StatusBarComponent()
{
    // ── Row 1: MIDI device selectors ─────────────────────────────────────────

    styleLabel(midiInLabel_, "IN");
    addAndMakeVisible(midiInLabel_);

    midiInCombo_.setTextWhenNothingSelected("-- none --");
    styleCombo(midiInCombo_);
    midiInCombo_.onChange = [this]()
    {
        const int id = midiInCombo_.getSelectedId();
        if (onMidiInputChanged)
        {
            // ID 1 = "none", IDs 2+ = device index+2
            onMidiInputChanged(id <= 1 ? juce::String() : midiInCombo_.getItemText(midiInCombo_.getSelectedItemIndex()));
        }
    };
    addAndMakeVisible(midiInCombo_);

    styleLabel(midiOutLabel_, "OUT");
    addAndMakeVisible(midiOutLabel_);

    midiOutCombo_.setTextWhenNothingSelected("-- none --");
    styleCombo(midiOutCombo_);
    midiOutCombo_.onChange = [this]()
    {
        const int id = midiOutCombo_.getSelectedId();
        if (onMidiOutputChanged)
            onMidiOutputChanged(id <= 1 ? juce::String() : midiOutCombo_.getItemText(midiOutCombo_.getSelectedItemIndex()));
    };
    addAndMakeVisible(midiOutCombo_);

    // ── Row 2: SysEx ID, firmware, refresh ───────────────────────────────────

    for (int i = 0; i <= 126; ++i)
        sysExIdCombo_.addItem("ID: " + juce::String(i), i + 1);
    sysExIdCombo_.setSelectedId(2, juce::dontSendNotification);
    styleCombo(sysExIdCombo_);
    sysExIdCombo_.onChange = [this]()
    {
        if (onSysExIdChanged)
            onSysExIdChanged(sysExIdCombo_.getSelectedId() - 1);
    };
    addAndMakeVisible(sysExIdCombo_);

    firmwareLabel_.setText("firmware: --", juce::dontSendNotification);
    firmwareLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF9399B2));
    firmwareLabel_.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
    firmwareLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(firmwareLabel_);

    refreshButton_.setButtonText("Refresh");
    refreshButton_.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xFF45475A));
    refreshButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFCDD6F4));
    refreshButton_.onClick = [this]() { if (onRefreshClicked) onRefreshClicked(); };
    addAndMakeVisible(refreshButton_);

    refreshDeviceLists();
    startTimerHz(15); // LED fade
}

//==============================================================================

void StatusBarComponent::refreshDeviceLists()
{
    const auto prevInId  = midiInCombo_.getItemText (midiInCombo_.getSelectedItemIndex());
    const auto prevOutId = midiOutCombo_.getItemText(midiOutCombo_.getSelectedItemIndex());

    midiInCombo_.clear(juce::dontSendNotification);
    midiOutCombo_.clear(juce::dontSendNotification);

    midiInCombo_.addItem("-- none --", 1);
    midiOutCombo_.addItem("-- none --", 1);

    const auto inputs  = juce::MidiInput::getAvailableDevices();
    const auto outputs = juce::MidiOutput::getAvailableDevices();

    for (int i = 0; i < inputs.size(); ++i)
    {
        midiInCombo_.addItem(inputs[i].name, i + 2);
        if (inputs[i].name == prevInId)
            midiInCombo_.setSelectedId(i + 2, juce::dontSendNotification);
    }

    for (int i = 0; i < outputs.size(); ++i)
    {
        midiOutCombo_.addItem(outputs[i].name, i + 2);
        if (outputs[i].name == prevOutId)
            midiOutCombo_.setSelectedId(i + 2, juce::dontSendNotification);
    }
}

void StatusBarComponent::setSelectedMidiInput(const juce::String& deviceId)
{
    const auto inputs = juce::MidiInput::getAvailableDevices();
    for (int i = 0; i < inputs.size(); ++i)
    {
        if (inputs[i].identifier == deviceId)
        {
            midiInCombo_.setSelectedId(i + 2, juce::dontSendNotification);
            return;
        }
    }
    midiInCombo_.setSelectedId(1, juce::dontSendNotification);
}

void StatusBarComponent::setSelectedMidiOutput(const juce::String& deviceId)
{
    const auto outputs = juce::MidiOutput::getAvailableDevices();
    for (int i = 0; i < outputs.size(); ++i)
    {
        if (outputs[i].identifier == deviceId)
        {
            midiOutCombo_.setSelectedId(i + 2, juce::dontSendNotification);
            return;
        }
    }
    midiOutCombo_.setSelectedId(1, juce::dontSendNotification);
}

void StatusBarComponent::setSysExId(int id)
{
    sysExIdCombo_.setSelectedId(id + 1, juce::dontSendNotification);
}

void StatusBarComponent::setFirmwareVersion(const juce::String& version)
{
    firmwareLabel_.setText("firmware: " + version, juce::dontSendNotification);
}

void StatusBarComponent::setStatus(const juce::String& /*text*/)
{
    // Status is shown via firmware label; kept for compatibility
}

void StatusBarComponent::flashTx()
{
    txBrightness_.store(255);
    repaint();
}

void StatusBarComponent::flashRx()
{
    rxBrightness_.store(255);
    repaint();
}

void StatusBarComponent::timerCallback()
{
    // Fade LEDs
    bool needRepaint = false;
    auto fade = [&](std::atomic<int>& b)
    {
        const int v = b.load();
        if (v > 0) { b.store(std::max(0, v - 40)); needRepaint = true; }
    };
    fade(txBrightness_);
    fade(rxBrightness_);
    if (needRepaint) repaint();
}

//==============================================================================

void StatusBarComponent::resized()
{
    const int h = getHeight() / 2;
    const int w = getWidth();

    // Row 1: [IN lbl 20] [IN combo flex] [OUT lbl 22] [OUT combo flex] [TX● 20] [RX● 20]
    const int ledW   = 22;
    const int lblW   = 24;
    const int comboW = (w - lblW * 2 - ledW * 2 - 8) / 2;
    int x = 4;

    midiInLabel_.setBounds (x, 2, lblW, h - 4);  x += lblW;
    midiInCombo_.setBounds (x, 2, comboW, h - 4); x += comboW + 4;
    midiOutLabel_.setBounds(x, 2, lblW, h - 4);  x += lblW;
    midiOutCombo_.setBounds(x, 2, comboW, h - 4);
    // LEDs are painted in paint(), no component needed

    // Row 2: [SysEx ID 80] [firmware label flex] [Refresh 80]
    const int row2y = h;
    sysExIdCombo_.setBounds (4,         row2y + 2, 80,         h - 4);
    refreshButton_.setBounds(w - 84,    row2y + 2, 80,         h - 4);
    firmwareLabel_.setBounds(88,        row2y + 2, w - 176,    h - 4);
}

void StatusBarComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF181825));

    // Divider between rows
    g.setColour(juce::Colour(0xFF313244));
    g.drawHorizontalLine(getHeight() / 2, 0.0f, static_cast<float>(getWidth()));

    // TX/RX LEDs (top-right of row 1)
    const int h      = getHeight() / 2;
    const int ledSize = 10;
    const int ledY   = (h - ledSize) / 2;
    const int txX    = getWidth() - 22;
    const int rxX    = getWidth() - 44;

    auto drawLed = [&](int x, int brightness, juce::Colour onColour)
    {
        const float alpha = brightness / 255.0f;
        const juce::Colour col = onColour.withAlpha(0.2f + alpha * 0.8f);
        g.setColour(col);
        g.fillEllipse(static_cast<float>(x), static_cast<float>(ledY),
                      static_cast<float>(ledSize), static_cast<float>(ledSize));
        g.setColour(juce::Colour(0xFF585B70));
        g.drawEllipse(static_cast<float>(x), static_cast<float>(ledY),
                      static_cast<float>(ledSize), static_cast<float>(ledSize), 1.0f);
    };

    // RX = green, TX = yellow
    drawLed(rxX, rxBrightness_.load(), juce::Colour(0xFF00FF88));
    drawLed(txX, txBrightness_.load(), juce::Colour(0xFFFFD700));

    // LED labels
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.0f)));
    g.setColour(juce::Colour(0xFF585B70));
    g.drawText("RX", rxX - 1, ledY + ledSize + 1, ledSize + 2, 9,
               juce::Justification::centred);
    g.drawText("TX", txX - 1, ledY + ledSize + 1, ledSize + 2, 9,
               juce::Justification::centred);
}
