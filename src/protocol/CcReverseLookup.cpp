#include "CcReverseLookup.h"
#include <algorithm>

void CcReverseLookup::add(int midiChannel, int midiCC, const CcTarget& target)
{
    table_[(midiChannel << 8) | (midiCC & 0xFF)] = target;
}

void CcReverseLookup::clear()
{
    table_.clear();
}

const CcReverseLookup::CcTarget* CcReverseLookup::find(int midiChannel,
                                                        int midiCC) const
{
    auto it = table_.find((midiChannel << 8) | (midiCC & 0xFF));
    if (it == table_.end())
        return nullptr;
    return &it->second;
}

int CcReverseLookup::convertCcToValue(const CcTarget& t, int ccValue)
{
    if (t.paramMax <= t.paramMin)
        return t.paramMin;
    const float fraction = static_cast<float>(ccValue) / 127.0f;
    const float result   = t.paramMin + fraction * (t.paramMax - t.paramMin);
    return static_cast<int>(result + 0.5f);
}
