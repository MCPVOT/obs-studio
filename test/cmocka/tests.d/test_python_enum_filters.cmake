# Python scripts can enumerate a source's filters
# (obsproject/obs-studio#12116). Loads a real Python script through
# obs-scripting, so it needs Python scripting support. obs_startup needs a
# display, so it runs (under xvfb-run) on Linux and skips without one.
if(OS_LINUX AND TARGET OBS::scripting AND TARGET OBS::python)
  add_executable(test_python_enum_filters test_python_enum_filters.c)
  target_include_directories(test_python_enum_filters PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_python_enum_filters PRIVATE OBS::libobs OBS::scripting ${CMOCKA_LIBRARIES})
  add_dependencies(test_python_enum_filters obspython)
  add_dependencies(cmocka-tests test_python_enum_filters)
  # obs-scripting finds obspython relative to the working directory.
  add_test(
    NAME test_python_enum_filters
    COMMAND test_python_enum_filters
    WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
  )
  set_tests_properties(test_python_enum_filters PROPERTIES TIMEOUT 120)
endif()
