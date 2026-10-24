# obs_reset_video() refuses a zero fps_num or fps_den before it frees the
# running video (#1713). Public API only, so it also covers the hybrid
# libobs. Needs core video, so it runs (under xvfb-run) on Linux and skips
# without a display.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_video_reset_fps test_video_reset_fps.c)
  target_compile_definitions(test_video_reset_fps PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>")
  target_include_directories(test_video_reset_fps PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_video_reset_fps PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(test_video_reset_fps libobs-opengl)
  add_dependencies(cmocka-tests test_video_reset_fps)
  # libobs finds its effect files relative to the working directory.
  find_program(XVFB_RUN xvfb-run)
  if(XVFB_RUN)
    add_test(
      NAME test_video_reset_fps
      COMMAND ${XVFB_RUN} -a $<TARGET_FILE:test_video_reset_fps>
      WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
    )
  else()
    add_test(NAME test_video_reset_fps COMMAND test_video_reset_fps WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>")
  endif()
  set_tests_properties(test_video_reset_fps PROPERTIES TIMEOUT 60)
endif()
