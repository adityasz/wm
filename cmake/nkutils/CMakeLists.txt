set(NKUTILS_SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/libnkutils)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0>=2.40 gio-2.0 gobject-2.0)

add_library(nkutils-icons STATIC
    ${NKUTILS_SOURCE_DIR}/core/src/enum.c
    ${NKUTILS_SOURCE_DIR}/core/src/format-string.c
    ${NKUTILS_SOURCE_DIR}/core/src/colour.c
    ${NKUTILS_SOURCE_DIR}/core/src/xdg-de.c
    ${NKUTILS_SOURCE_DIR}/core/src/gtk-settings.c
    ${NKUTILS_SOURCE_DIR}/core/src/xdg-theme.c
)

set_property(TARGET nkutils-icons PROPERTY C_STANDARD 11)
set_property(TARGET nkutils-icons PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(nkutils-icons PUBLIC
    $<BUILD_INTERFACE:${NKUTILS_SOURCE_DIR}/core/include>
    ${GLIB_INCLUDE_DIRS})

target_compile_options(nkutils-icons PRIVATE
    "-DSYSCONFDIR=\"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}\""
    "-DGLIB_VERSION_MIN_REQUIRED=(G_ENCODE_VERSION(2,40))"
    -DNK_EXPORT=
)

target_link_libraries(nkutils-icons PUBLIC ${GLIB_LIBRARIES})
