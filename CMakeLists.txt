cmake_minimum_required(VERSION 3.22)
project(DYSequencer VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Static MSVC runtime: the plugin then has no dependency on the VC++ redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(DY_BUILD_PLUGIN "Build the VST3 / Standalone plugin (fetches JUCE)" ON)
option(DY_BUILD_TESTS  "Build the engine unit tests (no JUCE needed)"       ON)

# ---------------------------------------------------------------- engine tests
if(DY_BUILD_TESTS)
    enable_testing()
    add_executable(engine_tests
        Tests/EngineTests.cpp
        Source/Engine/Sequencer.cpp)
    target_include_directories(engine_tests PRIVATE Source)
    add_test(NAME engine_tests COMMAND engine_tests)
endif()

# ---------------------------------------------------------------- plugin
if(DY_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(JUCE
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.2.zip)
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(DYSequencer
        COMPANY_NAME                "dy"
        BUNDLE_ID                   "com.dy.dysequencer"
        IS_SYNTH                    TRUE      # "Instrument" so Live puts it on a MIDI track
        NEEDS_MIDI_INPUT            TRUE
        NEEDS_MIDI_OUTPUT           TRUE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD     FALSE     # see scripts/install.ps1
        PLUGIN_MANUFACTURER_CODE    Dyse
        PLUGIN_CODE                 Dysq
        FORMATS                     VST3 Standalone
        VST3_CATEGORIES             Instrument Tools
        PRODUCT_NAME                "DY Sequencer")

    target_sources(DYSequencer PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Engine/Sequencer.cpp
        Source/Params/Parameters.cpp
        Source/UI/Theme.cpp
        Source/UI/HeaderBar.cpp
        Source/UI/ChainStrip.cpp
        Source/UI/OverviewGrid.cpp
        Source/UI/PadsPanel.cpp
        Source/UI/EditPanel.cpp
        Source/UI/GlobalPanel.cpp
        Source/UI/LaneEditor.cpp
        Source/UI/StatusBar.cpp)

    target_include_directories(DYSequencer PRIVATE Source)

    target_compile_definitions(DYSequencer PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        DY_VERSION_STRING="${PROJECT_VERSION}")

    target_link_libraries(DYSequencer
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
