# Hiding a group must stop a child whose scene item has show/hide transitions
# (obsproject/obs-studio#5551). Hiding a group is applied by the audio thread,
# which needs core video, so it runs (under xvfb-run) on Linux only.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_group_hide_transition_audio test_group_hide_transition_audio.c)
  target_compile_definitions(
    test_group_hide_transition_audio
    PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>"
  )
  target_include_directories(test_group_hide_transition_audio PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_group_hide_transition_audio PRIVATE OBS::libobs OBS::caption ${CMOCKA_LIBRARIES} m)
  add_dependencies(test_group_hide_transition_audio libobs-opengl)
  add_dependencies(cmocka-tests test_group_hide_transition_audio)
  # libobs finds its effect files relative to the working directory.
  add_test(
    NAME test_group_hide_transition_audio
    COMMAND test_group_hide_transition_audio
    WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
  )
  set_tests_properties(test_group_hide_transition_audio PROPERTIES TIMEOUT 120)
endif()
