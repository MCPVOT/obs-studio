# Moving one child of a group must not drag its siblings across the canvas
# (obsproject/obs-studio#9298). Groups resize on video ticks, so this needs
# core video and runs (under xvfb-run) on Linux only.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_group_child_drift test_group_child_drift.c)
  target_compile_definitions(test_group_child_drift PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>")
  target_include_directories(test_group_child_drift PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_group_child_drift PRIVATE OBS::libobs ${CMOCKA_LIBRARIES} m)
  add_dependencies(test_group_child_drift libobs-opengl)
  add_dependencies(cmocka-tests test_group_child_drift)
  # libobs finds its effect files relative to the working directory.
  add_test(NAME test_group_child_drift COMMAND test_group_child_drift WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>")
  set_tests_properties(test_group_child_drift PROPERTIES TIMEOUT 120)
endif()
