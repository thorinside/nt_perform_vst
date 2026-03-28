#include "PluginProcessor.h"
#include "PluginEditor.h"

NTPerformProcessor::NTPerformProcessor()
    : AudioProcessor(BusesProperties())
{
    // Timer drives the queue timeout checks and CC fifo draining
    startTimerHz(20); // 50ms
}

NTPerformProcessor::~NTPerformProcessor()
{
    stopTimer();
}

//==============================================================================
// processBlock — runs on the audio thread
//==============================================================================

void NTPerformProcessor::processBlock(juce::AudioBuffer<float>& /*audio*/,
                                      juce::MidiBuffer& midi)
{
    // 1. Inject any outgoing SysEx message
    {
        juce::ScopedLock sl(outgoingLock_);
        if (hasPendingOutgoing_)
        {
            midi.addEvent(pendingOutgoing_, 0);
            hasPendingOutgoing_ = false;
        }
    }

    // 2. Also drain the queue for the next outgoing message
    juce::MidiMessage outMsg;
    if (queue_.tryDequeueNext(outMsg))
    {
        midi.addEvent(outMsg, 0);
    }

    // 3. Scan incoming MIDI
    for (const auto meta : midi)
    {
        const auto& msg = meta.getMessage();

        if (msg.isSysEx())
        {
            DistingNT::ParsedMessage parsed;
            if (DistingNT::tryParse(msg, sysExId_.load(), parsed))
            {
                queue_.handleIncoming(parsed.commandByte,
                                      parsed.payload,
                                      parsed.payloadLen);
            }
        }
        else if (msg.isController())
        {
            // Push to lock-free ring buffer for message-thread processing
            int s1, b1, s2, b2;
            ccFifo_.prepareToWrite(1, s1, b1, s2, b2);
            const int slot = (b1 > 0) ? s1 : (b2 > 0 ? s2 : -1);
            if (slot >= 0)
            {
                ccBuffer_[slot] = { msg.getChannel() - 1,   // 0-based
                                    msg.getControllerNumber(),
                                    msg.getControllerValue() };
                ccFifo_.finishedWrite(b1 > 0 ? b1 : b2);
            }
        }
    }
}

//==============================================================================
// timerCallback — runs on the message thread
//==============================================================================

void NTPerformProcessor::timerCallback()
{
    // 1. Drive timeout logic
    queue_.tick();

    // 2. Drain CC events and update model
    {
        const int numReady = ccFifo_.getNumReady();
        if (numReady > 0)
        {
            int s1, b1, s2, b2;
            ccFifo_.prepareToRead(numReady, s1, b1, s2, b2);
            bool changed = false;

            auto processCcSlot = [&](int slot)
            {
                const auto& ev = ccBuffer_[slot];
                const auto* target = ccLookup_.find(ev.channel, ev.cc);
                if (target)
                {
                    const int value = CcReverseLookup::convertCcToValue(*target, ev.value);
                    model_.setParamValue(target->slotIndex, target->paramNumber, value);
                    changed = true;
                }
            };

            for (int i = 0; i < b1; ++i) processCcSlot(s1 + i);
            for (int i = 0; i < b2; ++i) processCcSlot(s2 + i);

            ccFifo_.finishedRead(b1 + b2);
            if (changed)
                sendChangeMessage();
        }
    }
}

//==============================================================================
// Protocol control
//==============================================================================

void NTPerformProcessor::refreshAllItems()
{
    queue_.clear();
    ccLookup_.clear();

    // Reset model loaded state
    for (int i = 0; i < PerformPageModel::kTotalItems; ++i)
    {
        PerformancePageItem empty;
        empty.itemIndex = i;
        model_.setItem(empty);
    }

    loadProgress_.store(0);
    refreshPhase_.store(1);

    startPhase1();
    sendChangeMessage();
}

void NTPerformProcessor::startPhase1()
{
    enqueueItemRequest(0);
}

void NTPerformProcessor::enqueueItemRequest(int itemIndex)
{
    if (itemIndex >= PerformPageModel::kTotalItems)
    {
        // Phase 1 complete — move to phase 2
        refreshPhase_.store(2);
        startPhase2();
        return;
    }

    SysExQueue::Request req;
    req.message             = DistingNT::buildRequestPerfPageItem(sysExId_.load(), itemIndex);
    req.expectedResponseCmd = DistingNT::kCmdRequestPerfPageItem;
    req.matchItemIndex      = itemIndex;

    req.onResponse = [this, itemIndex](const uint8_t* payload, int len)
    {
        DistingNT::PerfPageItemData data;
        if (DistingNT::parsePerfPageItemResponse(payload, len, data))
        {
            PerformancePageItem item;
            item.itemIndex       = data.itemIndex;
            item.enabled         = data.enabled;
            item.slotIndex       = data.slotIndex;
            item.parameterNumber = data.parameterNumber;
            item.min             = data.min;
            item.max             = data.max;
            std::strncpy(item.upperLabel, data.upperLabel, 32);
            std::strncpy(item.lowerLabel, data.lowerLabel, 32);
            model_.setItem(item);
            loadProgress_.fetch_add(1);
            sendChangeMessage();
        }

        // Chain to next item (called under queue lock — keep light)
        juce::MessageManager::callAsync([this, itemIndex]()
        {
            enqueueItemRequest(itemIndex + 1);
        });
    };

    req.onTimeout = [this, itemIndex]()
    {
        // Skip this item and continue
        loadProgress_.fetch_add(1);
        juce::MessageManager::callAsync([this, itemIndex]()
        {
            enqueueItemRequest(itemIndex + 1);
        });
    };

    queue_.enqueue(std::move(req));
}

void NTPerformProcessor::startPhase2()
{
    // Find enabled items and query their CC mappings
    mappingsPending_ = 0;
    for (int i = 0; i < PerformPageModel::kTotalItems; ++i)
    {
        const auto& item = model_.getItem(i);
        if (item.enabled)
            ++mappingsPending_;
    }

    if (mappingsPending_ == 0)
    {
        refreshPhase_.store(0);
        sendChangeMessage();
        return;
    }

    for (int i = 0; i < PerformPageModel::kTotalItems; ++i)
    {
        const auto& item = model_.getItem(i);
        if (item.enabled)
            enqueueMappingRequest(i);
    }
}

void NTPerformProcessor::enqueueMappingRequest(int itemIndex)
{
    const auto& item = model_.getItem(itemIndex);

    SysExQueue::Request req;
    req.message             = DistingNT::buildRequestMappings(sysExId_.load(),
                                                               item.slotIndex,
                                                               item.parameterNumber);
    req.expectedResponseCmd = DistingNT::kCmdRequestMappings;
    req.matchItemIndex      = -1; // any 0x4B response — the slot/param in payload identifies it

    req.onResponse = [this, itemIndex](const uint8_t* payload, int len)
    {
        DistingNT::MappingData md;
        if (DistingNT::parseMappingsResponse(payload, len, md))
        {
            if (md.enabled && md.midiCC >= 0)
            {
                const auto& item = model_.getItem(itemIndex);
                CcReverseLookup::CcTarget target;
                target.itemIndex   = itemIndex;
                target.slotIndex   = item.slotIndex;
                target.paramNumber = item.parameterNumber;
                target.paramMin    = item.min;
                target.paramMax    = item.max;
                ccLookup_.add(md.midiChannel, md.midiCC, target);
            }
        }

        --mappingsPending_;
        if (mappingsPending_ <= 0)
        {
            refreshPhase_.store(0);
            sendChangeMessage();
        }
    };

    req.onTimeout = [this]()
    {
        --mappingsPending_;
        if (mappingsPending_ <= 0)
        {
            refreshPhase_.store(0);
            sendChangeMessage();
        }
    };

    queue_.enqueue(std::move(req));
}

void NTPerformProcessor::sendParamValue(int slot, int param, int value)
{
    // Optimistic model update
    model_.setParamValue(slot, param, value);
    sendChangeMessage();

    // Fire-and-forget SysEx
    SysExQueue::Request req;
    req.message             = DistingNT::buildSetParamValue(sysExId_.load(), slot, param, value);
    req.expectedResponseCmd = 0;
    queue_.enqueue(std::move(req));
}

void NTPerformProcessor::setSysExId(int id)
{
    sysExId_.store(id & 0x7F);
    refreshAllItems();
}

//==============================================================================
// State save / restore
//==============================================================================

void NTPerformProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = std::make_unique<juce::XmlElement>("NTPerformState");
    xml->setAttribute("sysExId",     sysExId_.load());
    xml->setAttribute("currentPage", currentPage_.load());
    copyXmlToBinary(*xml, dest);
}

void NTPerformProcessor::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName("NTPerformState"))
    {
        sysExId_.store(xml->getIntAttribute("sysExId", 1));
        currentPage_.store(xml->getIntAttribute("currentPage", 0));
        refreshAllItems();
    }
}

//==============================================================================
// Editor
//==============================================================================

juce::AudioProcessorEditor* NTPerformProcessor::createEditor()
{
    return new NTPerformEditor(*this);
}

//==============================================================================
// Plugin entry point
//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NTPerformProcessor();
}
