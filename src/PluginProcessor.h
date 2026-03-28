#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <array>

#include "protocol/DistingNTProtocol.h"
#include "protocol/SysExQueue.h"
#include "protocol/CcReverseLookup.h"
#include "model/PerformPageModel.h"

class NTPerformProcessor : public juce::AudioProcessor,
                           public juce::ChangeBroadcaster,
                           private juce::Timer
{
public:
    NTPerformProcessor();
    ~NTPerformProcessor() override;

    //==========================================================================
    // AudioProcessor
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NT Perform"; }

    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    //==========================================================================
    // Protocol control (call from message thread / editor)
    void refreshAllItems();
    void sendParamValue(int slot, int param, int value);
    void setSysExId(int id);

    //==========================================================================
    // Accessors
    PerformPageModel& getModel()      { return model_; }
    int               getSysExId() const { return sysExId_.load(); }

    // How many items have been loaded during the current refresh (0-30)
    int getLoadProgress() const       { return loadProgress_.load(); }

    // True while a refresh is in progress
    bool isRefreshing() const         { return refreshPhase_.load() != 0; }

    // Current page cached for save/restore
    int  getCurrentPage() const       { return currentPage_.load(); }
    void setCurrentPage(int p)        { currentPage_.store(p); }

private:
    PerformPageModel model_;
    SysExQueue       queue_;
    CcReverseLookup  ccLookup_;

    std::atomic<int>  sysExId_     { 1 };
    std::atomic<int>  currentPage_ { 0 };

    // Refresh state: 0 = idle, 1 = loading items (phase1), 2 = loading mappings (phase2)
    std::atomic<int> refreshPhase_ { 0 };
    std::atomic<int> loadProgress_ { 0 };
    int              mappingsPending_ = 0; // protected by queue lock (message thread only)

    // Lock-free ring buffer: audio thread → message thread
    struct CcEvent { int channel, cc, value; };
    static constexpr int kCcFifoSize = 256;
    juce::AbstractFifo               ccFifo_ { kCcFifoSize };
    std::array<CcEvent, kCcFifoSize> ccBuffer_;

    // Message to inject into the MidiBuffer next processBlock call.
    // Protected by outgoingLock_.
    juce::MidiMessage   pendingOutgoing_;
    bool                hasPendingOutgoing_ = false;
    juce::CriticalSection outgoingLock_;

    void timerCallback() override;

    // Called on message thread: start phase 1 (query perf page items)
    void startPhase1();

    // Called on message thread after phase 1: start phase 2 (query CC mappings)
    void startPhase2();

    // Enqueue a single perf page item request, chaining to the next on response
    void enqueueItemRequest(int itemIndex);

    // Enqueue CC mapping request for a single item, counting down mappingsPending_
    void enqueueMappingRequest(int itemIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NTPerformProcessor)
};
