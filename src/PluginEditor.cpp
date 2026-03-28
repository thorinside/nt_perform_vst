#include "PluginEditor.h"

NTPerformEditor::NTPerformEditor(NTPerformProcessor& proc)
    : AudioProcessorEditor(&proc), proc_(proc)
{
    setSize(560, 300);
    setResizable(true, false);
    setResizeLimits(440, 260, 900, 600);

    // Status bar
    statusBar_.onMidiInputChanged  = [this](const juce::String& name)
    {
        handleMidiInputSelected(name);
    };
    statusBar_.onMidiOutputChanged = [this](const juce::String& name)
    {
        handleMidiOutputSelected(name);
    };
    statusBar_.onSysExIdChanged = [this](int id) { proc_.setSysExId(id); };
    statusBar_.onRefreshClicked = [this]()       { proc_.refreshAllItems(); };

    statusBar_.setSysExId(proc_.getSysExId());
    statusBar_.setSelectedMidiInput (proc_.getSelectedMidiInputId());
    statusBar_.setSelectedMidiOutput(proc_.getSelectedMidiOutputId());
    statusBar_.setFirmwareVersion(proc_.getFirmwareVersion());
    addAndMakeVisible(statusBar_);

    // Page selector
    pageSelector_.setCurrentPage(proc_.getCurrentPage());
    pageSelector_.onPageSelected = [this](int page)
    {
        proc_.setCurrentPage(page);
        updatePage();
    };
    addAndMakeVisible(pageSelector_);

    // Encoders
    leftEncoder_.onPageDelta = [this](int delta)
    {
        const int newPage = juce::jlimit(0, PageSelectorComponent::kNumPages - 1,
                                         pageSelector_.getCurrentPage() + delta);
        pageSelector_.setCurrentPage(newPage);
        proc_.setCurrentPage(newPage);
        updatePage();
    };
    rightEncoder_.onValueDelta = [this](int delta)
    {
        const auto& data = pots_[focusedPot_].getData();
        if (!data.enabled || data.isEmpty) return;
        const float step    = 1.0f / static_cast<float>(
                                  std::max(1, data.rangeMax - data.rangeMin));
        const float newFill = juce::jlimit(0.0f, 1.0f, data.fillFraction + delta * step);
        const int newValue  = data.rangeMin +
            static_cast<int>(newFill * (data.rangeMax - data.rangeMin) + 0.5f);
        proc_.sendParamValue(data.slotIndex, data.paramNumber, newValue);
    };
    addAndMakeVisible(leftEncoder_);
    addAndMakeVisible(rightEncoder_);

    // Pots
    static const char* posLabels[] = { "Pot L", "Pot C", "Pot R" };
    for (int i = 0; i < 3; ++i)
    {
        auto& pot = pots_[i];
        PotComponent::Data d;
        d.posLabel = posLabels[i];
        pot.setData(d);

        const int idx = i;
        pot.onValueChange = [this, idx](int slot, int param, int value, bool)
        {
            focusedPot_ = idx;
            proc_.sendParamValue(slot, param, value);
        };
        addAndMakeVisible(pot);
    }

    proc_.addChangeListener(this);
    startTimerHz(20); // poll TX/RX activity
    updatePage();
}

NTPerformEditor::~NTPerformEditor()
{
    stopTimer();
    proc_.removeChangeListener(this);
}

//==============================================================================

void NTPerformEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    statusBar_.setFirmwareVersion(proc_.getFirmwareVersion());
    statusBar_.setSysExId(proc_.getSysExId());
    updatePage();
}

void NTPerformEditor::timerCallback()
{
    if (proc_.hasTxActivity()) statusBar_.flashTx();
    if (proc_.hasRxActivity()) statusBar_.flashRx();
}

void NTPerformEditor::updatePage()
{
    const int page           = pageSelector_.getCurrentPage();
    const auto& model        = proc_.getModel();
    static const char* pos[] = { "Pot L", "Pot C", "Pot R" };

    for (int i = 0; i < 3; ++i)
    {
        const int itemIndex = page * 3 + i;
        const auto& item    = model.getItem(itemIndex);
        const float fill    = model.getFillFraction(itemIndex);

        PotComponent::Data d;
        d.posLabel     = pos[i];
        d.enabled      = item.enabled;
        d.isEmpty      = !item.enabled;
        d.upperLabel   = item.upperLabel[0] ? juce::String(item.upperLabel)
                                            : juce::String(item.lowerLabel);
        d.lowerLabel   = item.lowerLabel;
        d.fillFraction = fill >= 0.0f ? fill : 0.0f;
        d.slotIndex    = item.slotIndex;
        d.paramNumber  = item.parameterNumber;
        d.rangeMin     = item.min;
        d.rangeMax     = item.max;
        pots_[i].setData(d);
    }

    rightEncoder_.setAngle(pots_[focusedPot_].getData().fillFraction);
    leftEncoder_.setAngle(static_cast<float>(page) /
                          (PageSelectorComponent::kNumPages - 1));
}

//==============================================================================

void NTPerformEditor::handleMidiInputSelected(const juce::String& name)
{
    // Translate name → identifier
    juce::String inputId, outputId = proc_.getSelectedMidiOutputId();
    if (name.isNotEmpty())
    {
        for (const auto& d : juce::MidiInput::getAvailableDevices())
            if (d.name == name) { inputId = d.identifier; break; }
    }
    proc_.openMidiDevices(inputId, outputId);
}

void NTPerformEditor::handleMidiOutputSelected(const juce::String& name)
{
    juce::String inputId = proc_.getSelectedMidiInputId(), outputId;
    if (name.isNotEmpty())
    {
        for (const auto& d : juce::MidiOutput::getAvailableDevices())
            if (d.name == name) { outputId = d.identifier; break; }
    }
    proc_.openMidiDevices(inputId, outputId);
}

//==============================================================================

void NTPerformEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1E1E2E));
}

void NTPerformEditor::resized()
{
    auto area = getLocalBounds();
    statusBar_.setBounds  (area.removeFromTop(StatusBarComponent::kHeight));
    pageSelector_.setBounds(area.removeFromTop(36));

    const int encoderW = 80;
    leftEncoder_.setBounds (area.removeFromLeft(encoderW));
    rightEncoder_.setBounds(area.removeFromRight(encoderW));

    const int potW = area.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
        pots_[i].setBounds(area.removeFromLeft(i < 2 ? potW : area.getWidth()));
}
