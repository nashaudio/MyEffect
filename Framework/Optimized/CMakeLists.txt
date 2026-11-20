# The analyser and automatic editor are platform services.
# Optimise this one translation unit in Debug while leaving the effect sources
# and PluginProcessor.cpp under the normal Debug settings and runtime checks.

if(MSVC)
    string(REPLACE "/Od" "/O2" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Ob0" "/Ob2" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    string(APPEND CMAKE_CXX_FLAGS_DEBUG " -O2")
endif()

add_library(MyEffectHarness OBJECT ../source/PluginEditor.cpp)

set_target_properties(MyEffectHarness PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    MSVC_RUNTIME_CHECKS "$<$<CONFIG:__never__>:StackFrameErrorCheck>")

target_include_directories(MyEffectHarness
    PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../include"
    "${CMAKE_CURRENT_LIST_DIR}/../../Source"
    "${CMAKE_CURRENT_LIST_DIR}/../JUCE/modules")

target_compile_definitions(MyEffectHarness
    PRIVATE
    # Inherit the plugin's JUCE settings, including its configuration-dependent
    # DEBUG/NDEBUG definitions, without pulling in JUCE implementation sources.
    $<TARGET_PROPERTY:MyEffect,COMPILE_DEFINITIONS>)

target_compile_features(MyEffectHarness PRIVATE cxx_std_17)
