cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0184)
    cmake_policy(SET CMP0184 NEW)
endif()

# Default macOS builds to Apple Silicon, including builds launched under Rosetta.
# A non-empty CMAKE_OSX_ARCHITECTURES value can still select another architecture.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architectures" FORCE)
endif()

project(MyEffect LANGUAGES C CXX)

# Keep every generated file in one disposable directory. CMakePresets.json
# selects this location for VS Code and command-line builds.
file(REAL_PATH "${CMAKE_SOURCE_DIR}/Build" MYEFFECT_BUILD_ROOT)
file(REAL_PATH "${CMAKE_BINARY_DIR}" MYEFFECT_BINARY_DIR)
string(FIND "${MYEFFECT_BINARY_DIR}" "${MYEFFECT_BUILD_ROOT}" MYEFFECT_BUILD_ROOT_POSITION)

if(PROJECT_IS_TOP_LEVEL AND NOT MYEFFECT_BUILD_ROOT_POSITION EQUAL 0)
    message(FATAL_ERROR
        "Configure MyEffect inside ${MYEFFECT_BUILD_ROOT}. "
        "Use: cmake --preset development")
endif()

add_subdirectory(Framework)
