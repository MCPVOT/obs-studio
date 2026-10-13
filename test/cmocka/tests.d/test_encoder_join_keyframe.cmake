# An output started on a running video encoder must not wait for the
# encoder's next scheduled keyframe (obsproject/obs-studio#12069). Needs core
# video, so it runs (under xvfb-run) on Linux and skips without a display.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_encoder_join_keyframe test_encoder_join_keyframe.c)
  target_compile_definitions(
    test_encoder_join_keyframe
    PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>"
  )
  target_include_directories(test_encoder_join_keyframe PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_encoder_join_keyframe PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(test_encoder_join_keyframe libobs-opengl)
  add_dependencies(cmocka-tests test_encoder_join_keyframe)
  # libobs finds its effect files relative to the working directory.
  add_test(
    NAME test_encoder_join_keyframe
    COMMAND test_encoder_join_keyframe
    WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
  )
  set_tests_properties(test_encoder_join_keyframe PROPERTIES TIMEOUT 120)
endif()
