cmake_minimum_required(VERSION 3.31)

project(WM VERSION 0.1)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_FLAGS "-Wall -Wpedantic -Wextra -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -ffast-math -flto=auto")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HyprDeps REQUIRED IMPORTED_TARGET
    hyprland
    libdrm
    libinput
    libudev
    pangocairo
    pixman-1
    wayland-server
    xkbcommon
)

macro(add_wm_library name)
    cmake_parse_arguments(ARG "" "" "LINK_LIBS" ${ARGN})

    add_library(${name} OBJECT)

    target_sources(${name} PRIVATE ${ARG_UNPARSED_ARGUMENTS})

    if (ARG_LINK_LIBS)
        cmake_parse_arguments(LINK_LIBS_ARG "" "" "PUBLIC;PRIVATE" ${ARG_LINK_LIBS})
        if (LINK_LIBS_ARG_UNPARSED_ARGUMENTS)
            target_link_libraries(${name} PRIVATE ${LINK_LIBS_ARG_UNPARSED_ARGUMENTS})
        endif()
        if (LINK_LIBS_ARG_PUBLIC)
            target_link_libraries(${name} PUBLIC ${LINK_LIBS_ARG_PUBLIC})
        endif ()
        if (LINK_LIBS_ARG_PRIVATE)
            target_link_libraries(${name} PRIVATE ${LINK_LIBS_ARG_PRIVATE})
        endif ()
    endif ()
endmacro()

function(add_comptime_value name desc default)
    set(${name} ${default} CACHE STRING ${desc})
    add_compile_definitions(${name}=${${name}})
endfunction()

add_comptime_value(SWITCHER_MOD "The modifier key for switching apps and windows" KEY_LEFTMETA)
add_comptime_value(NUM_QUICK_ACCESS_APPS "The number of quick access apps" 10)

option(DEBUG_LOGS "Debug logs" OFF)
if (DEBUG_LOGS)
    add_compile_definitions(DEBUG_LOGS)
endif ()

include_directories("${CMAKE_SOURCE_DIR}/include")

add_subdirectory(lib)
add_subdirectory(plugin)
