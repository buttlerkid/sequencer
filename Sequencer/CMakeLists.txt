set(DY_SEQUENCER_VERSION 0.4.0)

if(DY_BUILD_TESTS)
    add_executable(engine_tests
        Tests/EngineTests.cpp
        Source/Engine/Sequencer.cpp)
    target_include_directories(engine_tests PRIVATE Source)
    add_test(NAME sequencer_engine_tests COMMAND engine_tests)
endif()

if(DY_BUILD_PLUGIN)
    juce_add_plugin(DYSequencer
        VERSION                     ${DY_SEQUENCER_VERSION}
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
    target_link_libraries(DYSequencer PRIVATE juce::juce_audio_utils)
    dy_plugin_defaults(DYSequencer ${DY_SEQUENCER_VERSION})
endif()
