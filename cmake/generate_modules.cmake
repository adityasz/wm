find_package(cxxmgen)

find_program(CLANG clang REQUIRED)
execute_process(
    COMMAND ${CLANG} -print-resource-dir
    OUTPUT_VARIABLE CLANG_RESOURCE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

find_package(PkgConfig REQUIRED)

pkg_check_modules(HyprDeps REQUIRED IMPORTED_TARGET
    hyprland
    hyprutils
    libdrm
    libinput
    libudev
    pangocairo
    pixman-1
    wayland-server
    xkbcommon
)

separate_arguments(clang_opts NATIVE_COMMAND ${CMAKE_CXX_FLAGS})
if(NOT DEFINED CMAKE_BUILD_TYPE OR NOT DEFINED CMAKE_CXX_STANDARD)
    message(FATAL_ERROR "...")
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" uppercase_CMAKE_BUILD_TYPE)
separate_arguments(build_type_flags NATIVE_COMMAND "${CMAKE_CXX_FLAGS_${uppercase_CMAKE_BUILD_TYPE}}")
list(APPEND clang_opts
    ${build_type_flags}
    "-std=c++${CMAKE_CXX_STANDARD}"
    -x c++-header
    -resource-dir
    ${CLANG_RESOURCE_DIR}
)

set(GENERATED_MODULES_DIR "${CMAKE_BINARY_DIR}/modules")

# HORRIBLE: PREFIX already included in hyprutils INCLUDEDIR but not in hyprland INCLUDEDIR.
set(HYPRLAND_MODULES config debug desktop devices event helpers managers plugins render protocols)
foreach(module IN LISTS HYPRLAND_MODULES)
    cxxmgen("hyprland.${module}"
        OUTPUT "${GENERATED_MODULES_DIR}/hyprland/${module}.ixx"
        HEADERS "${CMAKE_SOURCE_DIR}/include/hyprland/${module}.h"
        RESTRICT_PATHS "${HyprDeps_hyprland_PREFIX}/${HyprDeps_hyprland_INCLUDEDIR}/hyprland/src/${module}"
        CLANG_OPTS ${clang_opts} ${HyprDeps_CFLAGS}
    )
endforeach()
cxxmgen("hyprland.globals"
    OUTPUT "${GENERATED_MODULES_DIR}/hyprland/globals.ixx"
    HEADERS "${CMAKE_SOURCE_DIR}/include/hyprland/globals.h"
    RESTRICT_PATHS "${HyprDeps_hyprland_PREFIX}/${HyprDeps_hyprland_INCLUDEDIR}/hyprland/src"
    JOBS 1
    CLANG_OPTS ${clang_opts} ${HyprDeps_CFLAGS}
)
list(APPEND HYPRLAND_MODULES "globals")
list(TRANSFORM HYPRLAND_MODULES APPEND ".ixx")

set(HYPRUTILS_MODULES math memory cli signal)
foreach(module IN LISTS HYPRUTILS_MODULES)
    file(GLOB_RECURSE headers "${HyprDeps_hyprutils_INCLUDEDIR}/hyprutils/${module}/*.hpp")
    cxxmgen("hyprutils.${module}"
        OUTPUT "${GENERATED_MODULES_DIR}/hyprutils/${module}.ixx"
        HEADERS ${headers}
        RESTRICT_PATHS "${HyprDeps_hyprutils_INCLUDEDIR}/hyprutils/${module}"
        JOBS 1
        CLANG_OPTS ${clang_opts} ${HyprDeps_CFLAGS}
    )
endforeach()
list(TRANSFORM HYPRUTILS_MODULES APPEND ".ixx")

wm_add_library(Hyprland
	MODULES_DIR ${GENERATED_MODULES_DIR}/hyprland
    MODULES ${HYPRLAND_MODULES}
    LINK_LIBS PUBLIC PkgConfig::HyprDeps
)

wm_add_library(Hyprutils
	MODULES_DIR ${GENERATED_MODULES_DIR}/hyprutils
	MODULES ${HYPRUTILS_MODULES}
    LINK_LIBS PUBLIC PkgConfig::HyprDeps
)
