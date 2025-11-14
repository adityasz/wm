add_library(wm SHARED)

file(GLOB_RECURSE SRC "*.cpp")
target_sources(wm PUBLIC ${SRC})

set_target_properties(wm PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

target_include_directories(wm
    PUBLIC ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(wm
    PUBLIC rt
    PUBLIC PkgConfig::deps
    PUBLIC turbojpeg
    PRIVATE gch::small_vector
    PRIVATE nkutils-icons)

option(DEBUG_LOGS "Logs for debugging" OFF)
target_compile_definitions(wm PRIVATE DEBUG_LOGS)

function(add_comptime_value name desc default)
    set(${name} ${default} CACHE STRING ${desc})
    target_compile_definitions(wm PRIVATE ${name}=${${name}})
endfunction()

add_comptime_value(SWITCHER_MOD "The modifier key for switching apps and windows" KEY_LEFTMETA)
add_comptime_value(NUM_QUICK_ACCESS_APPS "The number of quick access apps" 10)

install(TARGETS wm)
