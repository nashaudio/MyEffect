# The analyzer and automatic editor are infrastructure, not student code.
# Optimise this one translation unit in Debug while leaving Source/ and
# PluginProcessor.cpp under the normal Debug settings and runtime checks.

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
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    PRODUCT_NAME_WITHOUT_VERSION="MyEffect")

target_compile_features(MyEffectHarness PRIVATE cxx_std_17)
