cmake_minimum_required(VERSION 3.22)
project(AetherUmbra VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  # Universal (Apple silicon + Intel) is set by the CI workflow; macOS 11+.
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

# Milestone 0: two empty test shells of the plugin, built from the same code:
#   AetherUmbra Test Instrument    AU type aumu (an instrument)
#   AetherUmbra Test Music Effect  AU type aumf (a music effect: audio in AND notes in)
# Logic decides which of the two can be fed audio and notes at the same time. The firmware copy
# (third_party/oneiroi-firmware) is not compiled yet.
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(FORMATS VST3)
if(APPLE)
  list(APPEND FORMATS AU)
endif()
# For trying the window without a host (not part of the download): -DAU_EXTRA_FORMATS=Standalone
set(AU_EXTRA_FORMATS "" CACHE STRING "Extra plugin formats for local testing")
list(APPEND FORMATS ${AU_EXTRA_FORMATS})

set(SHELL_SOURCES src/ShellProcessor.cpp src/ShellEditor.cpp)

function(aetherumbra_shell TARGET NAME CODE IS_SYNTH KIND_DEF ENTRY)
  juce_add_plugin(${TARGET}
    COMPANY_NAME "particledots"
    PLUGIN_MANUFACTURER_CODE Ptdt
    PLUGIN_CODE ${CODE}
    FORMATS ${FORMATS}
    PRODUCT_NAME "${NAME}"
    IS_SYNTH ${IS_SYNTH}
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
  target_sources(${TARGET} PRIVATE ${SHELL_SOURCES} ${ENTRY})
  target_include_directories(${TARGET} PRIVATE src)
  target_compile_definitions(${TARGET} PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    AETHER_VERSION_STRING="${PROJECT_VERSION}"
    ${KIND_DEF})
  target_link_libraries(${TARGET} PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
endfunction()

aetherumbra_shell(AetherUmbra_Instrument "AetherUmbra Test Instrument" Aeu1 TRUE AETHER_KIND_INSTRUMENT src/EntryInstrument.cpp)
aetherumbra_shell(AetherUmbra_MusicEffect "AetherUmbra Test Music Effect" Aeu2 FALSE AETHER_KIND_MUSIC_EFFECT src/EntryMusicEffect.cpp)

# Plugin test (not part of the download): runs the plugin code without a host.
#   cmake -S . -B build -DAU_BUILD_PLUGIN_TEST=ON && cmake --build build --target AetherUmbra_PluginTest
option(AU_BUILD_PLUGIN_TEST "Build the plugin test program" OFF)
if(AU_BUILD_PLUGIN_TEST)
  juce_add_console_app(AetherUmbra_PluginTest)
  target_sources(AetherUmbra_PluginTest PRIVATE tools/plugin_test/plugin_test.cpp ${SHELL_SOURCES})
  target_include_directories(AetherUmbra_PluginTest PRIVATE src)
  target_compile_definitions(AetherUmbra_PluginTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    AETHER_VERSION_STRING="${PROJECT_VERSION}")
  target_link_libraries(AetherUmbra_PluginTest PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags)
endif()
