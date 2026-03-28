#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <deque>
#include <functional>

// Sequential SysEx request/response state machine.
// One request is in flight at a time. Responses are matched by command byte
// (and optionally by an item index for 0x57 responses).
// Timeout / retry logic mirrors the Dart DistingMessageScheduler.
class SysExQueue
{
public:
    struct Request
    {
        juce::MidiMessage message;
        uint8_t  expectedResponseCmd = 0; // 0 = fire-and-forget
        int      matchItemIndex      = -1; // -1 = match any; used for 0x57
        std::function<void(const uint8_t* payload, int len)> onResponse;
        std::function<void()> onTimeout;
    };

    SysExQueue() = default;

    // Thread-safe: enqueue from any thread.
    void enqueue(Request req);

    // Called from processBlock (audio thread): dequeues the next pending
    // message if the queue is idle. Returns true and fills 'out' if a
    // message should be sent this block.
    bool tryDequeueNext(juce::MidiMessage& out);

    // Called from processBlock when a matching SysEx response arrives.
    // Returns true if the message was consumed.
    bool handleIncoming(uint8_t commandByte, const uint8_t* payload, int payloadLen);

    // Called from a Timer on the message thread (~50ms).
    void tick();

    void clear();

    int pendingCount() const;

private:
    static constexpr int kTimeoutMs  = 800;
    static constexpr int kMaxRetries = 3;

    enum class State { Idle, WaitingForResponse };

    State state_ = State::Idle;
    Request current_;
    int64_t sentTimeMs_  = 0;
    int     retryCount_  = 0;

    std::deque<Request> queue_;
    mutable juce::CriticalSection lock_;

    void sendCurrent();          // advances queue_, transitions to WaitingForResponse
    void advance();              // moves to Idle, tries next in queue_
};
