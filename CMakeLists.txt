cmake_minimum_required(VERSION 3.22)

project(WishcraftMasteringLimiter VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE lives outside this project folder, so we point add_subdirectory at it
# directly and give it an explicit binary dir (required for out-of-tree sources).
add_subdirectory("/Users/glennburgos/JUCE" JUCE)

juce_add_plugin(WishcraftMasteringLimiter
    COMPANY_NAME "Wishcraft"
    PLUGIN_MANUFACTURER_CODE Wish
    PLUGIN_CODE Wml1
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Wishcraft Mastering Limiter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(WishcraftMasteringLimiter
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(WishcraftMasteringLimiter
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(WishcraftMasteringLimiter
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
