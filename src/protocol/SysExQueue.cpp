#include "SysExQueue.h"

void SysExQueue::enqueue(Request req)
{
    juce::ScopedLock sl(lock_);
    queue_.push_back(std::move(req));
}

bool SysExQueue::tryDequeueNext(juce::MidiMessage& out)
{
    juce::ScopedLock sl(lock_);

    if (state_ == State::WaitingForResponse)
        return false;

    if (queue_.empty())
        return false;

    current_     = std::move(queue_.front());
    queue_.pop_front();
    retryCount_  = 0;
    sentTimeMs_  = juce::Time::currentTimeMillis();

    out = current_.message;

    if (current_.expectedResponseCmd == 0)
    {
        // Fire-and-forget: stay Idle so we can immediately send the next one
        state_ = State::Idle;
    }
    else
    {
        state_ = State::WaitingForResponse;
    }

    return true;
}

bool SysExQueue::handleIncoming(uint8_t commandByte,
                                const uint8_t* payload, int payloadLen)
{
    juce::ScopedLock sl(lock_);

    if (state_ != State::WaitingForResponse)
        return false;
    if (commandByte != current_.expectedResponseCmd)
        return false;

    // For 0x57, match by item index in payload[1]
    if (current_.matchItemIndex >= 0)
    {
        if (payloadLen < 2)
            return false;
        if (payload[1] != static_cast<uint8_t>(current_.matchItemIndex))
            return false;
    }

    // Dispatch response callback (called under lock — keep it fast)
    if (current_.onResponse)
        current_.onResponse(payload, payloadLen);

    state_ = State::Idle;
    return true;
}

void SysExQueue::tick()
{
    juce::ScopedLock sl(lock_);

    if (state_ != State::WaitingForResponse)
        return;

    const int64_t now = juce::Time::currentTimeMillis();
    if (now - sentTimeMs_ < kTimeoutMs)
        return;

    // Timed out
    if (retryCount_ < kMaxRetries)
    {
        ++retryCount_;
        sentTimeMs_ = now;
        // Re-queue the current request at the front for retry
        queue_.push_front(current_);
        state_ = State::Idle;
    }
    else
    {
        // Exhausted retries
        if (current_.onTimeout)
            current_.onTimeout();
        state_ = State::Idle;
    }
}

void SysExQueue::clear()
{
    juce::ScopedLock sl(lock_);
    queue_.clear();
    state_       = State::Idle;
    retryCount_  = 0;
}

int SysExQueue::pendingCount() const
{
    juce::ScopedLock sl(lock_);
    return static_cast<int>(queue_.size())
         + (state_ == State::WaitingForResponse ? 1 : 0);
}
