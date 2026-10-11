# Encoders shut down after a failed output start (obsproject/obs-studio#10651).
if(NOT WIN32)
  add_executable(test_output_failed_start_encoders test_output_failed_start_encoders.c)
  target_include_directories(test_output_failed_start_encoders PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_output_failed_start_encoders PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_output_failed_start_encoders)
  add_test(test_output_failed_start_encoders ${CMAKE_CURRENT_BINARY_DIR}/test_output_failed_start_encoders)
endif()
