# A scene must not be moved into a group the scene itself contains
# (obsproject/obs-studio#10466). Each case forks a child with core video, so it
# runs (under xvfb-run) on Linux and skips without a display.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_group_recursion test_group_recursion.c)
  target_compile_definitions(test_group_recursion PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>")
  target_include_directories(test_group_recursion PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_group_recursion PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(test_group_recursion libobs-opengl)
  add_dependencies(cmocka-tests test_group_recursion)
  # libobs finds its effect files relative to the working directory.
  add_test(NAME test_group_recursion COMMAND test_group_recursion WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>")
  set_tests_properties(test_group_recursion PROPERTIES TIMEOUT 180)
endif()
