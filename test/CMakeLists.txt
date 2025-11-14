find_package(GTest REQUIRED)

add_executable(test_stuff test_stuff.cpp ${CMAKE_SOURCE_DIR}/src/Image.cpp)
target_include_directories(test_stuff
    PRIVATE ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_SOURCE_DIR}/src
    PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(test_stuff
    PRIVATE rt
    PRIVATE PkgConfig::deps
    PRIVATE
    PRIVATE GTest::gtest
    PRIVATE GTest::gtest_main
    PRIVATE gch::small_vector
    PRIVATE turbojpeg
    PRIVATE nkutils-icons
    PRIVATE ${OpenCV_LIBS})

enable_testing()
add_test(NAME AppInfoTest COMMAND test_stuff)
