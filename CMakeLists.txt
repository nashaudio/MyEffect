cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0184)
    cmake_policy(SET CMP0184 NEW)
endif()

project(MyEffect LANGUAGES C CXX)

# Keep every generated file in one disposable directory. CMakePresets.json
# selects this location for students using VS Code.
file(REAL_PATH "${CMAKE_SOURCE_DIR}/Build" MYEFFECT_BUILD_ROOT)
file(REAL_PATH "${CMAKE_BINARY_DIR}" MYEFFECT_BINARY_DIR)
string(FIND "${MYEFFECT_BINARY_DIR}" "${MYEFFECT_BUILD_ROOT}" MYEFFECT_BUILD_ROOT_POSITION)

if(PROJECT_IS_TOP_LEVEL AND NOT MYEFFECT_BUILD_ROOT_POSITION EQUAL 0)
    message(FATAL_ERROR
        "Configure MyEffect inside ${MYEFFECT_BUILD_ROOT}. "
        "Use: cmake --preset student")
endif()

add_subdirectory(Framework)
