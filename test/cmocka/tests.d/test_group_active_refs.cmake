# Moving scene items into or out of groups keeps activate/show counts balanced
# (obsproject/obs-studio#9961). Hiding a group is applied by the audio thread,
# which needs core video, so it runs (under xvfb-run) on Linux only.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_group_active_refs test_group_active_refs.c)
  target_compile_definitions(test_group_active_refs PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>")
  target_include_directories(test_group_active_refs PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_group_active_refs PRIVATE OBS::libobs OBS::caption ${CMOCKA_LIBRARIES})
  add_dependencies(test_group_active_refs libobs-opengl)
  add_dependencies(cmocka-tests test_group_active_refs)
  # libobs finds its effect files relative to the working directory.
  add_test(NAME test_group_active_refs COMMAND test_group_active_refs WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>")
  set_tests_properties(test_group_active_refs PROPERTIES TIMEOUT 120)
endif()
