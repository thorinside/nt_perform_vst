cmake_minimum_required(VERSION 3.22)
project(NTPerform VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Code signing — override on command line for release builds:
#   -DCODE_SIGN_IDENTITY=4C9BC6484BFCD52CBB79C2C84583C40051808DA1
#
# Available identities:
#   Apple Development (local testing):
#     6EDA7EFA231C691B7862F1B7AAA134B31920710C
#   Developer ID Application (distribution):
#     4C9BC6484BFCD52CBB79C2C84583C40051808DA1
set(CODE_SIGN_IDENTITY "6EDA7EFA231C691B7862F1B7AAA134B31920710C"
    CACHE STRING "macOS code signing identity (SHA1 hash or name)")
set(DEVELOPMENT_TEAM "KN424RZG26"
    CACHE STRING "Apple Development Team ID")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.7
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(NTPerform
    PLUGIN_MANUFACTURER_CODE Nscn
    PLUGIN_CODE              NtPf
    FORMATS                  VST3 AU
    IS_MIDI_EFFECT           TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        TRUE
    PLUGIN_NAME              "NT Perform"
    COMPANY_NAME             "Nosuch"
    PLUGIN_MANUFACTURER_URL  ""
    VST3_CATEGORIES          "Fx|Midi"
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(NTPerform PRIVATE
    src/PluginProcessor.cpp
    src/PerfPageParam.cpp
    src/PluginEditor.cpp
    src/protocol/DistingNTProtocol.cpp
    src/protocol/SysExQueue.cpp
    src/protocol/CcReverseLookup.cpp
    src/model/PerformPageModel.cpp
    src/ui/PotComponent.cpp
    src/ui/EncoderComponent.cpp
    src/ui/PageSelectorComponent.cpp
    src/ui/StatusBarComponent.cpp
)

target_include_directories(NTPerform PRIVATE src)

target_compile_features(NTPerform PRIVATE cxx_std_17)

target_compile_definitions(NTPerform PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NTPerform PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_devices
    juce::juce_gui_basics
    juce::juce_events
    juce::juce_audio_utils
)

# ── macOS code signing ──────────────────────────────────────────────────────

if(APPLE)
    # Xcode generator: set attributes so Xcode signs automatically
    set_target_properties(NTPerform_VST3 NTPerform_AU PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY      "${CODE_SIGN_IDENTITY}"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM        "${DEVELOPMENT_TEAM}"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE         "Manual"
        XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS   "--timestamp --options=runtime"
    )

    # Makefile/Ninja generator: sign with a post-build codesign command
    if(NOT CMAKE_GENERATOR MATCHES "Xcode")
        add_custom_command(TARGET NTPerform_VST3 POST_BUILD
            COMMAND codesign --force --sign ${CODE_SIGN_IDENTITY}
                    --timestamp --options runtime
                    "$<TARGET_BUNDLE_DIR:NTPerform_VST3>"
            COMMENT "Signing NTPerform.vst3"
            VERBATIM
        )
        add_custom_command(TARGET NTPerform_AU POST_BUILD
            COMMAND codesign --force --sign ${CODE_SIGN_IDENTITY}
                    --timestamp --options runtime
                    "$<TARGET_BUNDLE_DIR:NTPerform_AU>"
            COMMENT "Signing NTPerform.component"
            VERBATIM
        )
    endif()
endif()
