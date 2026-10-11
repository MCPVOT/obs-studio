# Transition audio must fade while an encoder rescales the output (upstream
# obsproject/obs-studio#12912). Needs core video, so it runs (under xvfb-run)
# on Linux and skips without a display.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_transition_audio_rescale test_transition_audio_rescale.c)
  target_compile_definitions(
    test_transition_audio_rescale
    PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>"
  )
  target_include_directories(test_transition_audio_rescale PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_transition_audio_rescale PRIVATE OBS::libobs ${CMOCKA_LIBRARIES} m)
  add_dependencies(test_transition_audio_rescale libobs-opengl)
  add_dependencies(cmocka-tests test_transition_audio_rescale)
  # libobs finds its effect files relative to the working directory.
  add_test(
    NAME test_transition_audio_rescale
    COMMAND test_transition_audio_rescale
    WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
  )
  set_tests_properties(test_transition_audio_rescale PROPERTIES TIMEOUT 60)
endif()
