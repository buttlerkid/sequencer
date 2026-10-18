set(DY_DUB_VERSION 0.2.0)

set(DUB_ENGINE_SOURCES
    Source/Engine/DubEngine.cpp
    Source/Engine/Effects.cpp)

if(DY_BUILD_TESTS)
    add_executable(dub_tests Tests/DubTests.cpp ${DUB_ENGINE_SOURCES})
    target_include_directories(dub_tests PRIVATE Source)
    add_test(NAME dub_engine_tests COMMAND dub_tests)
endif()

if(DY_BUILD_PLUGIN)
    juce_add_plugin(DYDub
        VERSION                     ${DY_DUB_VERSION}
        COMPANY_NAME                "dy"
        BUNDLE_ID                   "com.dy.dydub"
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        PLUGIN_MANUFACTURER_CODE    Dyse
        PLUGIN_CODE                 Dydb
        FORMATS                     VST3 Standalone
        VST3_CATEGORIES             Fx Delay Reverb
        PRODUCT_NAME                "DY Dub")

    target_sources(DYDub PRIVATE
        ${DUB_ENGINE_SOURCES}
        Source/Presets.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/UI/DubLookAndFeel.cpp
        Source/UI/StepStrips.cpp
        Source/UI/StepGrid.cpp
        Source/UI/EnvEditor.cpp
        ../Nodal/Source/UI/NodalLookAndFeel.cpp)     # the suite's fonts

    target_include_directories(DYDub PRIVATE Source Source/UI ../Nodal/Source/UI)
    target_link_libraries(DYDub PRIVATE juce::juce_audio_utils)
    dy_plugin_defaults(DYDub ${DY_DUB_VERSION})
endif()
