set(DY_NODAL_VERSION 0.4.0)

set(NODAL_ENGINE_SOURCES
    Source/Engine/Bodies.cpp
    Source/Engine/ModalBank.cpp
    Source/Engine/NodalEngine.cpp
    Source/Engine/PitchTracker.cpp)

if(DY_BUILD_TESTS)
    add_executable(nodal_tests Tests/NodalTests.cpp ${NODAL_ENGINE_SOURCES})
    target_include_directories(nodal_tests PRIVATE Source)
    add_test(NAME nodal_engine_tests COMMAND nodal_tests)
endif()

if(DY_BUILD_PLUGIN)
    # One code base, two plugins: the effect (audio in, MIDI optional, sidechain) and
    # the instrument (MIDI in, no audio input) that Live lists under Instruments.
    function(dy_nodal_plugin target product code is_synth bundle categories)
        juce_add_plugin(${target}
            VERSION                     ${DY_NODAL_VERSION}
            COMPANY_NAME                "dy"
            BUNDLE_ID                   ${bundle}
            IS_SYNTH                    ${is_synth}
            NEEDS_MIDI_INPUT            TRUE
            NEEDS_MIDI_OUTPUT           FALSE
            IS_MIDI_EFFECT              FALSE
            EDITOR_WANTS_KEYBOARD_FOCUS TRUE
            COPY_PLUGIN_AFTER_BUILD     FALSE
            PLUGIN_MANUFACTURER_CODE    Dyse
            PLUGIN_CODE                 ${code}
            FORMATS                     VST3 Standalone
            VST3_CATEGORIES             ${categories}
            PRODUCT_NAME                "${product}")

        target_sources(${target} PRIVATE
            ${NODAL_ENGINE_SOURCES}
            Source/Params.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp
            Source/UI/NodalLookAndFeel.cpp
            Source/UI/Controls.cpp
            Source/UI/ModPanel.cpp
            Source/UI/PlayPanel.cpp
            Source/UI/PlateView.cpp)

        target_include_directories(${target} PRIVATE Source)
        target_link_libraries(${target} PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_opengl)
        dy_plugin_defaults(${target} ${DY_NODAL_VERSION})
    endfunction()

    dy_nodal_plugin(DYNodal           "DY Nodal"            Dynd FALSE com.dy.dynodal           "Fx;Filter")
    dy_nodal_plugin(DYNodalInstrument "DY Nodal Instrument" Dyni TRUE  com.dy.dynodalinstrument "Instrument")
endif()
