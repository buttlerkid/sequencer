set(DY_NODAL_VERSION 0.1.0)

set(NODAL_ENGINE_SOURCES
    Source/Engine/Bodies.cpp
    Source/Engine/ModalBank.cpp
    Source/Engine/NodalEngine.cpp)

if(DY_BUILD_TESTS)
    add_executable(nodal_tests Tests/NodalTests.cpp ${NODAL_ENGINE_SOURCES})
    target_include_directories(nodal_tests PRIVATE Source)
    add_test(NAME nodal_engine_tests COMMAND nodal_tests)
endif()

if(DY_BUILD_PLUGIN)
    juce_add_plugin(DYNodal
        VERSION                     ${DY_NODAL_VERSION}
        COMPANY_NAME                "dy"
        BUNDLE_ID                   "com.dy.dynodal"
        IS_SYNTH                    FALSE     # audio effect
        NEEDS_MIDI_INPUT            TRUE      # key follow / instrument mode
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        PLUGIN_MANUFACTURER_CODE    Dyse
        PLUGIN_CODE                 Dynd
        FORMATS                     VST3 Standalone
        VST3_CATEGORIES             Fx Filter
        PRODUCT_NAME                "DY Nodal")

    target_sources(DYNodal PRIVATE
        ${NODAL_ENGINE_SOURCES}
        Source/Params.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/UI/NodalLookAndFeel.cpp
        Source/UI/Controls.cpp
        Source/UI/PlateView.cpp)

    target_include_directories(DYNodal PRIVATE Source)
    target_link_libraries(DYNodal PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_opengl)
    dy_plugin_defaults(DYNodal ${DY_NODAL_VERSION})
endif()
