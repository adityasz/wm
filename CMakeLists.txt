cmake_minimum_required(VERSION 4.3)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_MODULE_STD 1)

project(WM VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wpedantic -Wextra -march=native")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

include(GNUInstallDirs)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(AddWM)

include(generate_modules)

include_directories("${CMAKE_SOURCE_DIR}/include")

set(SWITCHER_MOD KEY_LEFTMETA CACHE STRING "The modifier key for switching apps and windows" )
option(DEBUG_LOGS "Enable debug logs" OFF)

add_compile_definitions(SWITCHER_MOD=${SWITCHER_MOD})
if(DEBUG_LOGS)
    add_compile_definitions(DEBUG_LOGS)
endif()

add_subdirectory(lib)
add_subdirectory(plugin)
