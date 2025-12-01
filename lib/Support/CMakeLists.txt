find_package(libjpeg-turbo REQUIRED)
pkg_check_modules(AppIconDeps REQUIRED IMPORTED_TARGET
    gio-2.0
    gobject-2.0
    glibmm-2.68
    gtkmm-4.0
    spng
    librsvg-2.0
)

set(NKUTILS_SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/libnkutils)
add_library(nkutils-icons OBJECT
    ${NKUTILS_SOURCE_DIR}/core/src/enum.c
    ${NKUTILS_SOURCE_DIR}/core/src/format-string.c
    ${NKUTILS_SOURCE_DIR}/core/src/colour.c
    ${NKUTILS_SOURCE_DIR}/core/src/xdg-de.c
    ${NKUTILS_SOURCE_DIR}/core/src/gtk-settings.c
    ${NKUTILS_SOURCE_DIR}/core/src/xdg-theme.c
)
set_property(TARGET nkutils-icons PROPERTY C_STANDARD 11)
target_include_directories(nkutils-icons
    PUBLIC
        $<BUILD_INTERFACE:${NKUTILS_SOURCE_DIR}/core/include>
        ${GLIB_INCLUDE_DIRS}
)
target_compile_options(nkutils-icons
    PRIVATE
        "-DSYSCONFDIR=\"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}\""
        "-DGLIB_VERSION_MIN_REQUIRED=(G_ENCODE_VERSION(2,40))"
        -DNK_EXPORT=
)
target_link_libraries(nkutils-icons PUBLIC ${GLIB_LIBRARIES})

add_wm_library(Support
    PUBLIC
        Support.ixx
        AppInfoLoader.ixx
        Image.ixx
        Logging.ixx
        Utils.ixx
    PRIVATE
        AppInfoLoader.cpp
        Image.cpp
        Utils.cpp
)
target_link_libraries(Support
    PRIVATE
        PkgConfig::AppIconDeps
        nkutils-icons
    PUBLIC
        gch::small_vector
        PkgConfig::HyprDeps
)
