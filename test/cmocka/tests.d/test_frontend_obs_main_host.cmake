# Frontend obs-main: its Windows-only header defines nothing elsewhere (#865).
if(NOT OS_WINDOWS)
  add_executable(test_frontend_obs_main_host test_frontend_obs_main_host.c)
  target_include_directories(test_frontend_obs_main_host PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_frontend_obs_main_host PRIVATE ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_frontend_obs_main_host)
  add_test(test_frontend_obs_main_host ${CMAKE_CURRENT_BINARY_DIR}/test_frontend_obs_main_host)
endif()
