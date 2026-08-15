cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# Tides - software emulation of an analogue BBD-resonator polyphonic synth.
# (Engine modelled on the Morphor ECHON 6. Product/branding names here are
#  placeholders and can be changed freely.)
# ---------------------------------------------------------------------------
project(Tides VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)

# Vendored JUCE (libs/JUCE).
add_subdirectory(libs/JUCE)

juce_add_plugin(Tides
    PRODUCT_NAME            "Tides"
    COMPANY_NAME            "Joel Eaton"
    BUNDLE_ID               "com.joeleaton.tides"
    PLUGIN_MANUFACTURER_CODE Joel
    PLUGIN_CODE             Tds1
    FORMATS                 AU VST3 Standalone
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Tides uses audio input for the Ext In bus."
    ICON_BIG  "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon-1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon-1024.png"
    COPY_PLUGIN_AFTER_BUILD TRUE)

juce_generate_juce_header(Tides)

target_sources(Tides PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/engine/SynthEngine.cpp
    src/engine/Voice.cpp
    src/dsp/Exciter.cpp
    src/dsp/BBDResonator.cpp
    src/dsp/OutputStage.cpp
    src/mod/ModMatrix.cpp
    src/state/PatchState.cpp
    src/state/FactoryPresets.cpp)

target_include_directories(Tides PRIVATE src)

target_compile_definitions(Tides PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Tides PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
