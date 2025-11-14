cmake_minimum_required(VERSION 3.31)

project(wm
    DESCRIPTION "A Hyprland plugin providng an app switcher, a window switcher, and some useful dispatchers"
    VERSION 0.1
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_FLAGS "-Wall -Wpedantic -Wextra -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math -flto=auto")

add_subdirectory(external/small_vector)
add_subdirectory(cmake/nkutils)

find_package(libjpeg-turbo REQUIRED)
find_package(OpenCV)
find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    hyprland
    libdrm
    libinput
    libudev
    pangocairo
    pixman-1
    wayland-server
    xkbcommon
    glib-2.0
    glibmm-2.68
    gtkmm-4.0
    spng
    librsvg-2.0
)

add_subdirectory(src)
add_subdirectory(test)
