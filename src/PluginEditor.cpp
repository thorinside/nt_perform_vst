#include "PluginEditor.h"

NTPerformEditor::NTPerformEditor(NTPerformProcessor& proc)
    : AudioProcessorEditor(&proc), proc_(proc)
{
    setSize(560, 304);          // 35×16, 19×16
    setResizable(true, false);
    setResizeLimits(448, 256, 896, 608); // all multiples of 16

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
    statusBar_.onViewToggled = [this]()
    {
        setViewMode(viewMode_ == ViewMode::Standard ? ViewMode::Grid : ViewMode::Standard);
    };

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
        focusedPot_ = page * 3 + (focusedPot_ % 3);
        updatePage();
    };
    addAndMakeVisible(pageSelector_);

    // Encoders
    leftEncoder_.onPageDelta = [this](int delta)
    {
        const int newPage = juce::jlimit(0, PageSelectorComponent::kNumPages - 1,
                                         proc_.getCurrentPage() + delta);
        proc_.setCurrentPage(newPage);
        focusedPot_ = newPage * 3 + (focusedPot_ % 3);
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
        if (auto* p = proc_.getPerfParam(focusedPot_))
            p->sendValueChangedMessageToListeners(p->getValue());
    };
    addAndMakeVisible(leftEncoder_);
    addAndMakeVisible(rightEncoder_);

    // Pots — all 30 items; visibility managed by setViewMode / updatePage
    static const char* posLabels[] = { "Pot L", "Pot C", "Pot R" };
    for (int i = 0; i < 30; ++i)
    {
        auto& pot = pots_[i];
        PotComponent::Data d;
        d.posLabel = posLabels[i % 3];
        pot.setData(d);

        pot.onDragStart = [this, i]()
        {
            if (auto* p = proc_.getPerfParam(i))
                p->beginChangeGesture();
        };
        pot.onDragEnd = [this, i]()
        {
            if (auto* p = proc_.getPerfParam(i))
                p->endChangeGesture();
        };
        pot.onValueChange = [this, i](int slot, int param, int value, bool)
        {
            focusedPot_ = i;
            proc_.sendParamValue(slot, param, value);
            if (auto* p = proc_.getPerfParam(i))
                p->sendValueChangedMessageToListeners(p->getValue());
        };
        // Initially only show page 0 pots (standard mode)
        pot.setVisible(i < 3);
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
    const int page    = proc_.getCurrentPage();
    const auto& model = proc_.getModel();

    static const char* pagePos[] = { "Pot L", "Pot C", "Pot R" };

    for (int i = 0; i < 30; ++i)
    {
        const auto& item = model.getItem(i);
        const float fill = model.getFillFraction(i);

        PotComponent::Data d;
        d.posLabel     = pagePos[i % 3];
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

        // Visibility: in Standard mode only show the current page's 3 pots
        if (viewMode_ == ViewMode::Standard)
            pots_[i].setVisible(i / 3 == page);
    }

    if (viewMode_ == ViewMode::Standard)
    {
        pageSelector_.setCurrentPage(page);
        rightEncoder_.setAngle(pots_[focusedPot_].getData().fillFraction);
        leftEncoder_.setAngle(static_cast<float>(page) /
                              (PageSelectorComponent::kNumPages - 1));
    }
}

//==============================================================================

void NTPerformEditor::setViewMode(ViewMode mode)
{
    viewMode_ = mode;
    const bool isGrid = (mode == ViewMode::Grid);

    pageSelector_.setVisible(!isGrid);
    leftEncoder_.setVisible(!isGrid);
    rightEncoder_.setVisible(!isGrid);

    if (isGrid)
    {
        for (int i = 0; i < 30; ++i)
            pots_[i].setVisible(true);
        setResizeLimits(448, 384, 896, 1280);
        setSize(getWidth(), StatusBarComponent::kHeight + PageSelectorComponent::kNumPages * 48);
    }
    else
    {
        const int page = proc_.getCurrentPage();
        focusedPot_ = page * 3 + (focusedPot_ % 3);
        for (int i = 0; i < 30; ++i)
            pots_[i].setVisible(i / 3 == page);
        setResizeLimits(448, 256, 896, 608);
        setSize(getWidth(), 304);
    }

    statusBar_.setGridView(isGrid);
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
    statusBar_.setBounds(area.removeFromTop(StatusBarComponent::kHeight));

    if (viewMode_ == ViewMode::Standard)
    {
        pageSelector_.setBounds(area.removeFromTop(32));

        const int encoderW = 80;
        leftEncoder_.setBounds (area.removeFromLeft(encoderW));
        rightEncoder_.setBounds(area.removeFromRight(encoderW));

        // All pots in a page share the same 3 column positions (only 3 visible at a time)
        const int potW = area.getWidth() / 3;
        const juce::Rectangle<int> cols[3] = {
            area.withWidth(potW),
            area.withLeft(area.getX() + potW).withWidth(potW),
            area.withLeft(area.getX() + 2 * potW).withRight(area.getRight())
        };
        for (int i = 0; i < 30; ++i)
            pots_[i].setBounds(cols[i % 3]);
    }
    else
    {
        // Grid: 10 rows × 3 columns
        const int rowH = area.getHeight() / PageSelectorComponent::kNumPages;
        const int potW = area.getWidth() / 3;
        for (int row = 0; row < PageSelectorComponent::kNumPages; ++row)
        {
            const int y = area.getY() + row * rowH;
            const int h = (row == PageSelectorComponent::kNumPages - 1)
                              ? (area.getBottom() - y)
                              : rowH;
            for (int col = 0; col < 3; ++col)
            {
                const int x = area.getX() + col * potW;
                const int w = (col == 2) ? (area.getRight() - x) : potW;
                pots_[row * 3 + col].setBounds(x, y, w, h);
            }
        }
    }
}
