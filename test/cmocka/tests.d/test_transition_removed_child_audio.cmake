# A transition's audio must not come from a removed child after the audio
# stop (#2185). Needs core video, so it runs (under xvfb-run) on Linux and
# skips without a display.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_transition_removed_child_audio test_transition_removed_child_audio.c)
  target_compile_definitions(
    test_transition_removed_child_audio
    PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>"
  )
  target_include_directories(test_transition_removed_child_audio PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_transition_removed_child_audio PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(test_transition_removed_child_audio libobs-opengl)
  add_dependencies(cmocka-tests test_transition_removed_child_audio)
  # libobs finds its effect files relative to the working directory.
  add_test(
    NAME test_transition_removed_child_audio
    COMMAND test_transition_removed_child_audio
    WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
  )
  set_tests_properties(test_transition_removed_child_audio PROPERTIES TIMEOUT 60)
endif()
