cmake_minimum_required(VERSION 3.22)
project(DYSuite LANGUAGES C CXX)

# DY plugin suite: every module lives in its own folder and shares one JUCE.
#   Sequencer/  DY Sequencer - 16-track Euclidean MIDI sequencer (instrument, MIDI out)
#   Nodal/      DY Nodal     - Chladni plate resonator (audio effect)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Static MSVC runtime: the plugins then have no dependency on the VC++ redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(DY_BUILD_PLUGIN "Build the VST3 / Standalone plugins (fetches JUCE)" ON)
option(DY_BUILD_TESTS  "Build the engine unit tests (no JUCE needed)"        ON)

if(DY_BUILD_TESTS)
    enable_testing()
endif()

if(DY_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(JUCE
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.2.zip)
    FetchContent_MakeAvailable(JUCE)
endif()

# Common compile settings for every plugin target.
function(dy_plugin_defaults target version)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        DY_VERSION_STRING="${version}")
    target_link_libraries(${target}
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

add_subdirectory(Sequencer)
add_subdirectory(Nodal)
