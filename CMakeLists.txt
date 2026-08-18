cmake_minimum_required(VERSION 3.22)

project(WishcraftMasteringLimiter VERSION 1.0.0)

# Xcode only builds for the machine's own architecture unless told otherwise -- without
# this, a build done on an Apple Silicon Mac produces an arm64-only binary that won't
# launch at all on an Intel Mac. Setting both archs makes it a universal binary.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build a universal (Apple Silicon + Intel) binary" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pinned to the JUCE version this project was built against (9.0.1). Locally on the
# original dev machine, reuse the existing checkout at /Users/glennburgos/JUCE instead
# of re-cloning -- FETCHCONTENT_SOURCE_DIR_<name> is FetchContent's own override
# mechanism for exactly this. CI (GitHub Actions sets the CI env var, as does most
# other CI) and any other machine without that exact path just fetch JUCE fresh, so
# this stays portable without needing a git submodule.
include(FetchContent)
if(EXISTS "/Users/glennburgos/JUCE/CMakeLists.txt" AND NOT DEFINED ENV{CI})
    set(FETCHCONTENT_SOURCE_DIR_JUCE "/Users/glennburgos/JUCE")
endif()
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(WishcraftMasteringLimiter
    COMPANY_NAME "Wishcraft"
    PLUGIN_MANUFACTURER_CODE Wish
    PLUGIN_CODE Wml1
    FORMATS AU VST3
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
