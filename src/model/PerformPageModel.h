#pragma once
#include <juce_events/juce_events.h>
#include <array>
#include <unordered_map>
#include <functional>

struct PerformancePageItem
{
    int  itemIndex       = 0;
    bool enabled         = false;
    int  slotIndex       = 0;
    int  parameterNumber = 0;
    int  min             = 0;
    int  max             = 0;
    char upperLabel[33]  = {};
    char lowerLabel[33]  = {};
};

// Holds all 30 performance page items and their current parameter values.
// Thread-safe via its own CriticalSection.
class PerformPageModel
{
public:
    static constexpr int kTotalItems   = 30;
    static constexpr int kItemsPerPage = 3;
    static constexpr int kTotalPages   = 10;

    // Store an item received from a 0x57 response.
    void setItem(const PerformancePageItem& item);

    // Update a cached parameter value (from CC or 0x45 response).
    void setParamValue(int slot, int param, int value);

    // Get the cached parameter value (returns 0 if unknown).
    int getParamValue(int slot, int param) const;

    // Get a specific item.
    const PerformancePageItem& getItem(int index) const;

    // Fill fraction [0,1] for the item at 'index'. Returns -1.0f if not
    // computable (not loaded, disabled, or zero-range).
    float getFillFraction(int index) const;

    // Number of items received so far.
    int loadedCount() const;
    bool isFullyLoaded() const { return loadedCount() == kTotalItems; }

    // Called whenever setItem or setParamValue changes state.
    std::function<void()> onChange;

private:
    std::array<PerformancePageItem, kTotalItems> items_;
    std::array<bool, kTotalItems>                received_{};
    std::unordered_map<int, int>                 paramValues_; // (slot<<16|param) → value

    mutable juce::CriticalSection lock_;

    static int makeKey(int slot, int param)
    {
        return (slot << 16) | (param & 0xFFFF);
    }
};
