set(DY_PENDULUM_VERSION 0.2.0)

if(DY_BUILD_TESTS)
    add_executable(pendulum_tests Tests/PendulumTests.cpp Source/Engine/PendulumEngine.cpp)
    target_include_directories(pendulum_tests PRIVATE Source)
    add_test(NAME pendulum_engine_tests COMMAND pendulum_tests)
endif()

if(DY_BUILD_PLUGIN)
    juce_add_plugin(DYPendulum
        VERSION                     ${DY_PENDULUM_VERSION}
        COMPANY_NAME                "dy"
        BUNDLE_ID                   "com.dy.dypendulum"
        IS_SYNTH                    TRUE      # "Instrument" so Live puts it on a MIDI track
        NEEDS_MIDI_INPUT            TRUE
        NEEDS_MIDI_OUTPUT           TRUE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        PLUGIN_MANUFACTURER_CODE    Dyse
        PLUGIN_CODE                 Dypw
        FORMATS                     VST3 Standalone
        VST3_CATEGORIES             Instrument Tools
        PRODUCT_NAME                "DY Pendulum")

    target_sources(DYPendulum PRIVATE
        Source/Engine/PendulumEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ../Nodal/Source/UI/NodalLookAndFeel.cpp)     # the suite's brass-on-graphite look

    target_include_directories(DYPendulum PRIVATE Source ../Nodal/Source/UI)
    target_link_libraries(DYPendulum PRIVATE juce::juce_audio_utils)
    dy_plugin_defaults(DYPendulum ${DY_PENDULUM_VERSION})
endif()
