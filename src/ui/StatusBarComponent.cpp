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
    l.setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f)));
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
    firmwareLabel_.setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f)));
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
    const int w    = getWidth();
    const int row  = kHeight / 2;  // 32px per row
    const int itemH = 24;          // 3×8 — item height
    const int vpad  = (row - itemH) / 2; // 4px top/bottom within each row

    // Row 1: [8][lblW IN][8][comboW][16][lblW OUT][8][comboW][8][ledArea]
    const int lblW   = 32;  // 4×8 — wide enough for "OUT" at 12px
    const int lGap   = 8;   // gap between label and its combo
    const int sGap   = 16;  // gap between IN section and OUT section (1×16)
    const int ledArea = 48; // 3×16 — space reserved for TX/RX LEDs + right margin
    // fixed = 8 + lblW + lGap + sGap + lblW + lGap + ledArea
    const int fixed  = 8 + lblW + lGap + sGap + lblW + lGap + ledArea; // = 160
    const int comboW = (w - fixed) / 2;
    int x = 8;

    midiInLabel_.setBounds (x, vpad, lblW,  itemH); x += lblW + lGap;
    midiInCombo_.setBounds (x, vpad, comboW, itemH); x += comboW + sGap;
    midiOutLabel_.setBounds(x, vpad, lblW,  itemH); x += lblW + lGap;
    midiOutCombo_.setBounds(x, vpad, comboW, itemH);

    // Row 2: [8][80 SysEx][8][flex firmware][8][80 Refresh][8]
    const int row2y   = row;
    const int ctrlW   = 80; // 5×16
    const int fwX     = 8 + ctrlW + 8;
    const int fwW     = w - fwX - 8 - ctrlW - 8;

    sysExIdCombo_.setBounds(8,          row2y + vpad, ctrlW, itemH);
    firmwareLabel_.setBounds(fwX,       row2y + vpad, fwW,   itemH);
    refreshButton_.setBounds(w - 8 - ctrlW, row2y + vpad, ctrlW, itemH);
}

void StatusBarComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF181825));

    // Divider between rows
    g.setColour(juce::Colour(0xFF313244));
    g.drawHorizontalLine(getHeight() / 2, 0.0f, static_cast<float>(getWidth()));

    // TX/RX LEDs — top-right of row 1, within the 48px LED area
    const int row    = kHeight / 2; // 32
    const int ledSize = 12;         // 3×4
    const int ledY   = (row - ledSize) / 2; // centred in 32px row = 10
    const int txX    = getWidth() - 8 - ledSize;        // 8px right margin
    const int rxX    = getWidth() - 8 - ledSize - 8 - ledSize; // 8px gap between

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

    // LED labels (8px font, below LED)
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.0f)));
    g.setColour(juce::Colour(0xFF585B70));
    g.drawText("RX", rxX, ledY + ledSize + 2, ledSize, 8, juce::Justification::centred);
    g.drawText("TX", txX, ledY + ledSize + 2, ledSize, 8, juce::Justification::centred);
}
