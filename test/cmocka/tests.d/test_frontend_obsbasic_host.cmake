# Main window: the host headers' platform features on Linux (#871).
if(OS_LINUX)
  add_executable(test_frontend_obsbasic_host test_frontend_obsbasic_host.c)
  target_include_directories(test_frontend_obsbasic_host PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_frontend_obsbasic_host PRIVATE ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_frontend_obsbasic_host)
  add_test(test_frontend_obsbasic_host ${CMAKE_CURRENT_BINARY_DIR}/test_frontend_obsbasic_host)
endif()
