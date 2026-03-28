#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cstdint>
#include <cstring>

namespace DistingNT {

//==============================================================================
// Constants
//==============================================================================

constexpr uint8_t kSysExStart    = 0xF0;
constexpr uint8_t kSysExEnd      = 0xF7;
constexpr uint8_t kMfr0          = 0x00;
constexpr uint8_t kMfr1          = 0x21;
constexpr uint8_t kMfr2          = 0x27;
constexpr uint8_t kDevicePrefix  = 0x6D;

// Command bytes
constexpr uint8_t kCmdRequestPerfPageItem = 0x57;
constexpr uint8_t kCmdSetPerfPageItem     = 0x58;
constexpr uint8_t kCmdRequestParamValue   = 0x45;
constexpr uint8_t kCmdSetParamValue       = 0x46;
constexpr uint8_t kCmdRequestMappings     = 0x4B;

//==============================================================================
// Encoding / Decoding
//==============================================================================

// Encode a signed integer into 3 SysEx-safe bytes (16-bit range).
// Matches Dart encode16() in sysex_utils.dart exactly.
inline std::array<uint8_t, 3> encode16(int value)
{
    uint32_t v = static_cast<uint32_t>(value) & 0xFFFFu;
    return {
        static_cast<uint8_t>((v >> 14) & 0x03),
        static_cast<uint8_t>((v >>  7) & 0x7F),
        static_cast<uint8_t>( v        & 0x7F)
    };
}

// Decode 3 SysEx-safe bytes into a signed integer (sign-extends at bit 15).
// Matches Dart decode16() in sysex_utils.dart exactly.
inline int decode16(const uint8_t* data)
{
    int v = (static_cast<int>(data[0]) << 14)
          | (static_cast<int>(data[1]) <<  7)
          |  static_cast<int>(data[2]);
    if (v & 0x8000)
        v -= 0x10000;
    return v;
}

//==============================================================================
// Header builder
//==============================================================================

inline std::vector<uint8_t> buildHeader(int sysExId)
{
    return { kSysExStart, kMfr0, kMfr1, kMfr2,
             kDevicePrefix,
             static_cast<uint8_t>(sysExId & 0x7F) };
}

//==============================================================================
// Message builders
//==============================================================================

// SysEx 0x57 — request a single performance page item (0-29)
juce::MidiMessage buildRequestPerfPageItem(int sysExId, int itemIndex);

// SysEx 0x45 — request current value of a parameter
juce::MidiMessage buildRequestParamValue(int sysExId, int slot, int param);

// SysEx 0x46 — set a parameter value (no response from hardware)
juce::MidiMessage buildSetParamValue(int sysExId, int slot, int param, int value);

// SysEx 0x4B — request mapping data for a parameter
juce::MidiMessage buildRequestMappings(int sysExId, int slot, int param);

//==============================================================================
// Parsed message envelope
//==============================================================================

struct ParsedMessage
{
    uint8_t        commandByte  = 0;
    const uint8_t* payload      = nullptr;
    int            payloadLen   = 0;
};

// Validate that msg is a Disting NT SysEx message with the given sysExId.
// On success, fills 'out' and returns true.
bool tryParse(const juce::MidiMessage& msg, int sysExId, ParsedMessage& out);

//==============================================================================
// Response parsers
//==============================================================================

struct PerfPageItemData
{
    int  itemIndex      = 0;
    bool enabled        = false;
    int  slotIndex      = 0;
    int  parameterNumber = 0;
    int  min            = 0;
    int  max            = 0;
    char upperLabel[33] = {};
    char lowerLabel[33] = {};
};

// Parse a 0x57 response payload (after the command byte).
bool parsePerfPageItemResponse(const uint8_t* payload, int len,
                               PerfPageItemData& out);

struct ParamValueData
{
    int slotIndex       = 0;
    int parameterNumber = 0;
    int value           = 0;
};

// Parse a 0x45 response payload.
bool parseParamValueResponse(const uint8_t* payload, int len,
                             ParamValueData& out);

struct MappingData
{
    bool enabled        = false;
    int  midiChannel    = 0;   // 0-15
    int  midiCC         = -1;  // -1 = no CC mapped; 0-127 = CC number
    int  midiMin        = 0;
    int  midiMax        = 127;
};

// Parse a 0x4B response payload. Only extracts the MIDI CC fields needed
// for CC reverse lookup. Based on PackedMappingData v5 in nt_helper.
bool parseMappingsResponse(const uint8_t* payload, int len,
                           MappingData& out);

} // namespace DistingNT
