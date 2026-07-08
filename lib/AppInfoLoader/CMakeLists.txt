find_package(libjpeg-turbo REQUIRED)
pkg_check_modules(AppInfoDeps REQUIRED IMPORTED_TARGET
    glib-2.0
    gio-2.0
    gobject-2.0
    spng
    librsvg-2.0
)

set(NKUTILS_SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/libnkutils)
add_library(nkutils-icons OBJECT
    ${NKUTILS_SOURCE_DIR}/core/src/enum.c
    ${NKUTILS_SOURCE_DIR}/core/src/format-string.c
    ${NKUTILS_SOURCE_DIR}/core/src/colour.c
    ${NKUTILS_SOURCE_DIR}/core/src/xdg-de.c
    ${NKUTILS_SOURCE_DIR}/core/src/gtk-settings.c
    ${NKUTILS_SOURCE_DIR}/core/src/xdg-theme.c
)
set_property(TARGET nkutils-icons PROPERTY C_STANDARD 11)
target_include_directories(nkutils-icons PUBLIC
    $<BUILD_INTERFACE:${NKUTILS_SOURCE_DIR}/core/include>
)
target_compile_options(nkutils-icons PRIVATE
    "-DSYSCONFDIR=\"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}\""
    "-DGLIB_VERSION_MIN_REQUIRED=(G_ENCODE_VERSION(2,40))"
    -DNK_EXPORT=
)
target_link_libraries(nkutils-icons PUBLIC PkgConfig::AppInfoDeps)

wm_add_library(AppInfoLoader
    AppInfoLoader.cpp
    Xdg.cpp
    Image.cpp

    MODULES
        AppInfoLoader.ixx
        Image.ixx
        Xdg.ixx

    LINK_LIBS
        PUBLIC llvm_modules absl_modules nkutils-icons turbojpeg Support PkgConfig::AppInfoDeps
)
