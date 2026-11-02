# Aether as an AU / VST3 / standalone instrument.
# The DSP comes from faust2juce (make juce-export writes it to generated/juce/aether);
# the editor in Source/ replaces the generic Faust JUCE interface.
cmake_minimum_required(VERSION 3.22)
project(Aether VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

set(JUCE_DIR "" CACHE PATH "JUCE 8 checkout; downloaded when empty")
set(AETHER_EXPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../generated/juce/aether"
    CACHE PATH "faust2juce output holding FaustPluginProcessor.cpp and effect.h")
option(AETHER_INSTALL_PLUGINS "Copy the AU and VST3 into ~/Library/Audio/Plug-Ins after each build" OFF)

if(NOT EXISTS "${AETHER_EXPORT_DIR}/FaustPluginProcessor.cpp")
    message(FATAL_ERROR "No faust2juce export in ${AETHER_EXPORT_DIR}: run make juce-export first.")
endif()

if(JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                         GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

execute_process(COMMAND faust --includedir OUTPUT_VARIABLE FAUST_INCLUDE_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

juce_add_plugin(Aether
    PRODUCT_NAME "Aether"
    COMPANY_NAME "GRAME"
    BUNDLE_ID com.grame.aether
    PLUGIN_MANUFACTURER_CODE Grme
    PLUGIN_CODE Aeth
    FORMATS AU VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD ${AETHER_INSTALL_PLUGINS})

# The preset bank shared with the web interface and the offline renders.
juce_add_binary_data(AetherData SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../presets/patches.json")

target_sources(Aether PRIVATE
    "${AETHER_EXPORT_DIR}/FaustPluginProcessor.cpp"
    Source/AetherEditor.cpp)

# Source/ resolves "JuceLibraryCode/JuceHeader.h", Source/JuceLibraryCode resolves
# "../JuceLibraryCode/JuceHeader.h"; the export folder provides effect.h.
target_include_directories(Aether PRIVATE
    Source
    Source/JuceLibraryCode
    "${AETHER_EXPORT_DIR}"
    ${FAUST_INCLUDE_DIR})

# Same definitions as the .jucer written by faust2juce -nvoices 12 -effect auto -midi.
target_compile_definitions(Aether PUBLIC
    POLY2 MIDICTRL NVOICES=12 FAUSTFLOAT=float
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# The generated file inlines the Faust architecture, including the unused
# generic JuceGUI that relies on APIs JUCE 8 deprecates.
set_source_files_properties("${AETHER_EXPORT_DIR}/FaustPluginProcessor.cpp" PROPERTIES
    COMPILE_OPTIONS "-Wno-deprecated-declarations;-Wno-unused-parameter;-Wno-shadow")

target_link_libraries(Aether
    PRIVATE
        AetherData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# Offscreen check (make juce-snapshot): plays a chord through the processor and
# renders every editor tab to PNG. Not part of the default build.
juce_add_console_app(AetherSnapshot PRODUCT_NAME "AetherSnapshot")
set_target_properties(AetherSnapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(AetherSnapshot PRIVATE
    "${AETHER_EXPORT_DIR}/FaustPluginProcessor.cpp"
    Source/AetherEditor.cpp
    tools/Snapshot.cpp)
target_include_directories(AetherSnapshot PRIVATE
    Source Source/JuceLibraryCode "${AETHER_EXPORT_DIR}" ${FAUST_INCLUDE_DIR})
target_compile_definitions(AetherSnapshot PRIVATE
    POLY2 MIDICTRL NVOICES=12 FAUSTFLOAT=float
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1 # runDispatchLoopUntil lets timers and async calls run
    JucePlugin_Name="Aether" JucePlugin_WantsMidiInput=1 JucePlugin_ProducesMidiOutput=0)
target_link_libraries(AetherSnapshot PRIVATE
    AetherData juce::juce_audio_utils juce::juce_recommended_config_flags)
