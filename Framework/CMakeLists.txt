# MyEffect JUCE adapter
#
# This is intentionally a small build description. Product packaging, CI,
# CLAP, benchmarks, test frameworks and optional UI tooling belong in the
# development repository, not in the student starter.

set(MYEFFECT_TARGET MyEffect)
set(MYEFFECT_FORMATS Standalone VST3)

if(APPLE)
    list(APPEND MYEFFECT_FORMATS AU)
endif()

# JUCE is vendored with the adapter so the student project has no submodules.
add_subdirectory(JUCE)

juce_add_plugin(${MYEFFECT_TARGET}
    VERSION 1.0.0
    COMPANY_NAME "nash.audio"
    BUNDLE_ID "com.audio.nash.myeffect"
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE NAFX
    PLUGIN_CODE P001
    FORMATS ${MYEFFECT_FORMATS}
    PRODUCT_NAME "MyEffect")

# The supplied VS/Xcode projects compile PluginProcessor.cpp directly. It
# includes the two student .cpp files so every supported IDE uses precisely
# the same source boundary without needing to regenerate project files.
set(MYEFFECT_SOURCES
    source/PluginEditor.h
    source/PluginProcessor.cpp
    source/PluginProcessor.h
    source/AnalysisComponent.h
    include/StandaloneHarness.h
    ../Source/EffectPlugin.h
    ../Source/EffectExtra.h)

# This subdirectory changes Debug optimisation flags only for PluginEditor.cpp.
# It deliberately does not link JUCE module targets, because those targets also
# inject their implementation sources into consumers.
add_subdirectory(Optimized)

target_sources(${MYEFFECT_TARGET} PRIVATE
    ${MYEFFECT_SOURCES}
    $<TARGET_OBJECTS:MyEffectHarness>)

target_include_directories(${MYEFFECT_TARGET}
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Source")

target_compile_definitions(${MYEFFECT_TARGET}
    PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    PRODUCT_NAME_WITHOUT_VERSION="MyEffect")

target_link_libraries(${MYEFFECT_TARGET}
    PRIVATE
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

target_compile_features(${MYEFFECT_TARGET} PRIVATE cxx_std_17)

# Keep final products together; CMake's own intermediates remain beneath
# Build/CMake as selected by CMakePresets.json.
foreach(target
        ${MYEFFECT_TARGET}
        ${MYEFFECT_TARGET}_Standalone
        ${MYEFFECT_TARGET}_VST3
        ${MYEFFECT_TARGET}_AU)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/Products/$<CONFIG>"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/Products/$<CONFIG>"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/Products/$<CONFIG>")
    endif()
endforeach()
