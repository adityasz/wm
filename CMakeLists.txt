cmake_minimum_required(VERSION 3.31)

# This varies with cmake version.
# See https://github.com/Kitware/CMake/blob/v${cmake_version}/Help/dev/experimental.rst
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")

# Fedora compiles clang with libstdc++ as the default stdlib. CMake wants libc++.
# Also, std.cc is broken in gcc 15.2.1: See GCC bug 122625.
execute_process(
    COMMAND echo "#include <ciso646>"
    COMMAND ${CMAKE_CXX_COMPILER} -E -dM -x c++ -
    OUTPUT_VARIABLE _CXX_MACROS
    RESULT_VARIABLE _CXX_STDLIB_RESULT
)
if (NOT _CXX_STDLIB_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to detect the default C++ standard library configuration.")
endif ()
if (NOT _CXX_MACROS MATCHES "#define _LIBCPP_VERSION")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif ()

# Compile the standard library with the right flags.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(WM VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_MODULE_STD 1)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wpedantic -Wextra -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -ffast-math -flto=auto")

macro(add_wm_library target_name)
    add_library(${target_name} OBJECT)

    set(__scope PUBLIC)
    set(__pub_modules)
    set(__priv_modules)
    set(__pub_sources)
    set(__priv_sources)

    foreach (__arg ${ARGN})
        if (__arg STREQUAL "PUBLIC")
            set(__scope PUBLIC)
        elseif (__arg STREQUAL "PRIVATE")
            set(__scope PRIVATE)
        elseif (__arg MATCHES "\.ixx$")
            if (__scope STREQUAL "PUBLIC")
                list(APPEND __pub_modules "${CMAKE_SOURCE_DIR}/modules/${target_name}/${__arg}")
            else ()
                list(APPEND __priv_modules "${__arg}")
            endif ()
        else ()
            if (__scope STREQUAL "PUBLIC")
                list(APPEND __pub_sources "${__arg}")
            else ()
                list(APPEND __priv_sources "${__arg}")
            endif ()
        endif ()
    endforeach ()

    target_sources(${target_name}
       PUBLIC
           ${__pub_sources}
       PUBLIC
           FILE_SET modules_${target_name}_
           TYPE CXX_MODULES
           BASE_DIRS ${CMAKE_SOURCE_DIR}/modules/${target_name}
           FILES ${__pub_modules}
       PRIVATE
           ${__priv_sources}
       PRIVATE
           FILE_SET priv_modules_${target_name}_
           TYPE CXX_MODULES
           BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
           FILES ${__priv_modules}
    )
    target_include_directories(${target_name} PUBLIC ${CMAKE_SOURCE_DIR}/include)
endmacro()

function(add_comptime_value target name desc default)
    set(${name} ${default} CACHE STRING ${desc})
    target_compile_definitions(${target} PRIVATE ${name}=${${name}})
endfunction()

add_subdirectory(external/small_vector)

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

option(DEBUG_LOGS "Debug logs" OFF)
if(DEBUG_LOGS)
    add_compile_definitions(DEBUG_LOGS)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory(lib)
add_subdirectory(plugin)
