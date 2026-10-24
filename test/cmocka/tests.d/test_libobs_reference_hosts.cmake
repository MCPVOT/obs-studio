# libobs C reference: its Windows-only headers define nothing elsewhere (#876).
if(NOT OS_WINDOWS)
  add_executable(test_libobs_reference_hosts test_libobs_reference_hosts.c)
  target_include_directories(test_libobs_reference_hosts PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_libobs_reference_hosts PRIVATE ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_libobs_reference_hosts)
  add_test(test_libobs_reference_hosts ${CMAKE_CURRENT_BINARY_DIR}/test_libobs_reference_hosts)
endif()
