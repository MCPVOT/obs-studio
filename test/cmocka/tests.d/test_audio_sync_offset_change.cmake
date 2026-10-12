# A source's audio sync offset must apply while it is producing audio,
# including a change away from 0 and a saved offset set on load
# (upstream obsproject/obs-studio#7912). Mixing needs core video, so it runs
# (under xvfb-run) on Linux and skips without a display.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_audio_sync_offset_change test_audio_sync_offset_change.c)
  target_compile_definitions(
    test_audio_sync_offset_change
    PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>"
  )
  target_include_directories(test_audio_sync_offset_change PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_audio_sync_offset_change PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(test_audio_sync_offset_change libobs-opengl)
  add_dependencies(cmocka-tests test_audio_sync_offset_change)
  # libobs finds its effect files relative to the working directory.
  add_test(
    NAME test_audio_sync_offset_change
    COMMAND test_audio_sync_offset_change
    WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
  )
  set_tests_properties(test_audio_sync_offset_change PROPERTIES TIMEOUT 120)
endif()
