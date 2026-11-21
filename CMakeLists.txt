cmake_minimum_required(VERSION 3.22)
project(IceWave VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT JUCE_DIR)
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
else()
  add_subdirectory(${JUCE_DIR} juce)
endif()

juce_add_plugin(IceWave
  COMPANY_NAME "Thibaut Basely"
  BUNDLE_ID com.thibautbasely.icewave
  PLUGIN_MANUFACTURER_CODE Tbas
  PLUGIN_CODE IceW
  FORMATS VST3
  PRODUCT_NAME "Ice Wave"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Synth)

target_sources(IceWave PRIVATE
  src/Wavetables.cpp
  src/SynthVoice.cpp
  src/PluginProcessor.cpp
  src/PluginEditor.cpp)

target_compile_definitions(IceWave PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(IceWave
  PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
