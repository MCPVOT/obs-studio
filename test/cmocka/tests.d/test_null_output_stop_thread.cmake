# obs-outputs: the null output joins every stop thread it starts, also one
# that finished before the next start (split from obs-rust/obs-studio#1464).
# The real null-output.c is compiled in with libobs's output calls and
# pthread_create/pthread_join wrapped (GNU ld --wrap, so Linux only), so no
# encoder or display is needed.
if(OS_LINUX AND TARGET OBS::outputs)
  add_executable(test_null_output_stop_thread test_null_output_stop_thread.c)
  # No network code runs, so librtmp's TLS types are not needed.
  target_compile_definitions(test_null_output_stop_thread PRIVATE NO_CRYPTO)
  # The precompiled header's static helpers are unused here.
  target_compile_options(test_null_output_stop_thread PRIVATE -Wno-unused-function)
  target_include_directories(
    test_null_output_stop_thread
    PRIVATE
      ${CMOCKA_INCLUDE_DIR}
      "${CMAKE_SOURCE_DIR}"
      "${CMAKE_SOURCE_DIR}/plugins/obs-outputs"
      "${CMAKE_SOURCE_DIR}/shared/opts-parser"
      "${CMAKE_SOURCE_DIR}/shared/bpm"
  )
  target_link_libraries(test_null_output_stop_thread PRIVATE OBS::libobs Threads::Threads ${CMOCKA_LIBRARIES})
  target_link_options(
    test_null_output_stop_thread
    PRIVATE
      "LINKER:--wrap=obs_output_can_begin_data_capture,--wrap=obs_output_initialize_encoders,--wrap=obs_output_begin_data_capture,--wrap=obs_output_end_data_capture,--wrap=pthread_create,--wrap=pthread_join"
  )
  add_dependencies(cmocka-tests test_null_output_stop_thread)
  add_test(test_null_output_stop_thread ${CMAKE_CURRENT_BINARY_DIR}/test_null_output_stop_thread)
  set_tests_properties(test_null_output_stop_thread PROPERTIES TIMEOUT 60)
endif()
