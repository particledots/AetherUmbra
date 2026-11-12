cmake_minimum_required(VERSION 3.22)
project(AetherUmbra VERSION 1.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  # Universal (Apple silicon + Intel) is set by the CI workflow; macOS 11+.
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

# The Oneiroi engine: the firmware copy (third_party/oneiroi-firmware), the OwlProgram library files it is written
# against (third_party/owlprogram) and AetherUmbra's own stand-ins for what OpenWare provides on the hardware
# (src/engine). No JUCE in here, so it builds and tests on any machine (tools/engine_test).
add_library(aether_engine STATIC
  src/engine/AetherEngine.cpp
  src/engine/AetherStream.cpp
  src/engine/shim/owl_runtime.cpp
  third_party/owlprogram/FloatArray.cpp
  third_party/owlprogram/ComplexFloatArray.cpp
  third_party/owlprogram/SimpleValue.cpp
  third_party/owlprogram/basicmaths.c
  third_party/owlprogram/fastlog.c
  third_party/owlprogram/fastpow.c)
target_include_directories(aether_engine
  PUBLIC src/engine
  PRIVATE src/engine/shim third_party/owlprogram third_party/oneiroi-firmware)
set_target_properties(aether_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
# OwlProgram's random number function is global; AetherUmbra provides one per thread instead (src/engine/shim/owl_runtime.cpp).
set_source_files_properties(third_party/owlprogram/basicmaths.c PROPERTIES COMPILE_DEFINITIONS "randf=owlprogram_unused_randf")
# fast_powf(0, y) turns -infinity into an integer (undefined in C, the same result as on the module's ARM chip in practice).
# The sanitizer builds of the engine test would stop on it, so that one check is off for that one file.
set_source_files_properties(third_party/owlprogram/fastpow.c PROPERTIES COMPILE_OPTIONS "-fno-sanitize=float-cast-overflow")
# macOS has no exp10f (glibc has); the OwlProgram library uses it in its non-ARM fallback. A small header supplies it.
if(APPLE)
  target_compile_options(aether_engine PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/shim/apple_compat.h)
endif()
# Third-party code: no warnings from our flags (the firmware has some harmless ones).
target_compile_options(aether_engine PRIVATE -w -fvisibility=hidden -fvisibility-inlines-hidden)

# Engine test (not part of the download): runs test signals through every source and effect without a host and writes WAV files.
option(AETHER_BUILD_ENGINE_TEST "Build the engine test program" ON)
if(AETHER_BUILD_ENGINE_TEST)
  add_executable(aether_engine_test tools/engine_test/engine_test.cpp)
  target_link_libraries(aether_engine_test PRIVATE aether_engine)
  target_compile_options(aether_engine_test PRIVATE -O2)
endif()

option(AETHER_BUILD_PLUGIN "Build the AU / VST3 plugin (downloads JUCE)" ON)
if(AETHER_BUILD_PLUGIN)
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)

  # Plugin formats: AU (macOS only, the priority) + VST3.
  set(FORMATS VST3)
  if(APPLE)
    list(APPEND FORMATS AU)
  endif()
  # For trying the window without a host (not part of the download): -DAETHER_EXTRA_FORMATS=Standalone
  set(AETHER_EXTRA_FORMATS "" CACHE STRING "Extra plugin formats for local testing")
  list(APPEND FORMATS ${AETHER_EXTRA_FORMATS})

  # AU Instrument (aumu): the Milestone 0 test showed it gets notes AND audio (audio through Logic's Side Chain menu).
  juce_add_plugin(AetherUmbra
    COMPANY_NAME "particledots"
    PLUGIN_MANUFACTURER_CODE Ptdt
    PLUGIN_CODE Aeth
    FORMATS ${FORMATS}
    PRODUCT_NAME "AetherUmbra"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

  target_sources(AetherUmbra PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

  target_include_directories(AetherUmbra PRIVATE src)

  target_compile_definitions(AetherUmbra PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    AETHER_VERSION_STRING="${PROJECT_VERSION}")

  target_link_libraries(AetherUmbra PRIVATE
    aether_engine
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

  # Plugin test (not part of the download): runs the plugin code without a host.
  #   cmake -S . -B build -DAETHER_BUILD_PLUGIN_TEST=ON && cmake --build build --target AetherUmbra_PluginTest
  option(AETHER_BUILD_PLUGIN_TEST "Build the plugin test program" OFF)
  if(AETHER_BUILD_PLUGIN_TEST)
    juce_add_console_app(AetherUmbra_PluginTest)
    target_sources(AetherUmbra_PluginTest PRIVATE
      tools/plugin_test/plugin_test.cpp
      src/PluginProcessor.cpp
      src/PluginEditor.cpp)
    target_include_directories(AetherUmbra_PluginTest PRIVATE src)
    target_compile_definitions(AetherUmbra_PluginTest PRIVATE
      JUCE_WEB_BROWSER=0
      JUCE_USE_CURL=0
      AETHER_VERSION_STRING="${PROJECT_VERSION}")
    target_link_libraries(AetherUmbra_PluginTest PRIVATE
      aether_engine
      juce::juce_audio_utils
      juce::juce_recommended_config_flags)
  endif()
endif()
