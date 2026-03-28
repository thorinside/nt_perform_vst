#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <array>

#include "protocol/DistingNTProtocol.h"
#include "protocol/SysExQueue.h"
#include "protocol/CcReverseLookup.h"
#include "model/PerformPageModel.h"
#include "PerfPageParam.h"

class NTPerformProcessor : public juce::AudioProcessor,
                           public juce::ChangeBroadcaster,
                           private juce::Timer,
                           private juce::MidiInputCallback
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
    // Direct MIDI device management (bypasses DAW routing)
    void openMidiDevices(const juce::String& inputId,
                         const juce::String& outputId);
    juce::String getSelectedMidiInputId()  const;
    juce::String getSelectedMidiOutputId() const;

    //==========================================================================
    // Protocol control (call from message thread / editor)
    void refreshAllItems();
    void sendParamValue(int slot, int param, int value);
    void setSysExId(int id);

    //==========================================================================
    // Accessors
    PerformPageModel& getModel()          { return model_; }
    int               getSysExId() const  { return sysExId_.load(); }
    int               getLoadProgress() const { return loadProgress_.load(); }
    bool              isRefreshing() const    { return refreshPhase_.load() != 0; }
    int               getCurrentPage() const  { return currentPage_.load(); }
    void              setCurrentPage(int p)   { currentPage_.store(p); }
    juce::String      getFirmwareVersion() const;
    PerfPageParam*    getPerfParam(int index) const
    {
        if (index >= 0 && index < PerformPageModel::kTotalItems)
            return perfParams_[index];
        return nullptr;
    }

    // Activity: true if TX or RX fired in the last timer tick
    bool hasTxActivity() const { return txActivity_.load(); }
    bool hasRxActivity() const { return rxActivity_.load(); }

private:
    PerformPageModel model_;
    SysExQueue       queue_;
    CcReverseLookup  ccLookup_;

    // Automatable parameters — one per perf page slot, owned by JUCE base class.
    std::array<PerfPageParam*, PerformPageModel::kTotalItems> perfParams_ {};

    std::atomic<int>  sysExId_     { 1 };
    std::atomic<int>  currentPage_ { 0 };

    // Firmware version (set from message thread after 0x32 response)
    juce::String firmwareVersion_;
    mutable juce::CriticalSection fwVersionLock_;

    // Refresh state: 0=idle, 1=loading items, 2=loading mappings
    std::atomic<int> refreshPhase_ { 0 };
    std::atomic<int> loadProgress_ { 0 };
    int              mappingsPending_ = 0;

    // Activity indicators (set from any thread, cleared in timerCallback)
    std::atomic<bool> txActivity_ { false };
    std::atomic<bool> rxActivity_ { false };

    // Direct MIDI I/O (bypasses DAW routing)
    std::unique_ptr<juce::MidiInput>  midiInput_;
    std::unique_ptr<juce::MidiOutput> midiOutput_;
    juce::String selectedInputId_;
    juce::String selectedOutputId_;
    mutable juce::CriticalSection deviceLock_;

    // Lock-free CC ring buffer: MIDI callback thread → message thread
    struct CcEvent { int channel, cc, value; };
    static constexpr int kCcFifoSize = 256;
    juce::AbstractFifo               ccFifo_ { kCcFifoSize };
    std::array<CcEvent, kCcFifoSize> ccBuffer_;

    // Lock-free SysEx ring buffer: MIDI callback thread → message thread
    // Stores serialised SysEx messages as raw bytes prefixed by 2-byte length
    static constexpr int kSysExFifoSize = 4096;
    juce::AbstractFifo               sysexFifo_ { kSysExFifoSize };
    std::array<uint8_t, kSysExFifoSize> sysexBuffer_;

    //==========================================================================
    // MidiInputCallback (direct device — background thread)
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;

    // Send a MIDI message: via direct output if open, else buffered for processBlock
    void sendMidi(const juce::MidiMessage& msg);

    // Buffered outgoing message for processBlock fallback
    juce::MidiMessage   pendingOutgoing_;
    bool                hasPendingOutgoing_ = false;
    juce::CriticalSection outgoingLock_;

    //==========================================================================
    void timerCallback() override;
    void processIncomingMessage(const juce::MidiMessage& msg);
    void startPhase1();
    void startPhase2();
    void enqueueItemRequest(int itemIndex);
    void enqueueMappingRequest(int itemIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NTPerformProcessor)
};
