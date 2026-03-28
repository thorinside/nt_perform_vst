#include "DistingNTProtocol.h"
#include <cstring>

namespace DistingNT {

//==============================================================================
// Message builders
//==============================================================================

juce::MidiMessage buildRequestPerfPageItem(int sysExId, int itemIndex)
{
    auto hdr = buildHeader(sysExId);
    std::vector<uint8_t> bytes = hdr;
    bytes.push_back(kCmdRequestPerfPageItem);
    bytes.push_back(static_cast<uint8_t>(itemIndex & 0x7F));
    bytes.push_back(kSysExEnd);
    return juce::MidiMessage(bytes.data(), static_cast<int>(bytes.size()));
}

juce::MidiMessage buildRequestParamValue(int sysExId, int slot, int param)
{
    auto hdr = buildHeader(sysExId);
    auto paramBytes = encode16(param);
    std::vector<uint8_t> bytes = hdr;
    bytes.push_back(kCmdRequestParamValue);
    bytes.push_back(static_cast<uint8_t>(slot & 0x7F));
    bytes.insert(bytes.end(), paramBytes.begin(), paramBytes.end());
    bytes.push_back(kSysExEnd);
    return juce::MidiMessage(bytes.data(), static_cast<int>(bytes.size()));
}

juce::MidiMessage buildSetParamValue(int sysExId, int slot, int param, int value)
{
    auto hdr = buildHeader(sysExId);
    auto paramBytes = encode16(param);
    auto valueBytes = encode16(value);
    std::vector<uint8_t> bytes = hdr;
    bytes.push_back(kCmdSetParamValue);
    bytes.push_back(static_cast<uint8_t>(slot & 0x7F));
    bytes.insert(bytes.end(), paramBytes.begin(), paramBytes.end());
    bytes.insert(bytes.end(), valueBytes.begin(), valueBytes.end());
    bytes.push_back(kSysExEnd);
    return juce::MidiMessage(bytes.data(), static_cast<int>(bytes.size()));
}

juce::MidiMessage buildRequestVersion(int sysExId)
{
    auto hdr = buildHeader(sysExId);
    std::vector<uint8_t> bytes = hdr;
    bytes.push_back(kCmdRequestVersion);
    bytes.push_back(kSysExEnd);
    return juce::MidiMessage(bytes.data(), static_cast<int>(bytes.size()));
}

juce::MidiMessage buildRequestMappings(int sysExId, int slot, int param)
{
    auto hdr = buildHeader(sysExId);
    auto paramBytes = encode16(param);
    std::vector<uint8_t> bytes = hdr;
    bytes.push_back(kCmdRequestMappings);
    bytes.push_back(static_cast<uint8_t>(slot & 0x7F));
    bytes.insert(bytes.end(), paramBytes.begin(), paramBytes.end());
    bytes.push_back(kSysExEnd);
    return juce::MidiMessage(bytes.data(), static_cast<int>(bytes.size()));
}

//==============================================================================
// Parsing
//==============================================================================

// SysEx data layout (after JUCE strips the leading 0xF0):
// [0]=0x00 [1]=0x21 [2]=0x27 [3]=0x6D [4]=sysExId [5]=cmd [6..n]=payload
bool tryParse(const juce::MidiMessage& msg, int sysExId, ParsedMessage& out)
{
    if (!msg.isSysEx())
        return false;

    const uint8_t* d  = msg.getSysExData();
    const int       sz = msg.getSysExDataSize();

    if (sz < 6)
        return false;
    if (d[0] != kMfr0 || d[1] != kMfr1 || d[2] != kMfr2)
        return false;
    if (d[3] != kDevicePrefix)
        return false;
    if (d[4] != static_cast<uint8_t>(sysExId & 0x7F))
        return false;

    out.commandByte = d[5];
    out.payload     = d + 6;
    out.payloadLen  = sz - 6;
    return true;
}

//==============================================================================
// Response parsers
//==============================================================================

// 0x57 response payload layout (payload[0] onwards):
// [0]=version(1)  [1]=itemIndex  [2]=flags
// if flags & 1:
//   [3]=slot  [4..6]=param(3B)  [7..9]=min(3B)  [10..12]=max(3B)
//   [13..]=upperLabel\0 lowerLabel\0
bool parsePerfPageItemResponse(const uint8_t* payload, int len,
                               PerfPageItemData& out)
{
    if (len < 3)
        return false;

    out.itemIndex = payload[1];
    const bool enabled = (payload[2] & 1) != 0;
    out.enabled = enabled;

    if (!enabled)
        return true;

    if (len < 13)
        return false;

    out.slotIndex       = payload[3];
    out.parameterNumber = decode16(payload + 4);
    out.min             = decode16(payload + 7);
    out.max             = decode16(payload + 10);

    // Read null-terminated strings starting at offset 13
    int offset = 13;
    int i = 0;
    while (offset < len && payload[offset] != 0 && i < 32)
        out.upperLabel[i++] = static_cast<char>(payload[offset++]);
    out.upperLabel[i] = '\0';
    if (offset < len) ++offset; // skip null

    i = 0;
    while (offset < len && payload[offset] != 0 && i < 32)
        out.lowerLabel[i++] = static_cast<char>(payload[offset++]);
    out.lowerLabel[i] = '\0';

    return true;
}

// 0x45 response payload layout:
// [0]=slot  [1..3]=param(3B)  [4..6]=value(3B, with flag bits in byte[4])
bool parseParamValueResponse(const uint8_t* payload, int len,
                             ParamValueData& out)
{
    if (len < 7)
        return false;

    out.slotIndex       = payload[0];
    out.parameterNumber = decode16(payload + 1);

    // Mask out flag bits (bits 2-6 of payload[4]) — keep only bits 0-1 (value bits 14-15)
    uint8_t maskedByte0 = payload[4] & 0x03;
    uint8_t valueBuf[3] = { maskedByte0, payload[5], payload[6] };
    out.value = decode16(valueBuf);

    return true;
}

// 0x4B response payload layout:
// [0]=slot  [1..3]=param(3B)  [4]=version  [5..]=PackedMappingData bytes
//
// PackedMappingData layout (v4+ which is the common case):
//   CV:   [source][cvInput][cvFlags][volts][delta_h][delta_m][delta_l]   = 7 bytes
//   MIDI: [midiCC][midiFlags][midiFlags2][midiMin_3B][midiMax_3B]        = 9 bytes
//   I2C:  [i2cCC][i2cHighBit][i2cFlags][i2cMin_3B][i2cMax_3B]           = 9 bytes
//   Perf: [perfPageIndex]  (version 5 only)                              = 1 byte
bool parseMappingsResponse(const uint8_t* payload, int len,
                           MappingData& out)
{
    // Need at least: slot(1) + param(3) + version(1) + cv(7) + midi(9) = 21
    if (len < 21)
        return false;

    // payload[0] = slot, payload[1..3] = param (not needed here)
    const int version = payload[4];
    const uint8_t* d  = payload + 5; // start of PackedMappingData
    const int       dLen = len - 5;

    // Version 1: no 'source' byte in CV, shorter MIDI section
    // Version 4+: source byte present; version 2+: midiFlags2 present
    int cvOffset = 0;
    if (version >= 4)
        cvOffset = 1; // skip 'source' byte

    // cvInput, cvFlags, volts, delta(3) — we don't need these
    // CV section starts at d[cvOffset]
    int midiStart;
    if (version >= 4)
        midiStart = 7; // source(1) + cvInput(1) + cvFlags(1) + volts(1) + delta(3) = 7
    else
        midiStart = 6; // no source byte

    if (dLen < midiStart + 9)
        return false;

    const uint8_t* midi = d + midiStart;
    uint8_t midiCC    = midi[0];
    uint8_t midiFlags = midi[1];

    // Aftertouch: bit 2 of midiFlags
    if (midiFlags & 4)
        midiCC = 128;

    out.midiChannel  = (midiFlags >> 3) & 0x0F;
    out.enabled      = (midiFlags & 1) != 0;
    out.midiCC       = out.enabled ? static_cast<int>(midiCC) : -1;

    if (version >= 2)
    {
        // midiFlags2 at midi[2], midiMin at midi[3..5], midiMax at midi[6..8]
        if (dLen < midiStart + 9)
            return false;
        out.midiMin = decode16(midi + 3);
        out.midiMax = decode16(midi + 6);
    }
    else
    {
        // v1: midiMin at midi[2..4], midiMax at midi[5..7]
        if (dLen < midiStart + 8)
            return false;
        out.midiMin = decode16(midi + 2);
        out.midiMax = decode16(midi + 5);
    }

    return true;
}

juce::String parseMessageResponse(const uint8_t* payload, int len)
{
    if (len < 1)
        return {};
    // Read first null-terminated ASCII string
    int i = 0;
    juce::String result;
    while (i < len && payload[i] != 0)
        result += static_cast<char>(payload[i++]);
    return result;
}

} // namespace DistingNT
