cmake_minimum_required(VERSION 3.22)
project(BAKERY VERSION 1.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

# ---- Pure C++ DSP engine tests (no JUCE) ----
add_executable(bakery_engine_tests tests/test_engine.cpp)
target_include_directories(bakery_engine_tests PRIVATE src/engine tests)
add_test(NAME engine_dsp_sequencer_random_longrun COMMAND bakery_engine_tests)
add_executable(bakery_render_demos tests/render_demos.cpp)
target_include_directories(bakery_render_demos PRIVATE src/engine tests)

option(BAKERY_ENGINE_ONLY "Build dependency-free engine tests only" OFF)
if(BAKERY_ENGINE_ONLY)
  return()
endif()

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
  add_subdirectory(JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Bakery
  VERSION 1.1.0
  COMPANY_NAME koobacorp
  COMPANY_WEBSITE "https://github.com/koobacorp"
  BUNDLE_ID com.koobacorp.bakery
  PLUGIN_MANUFACTURER_CODE Koob
  PLUGIN_CODE Bkry
  FORMATS VST3 AU
  PRODUCT_NAME BAKERY
  DESCRIPTION "BAKERY drum synth"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Drum
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  AU_SANDBOX_SAFE TRUE)
target_sources(Bakery PRIVATE src/plugin/PluginProcessor.cpp src/plugin/PluginEditor.cpp)
target_include_directories(Bakery PUBLIC src/engine src/plugin)
target_compile_definitions(Bakery PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_USE_CAMERA=0 JUCE_USE_FLAC=0 JUCE_USE_OGGVORBIS=0 JUCE_USE_MP3AUDIOFORMAT=0 JUCE_STRICT_REFCOUNTEDPOINTER=1)
target_link_libraries(Bakery PRIVATE juce::juce_audio_utils juce::juce_gui_basics PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
set_target_properties(Bakery PROPERTIES VISIBILITY_INLINES_HIDDEN ON CXX_VISIBILITY_PRESET hidden)
if(APPLE)
  target_link_options(Bakery PUBLIC -Wl,-dead_strip)
endif()

option(BAKERY_BUILD_TOOLS "Build JUCE integration tests and UI screenshots" ON)
if(BAKERY_BUILD_TOOLS)
  juce_add_console_app(bakery_plugin_tests PRODUCT_NAME bakery_plugin_tests)
  target_sources(bakery_plugin_tests PRIVATE tests/plugin_tests.cpp)
  target_include_directories(bakery_plugin_tests PRIVATE src/engine src/plugin $<TARGET_PROPERTY:Bakery,INCLUDE_DIRECTORIES>)
  target_compile_definitions(bakery_plugin_tests PRIVATE $<TARGET_PROPERTY:Bakery,COMPILE_DEFINITIONS>)
  target_link_libraries(bakery_plugin_tests PRIVATE Bakery juce::juce_recommended_config_flags)
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shots)
  add_test(NAME plugin_params_buses_midi_state_presets_editor COMMAND bakery_plugin_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/shots)
endif()
