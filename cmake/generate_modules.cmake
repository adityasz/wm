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
    lua
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
set(HYPRLAND_MODULES config debug desktop devices event helpers managers plugins render protocols xwayland layout)
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
    MODULES_DIR "${GENERATED_MODULES_DIR}/hyprland"
    MODULES ${HYPRLAND_MODULES}
    LINK_LIBS PUBLIC PkgConfig::HyprDeps
)

wm_add_library(Hyprutils
    MODULES_DIR "${GENERATED_MODULES_DIR}/hyprutils"
    MODULES ${HYPRUTILS_MODULES}
        LINK_LIBS PUBLIC PkgConfig::HyprDeps
)

find_package(LLVM REQUIRED CONFIG)
llvm_map_components_to_libnames(llvm_libs support)

set(llvm_headers ADT/SmallVector.h ADT/StringRef.h ADT/SmallString.h ADT/Twine.h ADT/STLExtras.h Support/Allocator.h Support/FileSystem.h Support/Path.h Support/StringSaver.h)
list(TRANSFORM llvm_headers PREPEND "${LLVM_INCLUDE_DIRS}/llvm/")
cxxmgen("llvm.Support"
    OUTPUT "${GENERATED_MODULES_DIR}/llvm/Support.ixx"
    HEADERS ${llvm_headers}
    RESTRICT
    JOBS 1
    CLANG_OPTS ${clang_opts}
)

wm_add_library(llvm_modules
    MODULES_DIR "${GENERATED_MODULES_DIR}/llvm"
    MODULES Support.ixx
    LINK_LIBS PUBLIC $<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,LLVM,${llvm_libs}>
)

set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_ENABLE_INSTALL OFF)
set(ABSL_BUILD_TESTING OFF)
add_subdirectory("${CMAKE_SOURCE_DIR}/external/abseil-cpp")

set(absl_INCLUDEDIR "${CMAKE_SOURCE_DIR}/external/abseil-cpp")
set(absl_headers
    container/flat_hash_map.h
    container/flat_hash_set.h
    container/internal/hashtable_debug_hooks.h
    hash/hash.h
)
list(TRANSFORM absl_headers PREPEND "${absl_INCLUDEDIR}/absl/")
cxxmgen("absl"
    OUTPUT "${GENERATED_MODULES_DIR}/absl/absl.ixx"
    HEADERS ${absl_headers}
    RESTRICT_PATHS "${absl_INCLUDEDIR}/absl"
    JOBS 1
    CLANG_OPTS ${clang_opts} ${absl_CFLAGS}
)

wm_add_library(absl_modules
    MODULES_DIR "${GENERATED_MODULES_DIR}/absl"
    MODULES absl.ixx
    LINK_LIBS PUBLIC absl::flat_hash_map absl::flat_hash_set absl::hash
)
