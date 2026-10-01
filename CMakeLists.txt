cmake_minimum_required(VERSION 3.22)

project(MIDI2Svara VERSION 0.14.0)

set(PROJECT_NAME "MIDI2Svara")
set(CURRENT_VERSION 0.14.0)
set(CURRENT_PLUGIN_CODE Fp2a)

message(STATUS "Plugin Name: ${PROJECT_NAME}")
message(STATUS "Version: ${CURRENT_VERSION}")
message(STATUS "Plugin Code: ${CURRENT_PLUGIN_CODE}")

# platform specific settings
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version" FORCE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
endif()

add_subdirectory(libs/JUCE)

# ══════════════════════════════════════════════════
# BinaryData: embed raga JSON resources
# ══════════════════════════════════════════════════

set(RAGA_NAMES abhogi begada kalyani mohanam sahana saveri sri)

set(RAGA_BINARY_DATA_SOURCES "")
foreach(RAGA ${RAGA_NAMES})
    list(APPEND RAGA_BINARY_DATA_SOURCES
        "${CMAKE_SOURCE_DIR}/res/${RAGA}.json")
endforeach()

juce_add_binary_data(RagaData
    NAMESPACE RagaData
    SOURCES ${RAGA_BINARY_DATA_SOURCES})

target_compile_features(RagaData PRIVATE cxx_std_17)

# ══════════════════════════════════════════════════
# Main target: Instrument plugin (VST3 + AU + Standalone)
# ══════════════════════════════════════════════════

set(FORMATS Standalone VST3 AU)

juce_add_plugin(FluidPitch2
    VERSION ${CURRENT_VERSION}
    COMPANY_NAME "Pitch Innovations"
    BUNDLE_ID "com.pitchinnovations.FluidPitch2"
    PLUGIN_MANUFACTURER_CODE PiIn
    PLUGIN_CODE ${CURRENT_PLUGIN_CODE}
    FORMATS ${FORMATS}
    PRODUCT_NAME "${PROJECT_NAME}"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_generate_juce_header(FluidPitch2)

target_sources(FluidPitch2
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/dsp/PitchCurve.cpp
        src/midi/RecordingBuffer.cpp
        src/io/PresetManager.cpp
        src/dsp/CarnaticEngine.h
        src/dsp/CarnaticEngine.cpp
        src/midi/MidiEngine.cpp
        src/ui/PianoRoll.cpp
        src/ui/Keyboard.cpp
        src/ui/TimeRuler.cpp
        src/ui/EditorToolbar.cpp
        src/ui/SettingsPanel.cpp
        src/ui/NoteComponent.cpp
        src/ui/CurveDrawing.cpp
        src/ui/LookAndFeel.cpp
        src/ui/VelocityLane.cpp)

target_compile_features(FluidPitch2 PRIVATE cxx_std_17)

target_include_directories(FluidPitch2 PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/rapidjson/include)

target_link_libraries(FluidPitch2
    PRIVATE
        RagaData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

target_compile_definitions(FluidPitch2
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

# ══════════════════════════════════════════════════
# Tests
# ══════════════════════════════════════════════════

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/googletest EXCLUDE_FROM_ALL)

juce_add_console_app(CarnaticEngineTests PRODUCT_NAME "CarnaticEngineTests")
juce_generate_juce_header(CarnaticEngineTests)

target_sources(CarnaticEngineTests
    PRIVATE
        tests/CarnaticEngineTests.cpp
        src/dsp/CarnaticEngine.cpp
        src/dsp/PitchCurve.cpp)

target_compile_features(CarnaticEngineTests PRIVATE cxx_std_17)

target_include_directories(CarnaticEngineTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/rapidjson/include)

target_link_libraries(CarnaticEngineTests
    PRIVATE
        RagaData
        GTest::gtest
        GTest::gtest_main
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

target_compile_definitions(CarnaticEngineTests
    PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

enable_testing()
include(GoogleTest)
gtest_discover_tests(CarnaticEngineTests
    DISCOVERY_TIMEOUT 60
    PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
