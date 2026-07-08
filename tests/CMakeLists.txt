find_package(GTest REQUIRED)

set(APP_INFO_TEST_DEPS
    GTest::gtest
    GTest::gtest_main
    llvm_modules
    absl_modules
    AppInfoLoader
    nkutils-icons
)

add_executable(DesktopFileReadTest DesktopFileRead.cpp)
target_link_libraries(DesktopFileReadTest PRIVATE ${APP_INFO_TEST_DEPS})

add_executable(XdgAppDirsTest XdgAppDirs.cpp)
target_link_libraries(XdgAppDirsTest PRIVATE ${APP_INFO_TEST_DEPS})

add_executable(AppInfoTest AppInfo.cpp)
target_link_libraries(AppInfoTest PRIVATE ${APP_INFO_TEST_DEPS})

enable_testing()
add_test(NAME DesktopFileReadTest COMMAND DesktopFileReadTest)
add_test(NAME XdgAppDirsTest COMMAND XdgAppDirsTest)
add_test(NAME AppInfoTest COMMAND AppInfoTest)
