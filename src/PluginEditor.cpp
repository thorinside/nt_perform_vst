#include "PluginEditor.h"

NTPerformEditor::NTPerformEditor(NTPerformProcessor& proc)
    : AudioProcessorEditor(&proc), proc_(proc)
{
    setSize(520, 260);
    setResizable(true, false);
    setResizeLimits(400, 220, 900, 500);

    // Status bar
    statusBar_.onSysExIdChanged = [this](int id) { proc_.setSysExId(id); };
    statusBar_.onRefreshClicked = [this]() { proc_.refreshAllItems(); };
    statusBar_.setSysExId(proc_.getSysExId());
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

        const float step  = 1.0f / static_cast<float>(data.rangeMax - data.rangeMin + 1);
        const float newFill = juce::jlimit(0.0f, 1.0f,
                                            data.fillFraction + delta * step);
        // Compute value and send
        const int newValue = data.rangeMin
            + static_cast<int>(newFill * (data.rangeMax - data.rangeMin) + 0.5f);
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
        pot.onValueChange = [this, idx](int slot, int param, int value, bool /*dragging*/)
        {
            focusedPot_ = idx;
            proc_.sendParamValue(slot, param, value);
        };
        addAndMakeVisible(pot);
    }

    proc_.addChangeListener(this);
    updatePage();
    updateStatus();
}

NTPerformEditor::~NTPerformEditor()
{
    proc_.removeChangeListener(this);
}

//==============================================================================

void NTPerformEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    updatePage();
    updateStatus();
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
        d.posLabel    = pos[i];
        d.enabled     = item.enabled;
        d.isEmpty     = !item.enabled;
        d.upperLabel  = item.upperLabel[0] ? juce::String(item.upperLabel)
                                           : juce::String(item.lowerLabel); // fallback
        d.lowerLabel  = item.lowerLabel;
        d.fillFraction = fill >= 0.0f ? fill : 0.0f;
        d.slotIndex   = item.slotIndex;
        d.paramNumber = item.parameterNumber;
        d.rangeMin    = item.min;
        d.rangeMax    = item.max;

        pots_[i].setData(d);
    }

    // Update right encoder angle to show focused pot position
    const auto& focusData = pots_[focusedPot_].getData();
    rightEncoder_.setAngle(focusData.fillFraction);

    // Update left encoder angle to show current page position
    leftEncoder_.setAngle(static_cast<float>(page)
                          / (PageSelectorComponent::kNumPages - 1));
}

void NTPerformEditor::updateStatus()
{
    const bool refreshing = proc_.isRefreshing();
    const int  loaded     = proc_.getLoadProgress();

    statusBar_.setSysExId(proc_.getSysExId());
    statusBar_.setRefreshing(refreshing, loaded, PerformPageModel::kTotalItems);

    if (refreshing)
    {
        statusBar_.setStatus("Loading items...");
    }
    else if (proc_.getModel().isFullyLoaded())
    {
        statusBar_.setStatus("Ready");
    }
    else
    {
        statusBar_.setStatus("Not connected — click Refresh");
    }
}

//==============================================================================

void NTPerformEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1E1E2E));
}

void NTPerformEditor::resized()
{
    auto area = getLocalBounds();

    // Status bar at top
    statusBar_.setBounds(area.removeFromTop(30));

    // Page selector below
    pageSelector_.setBounds(area.removeFromTop(36));

    // Remaining area: [left encoder] [3 pots] [right encoder]
    const int encoderW = 80;
    leftEncoder_.setBounds (area.removeFromLeft(encoderW));
    rightEncoder_.setBounds(area.removeFromRight(encoderW));

    // 3 pots share the remaining width
    const int potW = area.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
        pots_[i].setBounds(area.removeFromLeft(i < 2 ? potW : area.getWidth()));
}
