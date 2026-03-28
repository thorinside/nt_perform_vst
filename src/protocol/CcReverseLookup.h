#pragma once
#include <unordered_map>

// Maps (midiChannel << 8) | midiCC → CcTarget.
// Built after all perf page items and their 0x4B mappings are loaded.
class CcReverseLookup
{
public:
    struct CcTarget
    {
        int itemIndex  = 0;
        int slotIndex  = 0;
        int paramNumber = 0;
        int paramMin   = 0;
        int paramMax   = 0;
    };

    void add(int midiChannel, int midiCC, const CcTarget& target);
    void clear();

    // Returns nullptr if no mapping exists for (channel, cc).
    const CcTarget* find(int midiChannel, int midiCC) const;

    // Linear interpolation: paramMin + (cc / 127.0f) * (paramMax - paramMin)
    static int convertCcToValue(const CcTarget& t, int ccValue);

private:
    std::unordered_map<int, CcTarget> table_;
};
