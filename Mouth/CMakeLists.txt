set(DY_MOUTH_VERSION 0.3.0)

if(DY_BUILD_TESTS)
    add_executable(mouth_tests Tests/MouthTests.cpp Source/Engine/MouthMapper.cpp)
    target_include_directories(mouth_tests PRIVATE Source)
    add_test(NAME mouth_engine_tests COMMAND mouth_tests)
endif()

if(DY_BUILD_PLUGIN AND WIN32)
    # ONNX Runtime runs the face models; its dll ships inside the plugin, renamed so it
    # never meets another onnxruntime.dll a host (or Windows) has loaded.
    FetchContent_Declare(onnxruntime
        URL https://github.com/microsoft/onnxruntime/releases/download/v1.31.1/onnxruntime-win-x64-1.31.1.zip)
    FetchContent_MakeAvailable(onnxruntime)
    set(DY_ORT_DLL "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.dll")

    # MediaPipe Face and Hand Landmarker models (Apache 2.0), converted to ONNX by Models/convert.py
    juce_add_binary_data(DYMouthModels
        NAMESPACE MouthModels
        SOURCES Models/face_detector.onnx Models/face_landmarks_detector.onnx
                Models/hand_detector.onnx Models/hand_landmarks_detector.onnx)

    juce_add_plugin(DYMouth
        VERSION                     ${DY_MOUTH_VERSION}
        COMPANY_NAME                "dy"
        BUNDLE_ID                   "com.dy.dymouth"
        IS_SYNTH                    TRUE      # "Instrument" so Live puts it on a MIDI track
        NEEDS_MIDI_INPUT            TRUE
        NEEDS_MIDI_OUTPUT           TRUE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        PLUGIN_MANUFACTURER_CODE    Dyse
        PLUGIN_CODE                 Dymo
        FORMATS                     VST3 Standalone
        VST3_CATEGORIES             Instrument Tools
        PRODUCT_NAME                "DY Mouth")

    target_sources(DYMouth PRIVATE
        Source/Engine/MouthMapper.cpp
        Source/Engine/FaceTracker.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ../Nodal/Source/UI/NodalLookAndFeel.cpp)     # the suite's brass-on-graphite look

    target_include_directories(DYMouth PRIVATE Source ../Nodal/Source/UI "${onnxruntime_SOURCE_DIR}/include")
    target_compile_definitions(DYMouth PUBLIC JUCE_USE_CAMERA=1)
    target_link_libraries(DYMouth PRIVATE juce::juce_audio_utils juce::juce_video DYMouthModels)
    dy_plugin_defaults(DYMouth ${DY_MOUTH_VERSION})

    # the tracker on a still picture (tests and tuning)
    add_executable(mouth_track_image Tests/TrackImage.cpp Source/Engine/FaceTracker.cpp Source/Engine/MouthMapper.cpp)
    target_include_directories(mouth_track_image PRIVATE Source "${onnxruntime_SOURCE_DIR}/include")

    foreach(fmt VST3 Standalone)
        add_custom_command(TARGET DYMouth_${fmt} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DY_ORT_DLL}"
                    "$<TARGET_FILE_DIR:DYMouth_${fmt}>/DYMouth.onnxruntime.dll")
    endforeach()
endif()
