set(DY_NODAL_VERSION 0.0.1)

if(DY_BUILD_PLUGIN)
    juce_add_plugin(DYNodal
        VERSION                     ${DY_NODAL_VERSION}
        COMPANY_NAME                "dy"
        BUNDLE_ID                   "com.dy.dynodal"
        IS_SYNTH                    FALSE     # audio effect
        NEEDS_MIDI_INPUT            TRUE      # instrument mode / key follow later
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
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_include_directories(DYNodal PRIVATE Source)
    target_link_libraries(DYNodal PRIVATE juce::juce_audio_utils juce::juce_dsp)
    dy_plugin_defaults(DYNodal ${DY_NODAL_VERSION})
endif()
