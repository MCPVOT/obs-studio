# "Skipped frames due to encoding lag" must count encoders that rescale the
# output (upstream obsproject/obs-studio#11078). Needs core video, so it runs
# (under xvfb-run) on Linux and skips without a display.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_encoding_lag_rescale test_encoding_lag_rescale.c)
  target_compile_definitions(test_encoding_lag_rescale PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>")
  target_include_directories(test_encoding_lag_rescale PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_encoding_lag_rescale PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(test_encoding_lag_rescale libobs-opengl)
  add_dependencies(cmocka-tests test_encoding_lag_rescale)
  # libobs finds its effect files relative to the working directory.
  add_test(
    NAME test_encoding_lag_rescale
    COMMAND test_encoding_lag_rescale
    WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
  )
  set_tests_properties(test_encoding_lag_rescale PROPERTIES TIMEOUT 60)
endif()
