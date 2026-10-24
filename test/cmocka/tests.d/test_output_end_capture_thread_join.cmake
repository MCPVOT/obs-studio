# libobs: each stop's end_data_capture thread is joined (obs-rust/obs-studio#2063).
# Interposes pthread_create/join/detach; needs core video, so it runs on Linux
# and skips without a display.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_output_end_capture_thread_join test_output_end_capture_thread_join.c)
  target_compile_definitions(
    test_output_end_capture_thread_join
    PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>"
  )
  # Exported, so libobs binds its pthread calls to this executable's.
  set_target_properties(test_output_end_capture_thread_join PROPERTIES C_VISIBILITY_PRESET default)
  target_include_directories(test_output_end_capture_thread_join PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_output_end_capture_thread_join PRIVATE OBS::libobs ${CMOCKA_LIBRARIES} ${CMAKE_DL_LIBS})
  add_dependencies(test_output_end_capture_thread_join libobs-opengl)
  add_dependencies(cmocka-tests test_output_end_capture_thread_join)
  # libobs finds its effect files relative to the working directory.
  add_test(
    NAME test_output_end_capture_thread_join
    COMMAND test_output_end_capture_thread_join
    WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
  )
  set_tests_properties(test_output_end_capture_thread_join PROPERTIES TIMEOUT 60)
endif()
