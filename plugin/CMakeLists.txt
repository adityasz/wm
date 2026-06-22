add_library(wm SHARED wm.cpp)
set_target_properties(wm PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(wm PRIVATE WindowManager AppSwitcher WindowSwitcher Support llvm_modules Hyprland nkutils-icons)
