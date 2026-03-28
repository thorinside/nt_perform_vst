#include "PluginProcessor.h"
#include "PluginEditor.h"

NTPerformProcessor::NTPerformProcessor()
    : AudioProcessor(BusesProperties())
{
    for (int i = 0; i < PerformPageModel::kTotalItems; ++i)
        perfParams_[i] = new PerfPageParam(*this, model_, i);

    // addParameter transfers ownership to the base class.
    for (auto* p : perfParams_)
        addParameter(p);

    startTimerHz(20); // 50ms
}

NTPerformProcessor::~NTPerformProcessor()
{
    stopTimer();
    juce::ScopedLock sl(deviceLock_);
    midiInput_.reset();
    midiOutput_.reset();
}

//==============================================================================
// Direct MIDI device management
//==============================================================================

void NTPerformProcessor::openMidiDevices(const juce::String& inputId,
                                          const juce::String& outputId)
{
    {
        juce::ScopedLock sl(deviceLock_);
        midiInput_.reset();
        midiOutput_.reset();

        if (inputId.isNotEmpty())
            midiInput_ = juce::MidiInput::openDevice(inputId, this);

        if (outputId.isNotEmpty())
            midiOutput_ = juce::MidiOutput::openDevice(outputId);

        selectedInputId_  = inputId;
        selectedOutputId_ = outputId;

        if (midiInput_)
            midiInput_->start();
    }

    refreshAllItems();
}

juce::String NTPerformProcessor::getSelectedMidiInputId() const
{
    juce::ScopedLock sl(deviceLock_);
    return selectedInputId_;
}

juce::String NTPerformProcessor::getSelectedMidiOutputId() const
{
    juce::ScopedLock sl(deviceLock_);
    return selectedOutputId_;
}

juce::String NTPerformProcessor::getFirmwareVersion() const
{
    juce::ScopedLock sl(fwVersionLock_);
    return firmwareVersion_;
}

//==============================================================================
// Send MIDI: direct output if available, else buffered for processBlock
//==============================================================================

void NTPerformProcessor::sendMidi(const juce::MidiMessage& msg)
{
    txActivity_.store(true);

    juce::ScopedLock sl(deviceLock_);
    if (midiOutput_)
    {
        midiOutput_->sendMessageNow(msg);
    }
    else
    {
        juce::ScopedLock ol(outgoingLock_);
        pendingOutgoing_    = msg;
        hasPendingOutgoing_ = true;
    }
}

//==============================================================================
// MidiInputCallback — background thread
//==============================================================================

void NTPerformProcessor::handleIncomingMidiMessage(juce::MidiInput*,
                                                    const juce::MidiMessage& msg)
{
    // Route through the same path as processBlock but from the direct input
    rxActivity_.store(true);

    if (msg.isSysEx())
    {
        // Serialise into the lock-free SysEx fifo for message-thread processing
        const int msgSize = msg.getRawDataSize();
        const int needed  = 2 + msgSize; // 2-byte length prefix
        int s1, b1, s2, b2;
        sysexFifo_.prepareToWrite(needed, s1, b1, s2, b2);
        if (b1 + b2 >= needed)
        {
            // Write length (big-endian 16-bit)
            auto writeByte = [&](int pos, uint8_t byte)
            {
                const int wrapped = pos % kSysExFifoSize;
                if (pos < s1 + b1)
                    sysexBuffer_[s1 + (pos - s1)] = byte;
                else
                    sysexBuffer_[s2 + (pos - (s1 + b1))] = byte;
                (void)wrapped;
            };
            // Simpler: just use linear write since messages are small
            // Pack into a temporary and copy
            std::vector<uint8_t> tmp(needed);
            tmp[0] = static_cast<uint8_t>((msgSize >> 8) & 0xFF);
            tmp[1] = static_cast<uint8_t>(msgSize & 0xFF);
            std::memcpy(tmp.data() + 2, msg.getRawData(), msgSize);

            int written = 0;
            for (int i = 0; i < b1 && written < needed; ++i, ++written)
                sysexBuffer_[s1 + i] = tmp[written];
            for (int i = 0; i < b2 && written < needed; ++i, ++written)
                sysexBuffer_[s2 + i] = tmp[written];

            sysexFifo_.finishedWrite(needed);
        }
    }
    else if (msg.isController())
    {
        int s1, b1, s2, b2;
        ccFifo_.prepareToWrite(1, s1, b1, s2, b2);
        const int slot = (b1 > 0) ? s1 : (b2 > 0 ? s2 : -1);
        if (slot >= 0)
        {
            ccBuffer_[slot] = { msg.getChannel() - 1,
                                msg.getControllerNumber(),
                                msg.getControllerValue() };
            ccFifo_.finishedWrite(b1 > 0 ? b1 : b2);
        }
    }
}

//==============================================================================
// processBlock — audio thread (DAW routing fallback)
//==============================================================================

void NTPerformProcessor::processBlock(juce::AudioBuffer<float>& /*audio*/,
                                       juce::MidiBuffer& midi)
{
    // Inject queued outgoing message (DAW-routing fallback only)
    {
        juce::ScopedLock ol(outgoingLock_);
        if (hasPendingOutgoing_)
        {
            midi.addEvent(pendingOutgoing_, 0);
            hasPendingOutgoing_ = false;
        }
    }

    // Drain queue for next outgoing (DAW-routing fallback)
    {
        juce::ScopedLock sl(deviceLock_);
        if (!midiOutput_)
        {
            juce::MidiMessage outMsg;
            if (queue_.tryDequeueNext(outMsg))
            {
                txActivity_.store(true);
                midi.addEvent(outMsg, 0);
            }
        }
    }

    // Scan incoming MIDI from DAW
    for (const auto meta : midi)
    {
        const auto& msg = meta.getMessage();
        if (msg.isSysEx() || msg.isController())
        {
            rxActivity_.store(true);
            if (msg.isSysEx())
            {
                DistingNT::ParsedMessage parsed;
                if (DistingNT::tryParse(msg, sysExId_.load(), parsed))
                    queue_.handleIncoming(parsed.commandByte, parsed.payload, parsed.payloadLen);
            }
            else if (msg.isController())
            {
                int s1, b1, s2, b2;
                ccFifo_.prepareToWrite(1, s1, b1, s2, b2);
                const int slot = (b1 > 0) ? s1 : (b2 > 0 ? s2 : -1);
                if (slot >= 0)
                {
                    ccBuffer_[slot] = { msg.getChannel() - 1,
                                        msg.getControllerNumber(),
                                        msg.getControllerValue() };
                    ccFifo_.finishedWrite(b1 > 0 ? b1 : b2);
                }
            }
        }
    }
}

//==============================================================================
// timerCallback — message thread (50ms)
//==============================================================================

void NTPerformProcessor::timerCallback()
{
    // 1. Drain direct SysEx fifo (from MidiInputCallback)
    {
        const int numReady = sysexFifo_.getNumReady();
        if (numReady >= 2)
        {
            // Read in chunks, reassemble messages
            std::vector<uint8_t> buf(numReady);
            int s1, b1, s2, b2;
            sysexFifo_.prepareToRead(numReady, s1, b1, s2, b2);
            int idx = 0;
            for (int i = 0; i < b1; ++i) buf[idx++] = sysexBuffer_[s1 + i];
            for (int i = 0; i < b2; ++i) buf[idx++] = sysexBuffer_[s2 + i];
            sysexFifo_.finishedRead(b1 + b2);

            int pos = 0;
            while (pos + 2 <= numReady)
            {
                const int msgSize = (buf[pos] << 8) | buf[pos + 1];
                pos += 2;
                if (pos + msgSize > numReady) break;
                const juce::MidiMessage msg(buf.data() + pos, msgSize);
                pos += msgSize;

                DistingNT::ParsedMessage parsed;
                if (DistingNT::tryParse(msg, sysExId_.load(), parsed))
                    queue_.handleIncoming(parsed.commandByte, parsed.payload, parsed.payloadLen);
            }
        }
    }

    // 2. Drive queue timeouts
    queue_.tick();

    // 3. If direct output open, drain queue and send
    {
        juce::ScopedLock sl(deviceLock_);
        if (midiOutput_)
        {
            juce::MidiMessage outMsg;
            if (queue_.tryDequeueNext(outMsg))
            {
                txActivity_.store(true);
                midiOutput_->sendMessageNow(outMsg);
            }
        }
    }

    // 4. Drain CC events
    {
        const int numReady = ccFifo_.getNumReady();
        if (numReady > 0)
        {
            int s1, b1, s2, b2;
            ccFifo_.prepareToRead(numReady, s1, b1, s2, b2);
            bool changed = false;

            auto processCc = [&](int slot)
            {
                const auto& ev = ccBuffer_[slot];
                const auto* target = ccLookup_.find(ev.channel, ev.cc);
                if (target)
                {
                    const int value = CcReverseLookup::convertCcToValue(*target, ev.value);
                    model_.setParamValue(target->slotIndex, target->paramNumber, value);
                    perfParams_[target->itemIndex]->notifyValueChanged();
                    changed = true;
                }
            };

            for (int i = 0; i < b1; ++i) processCc(s1 + i);
            for (int i = 0; i < b2; ++i) processCc(s2 + i);
            ccFifo_.finishedRead(b1 + b2);
            if (changed)
                sendChangeMessage();
        }
    }

    // 5. Clear activity flags after editor has had a chance to read them
    txActivity_.store(false);
    rxActivity_.store(false);
}

//==============================================================================
// Refresh sequence
//==============================================================================

void NTPerformProcessor::refreshAllItems()
{
    queue_.clear();
    ccLookup_.clear();

    {
        juce::ScopedLock sl(fwVersionLock_);
        firmwareVersion_ = "querying...";
    }

    for (int i = 0; i < PerformPageModel::kTotalItems; ++i)
    {
        PerformancePageItem empty;
        empty.itemIndex = i;
        model_.setItem(empty);
    }

    loadProgress_.store(0);
    refreshPhase_.store(1);
    sendChangeMessage();

    // First: request firmware version, then chain into perf page items
    SysExQueue::Request req;
    req.message             = DistingNT::buildRequestVersion(sysExId_.load());
    req.expectedResponseCmd = DistingNT::kCmdRespMessage;
    req.matchItemIndex      = -1;

    req.onResponse = [this](const uint8_t* payload, int len)
    {
        const juce::String version = DistingNT::parseMessageResponse(payload, len);
        {
            juce::ScopedLock sl(fwVersionLock_);
            firmwareVersion_ = version.isEmpty() ? "unknown" : version;
        }
        sendChangeMessage();

        juce::MessageManager::callAsync([this]() { startPhase1(); });
    };

    req.onTimeout = [this]()
    {
        {
            juce::ScopedLock sl(fwVersionLock_);
            firmwareVersion_ = "no response";
        }
        sendChangeMessage();
        juce::MessageManager::callAsync([this]() { startPhase1(); });
    };

    queue_.enqueue(std::move(req));
}

void NTPerformProcessor::startPhase1()
{
    enqueueItemRequest(0);
}

void NTPerformProcessor::enqueueItemRequest(int itemIndex)
{
    if (itemIndex >= PerformPageModel::kTotalItems)
    {
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
        juce::MessageManager::callAsync([this, itemIndex]()
        {
            enqueueItemRequest(itemIndex + 1);
        });
    };

    req.onTimeout = [this, itemIndex]()
    {
        loadProgress_.fetch_add(1);
        juce::MessageManager::callAsync([this, itemIndex]()
        {
            enqueueItemRequest(itemIndex + 1);
        });
    };

    queue_.enqueue(std::move(req));

    // Send immediately if direct output is open
    {
        juce::ScopedLock sl(deviceLock_);
        if (midiOutput_)
        {
            juce::MidiMessage outMsg;
            if (queue_.tryDequeueNext(outMsg))
            {
                txActivity_.store(true);
                midiOutput_->sendMessageNow(outMsg);
            }
        }
    }
}

void NTPerformProcessor::startPhase2()
{
    mappingsPending_ = 0;
    for (int i = 0; i < PerformPageModel::kTotalItems; ++i)
        if (model_.getItem(i).enabled)
            ++mappingsPending_;

    if (mappingsPending_ == 0)
    {
        refreshPhase_.store(0);
        updateHostDisplay(ChangeDetails{}.withParameterInfoChanged(true));
        sendChangeMessage();
        return;
    }

    for (int i = 0; i < PerformPageModel::kTotalItems; ++i)
        if (model_.getItem(i).enabled)
            enqueueMappingRequest(i);
}

void NTPerformProcessor::enqueueMappingRequest(int itemIndex)
{
    const auto& item = model_.getItem(itemIndex);

    SysExQueue::Request req;
    req.message             = DistingNT::buildRequestMappings(sysExId_.load(),
                                                               item.slotIndex,
                                                               item.parameterNumber);
    req.expectedResponseCmd = DistingNT::kCmdRequestMappings;
    req.matchItemIndex      = -1;

    req.onResponse = [this, itemIndex](const uint8_t* payload, int len)
    {
        DistingNT::MappingData md;
        if (DistingNT::parseMappingsResponse(payload, len, md) && md.enabled && md.midiCC >= 0)
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
        if (--mappingsPending_ <= 0)
        {
            refreshPhase_.store(0);
            updateHostDisplay(ChangeDetails{}.withParameterInfoChanged(true));
            sendChangeMessage();
        }
    };

    req.onTimeout = [this]()
    {
        if (--mappingsPending_ <= 0)
        {
            refreshPhase_.store(0);
            updateHostDisplay(ChangeDetails{}.withParameterInfoChanged(true));
            sendChangeMessage();
        }
    };

    queue_.enqueue(std::move(req));
}

void NTPerformProcessor::sendParamValue(int slot, int param, int value)
{
    model_.setParamValue(slot, param, value);
    sendChangeMessage();

    SysExQueue::Request req;
    req.message             = DistingNT::buildSetParamValue(sysExId_.load(), slot, param, value);
    req.expectedResponseCmd = 0;
    queue_.enqueue(std::move(req));

    // Send immediately if direct output is open
    {
        juce::ScopedLock sl(deviceLock_);
        if (midiOutput_)
        {
            juce::MidiMessage outMsg;
            if (queue_.tryDequeueNext(outMsg))
            {
                txActivity_.store(true);
                midiOutput_->sendMessageNow(outMsg);
            }
        }
    }
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
    xml->setAttribute("sysExId",       sysExId_.load());
    xml->setAttribute("currentPage",   currentPage_.load());
    xml->setAttribute("midiInputId",   getSelectedMidiInputId());
    xml->setAttribute("midiOutputId",  getSelectedMidiOutputId());
    copyXmlToBinary(*xml, dest);
}

void NTPerformProcessor::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName("NTPerformState"))
    {
        sysExId_.store(xml->getIntAttribute("sysExId", 1));
        currentPage_.store(xml->getIntAttribute("currentPage", 0));

        const auto inId  = xml->getStringAttribute("midiInputId");
        const auto outId = xml->getStringAttribute("midiOutputId");
        if (inId.isNotEmpty() || outId.isNotEmpty())
            openMidiDevices(inId, outId);
        else
            refreshAllItems();
    }
}

//==============================================================================

juce::AudioProcessorEditor* NTPerformProcessor::createEditor()
{
    return new NTPerformEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NTPerformProcessor();
}
