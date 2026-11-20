cmake_minimum_required(VERSION 3.22)

# Windows: Laufzeit statisch linken -> Plugin laeuft auch ohne installierte VC++-Redistributables
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
# macOS: Universal Binary (Apple Silicon + Intel) - Cubase laeuft nativ auf beiden
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
endif()

project(NovaTone VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NOVATONE_COPY_AFTER_BUILD "VST3 nach dem Build automatisch in den System-Plugin-Ordner kopieren (Windows: Admin noetig)" OFF)
option(NOVATONE_BUILD_TESTS "JUCE-freien Engine-Test mitbauen (Ziel: nova_engine_test)" OFF)

# Der Engine-Test braucht kein JUCE und laesst sich auch separat bauen: Tests/run_tests.sh
if(NOVATONE_BUILD_TESTS)
    add_executable(nova_engine_test Tests/engine_test.cpp)
    target_compile_features(nova_engine_test PRIVATE cxx_std_17)
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(NovaTone
    COMPANY_NAME "NovaTone Audio"
    BUNDLE_ID "com.novatone.audio"
    PLUGIN_MANUFACTURER_CODE Nvtn
    PLUGIN_CODE NvTn
    FORMATS VST3 Standalone
    PRODUCT_NAME "NovaTone"
    DESCRIPTION "Hybrid instrument for bass, synth and piano"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD ${NOVATONE_COPY_AFTER_BUILD}
)

juce_generate_juce_header(NovaTone)

target_sources(NovaTone PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Engine/NovaCommon.h
    Source/Engine/NovaFilters.h
    Source/Engine/NovaFx.h
    Source/Engine/NovaVoice.h
    Source/Engine/NovaEngine.h
    Source/Engine/NovaParams.h
    Source/Engine/NovaPresets.h
)

target_compile_definitions(NovaTone PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

if(MSVC)
    target_compile_options(NovaTone PRIVATE /utf-8)
endif()

target_link_libraries(NovaTone PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
