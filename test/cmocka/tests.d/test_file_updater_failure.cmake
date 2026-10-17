# shared/file-updater reports a failed single-file download, so an
# rtmp-services ingest refresh wait ends at once instead of at its timeout
# (obs-rust/obs-studio#1347). Uses localhost sockets; Linux only like the
# other network-touching tests.
if(OS_LINUX)
  if(NOT TARGET OBS::file-updater)
    add_subdirectory("${CMAKE_SOURCE_DIR}/shared/file-updater" "${CMAKE_BINARY_DIR}/shared/file-updater")
  endif()
  add_executable(test_file_updater_failure test_file_updater_failure.c)
  target_include_directories(test_file_updater_failure PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_file_updater_failure PRIVATE OBS::libobs OBS::file-updater ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_file_updater_failure)
  add_test(test_file_updater_failure ${CMAKE_CURRENT_BINARY_DIR}/test_file_updater_failure)
  set_tests_properties(test_file_updater_failure PROPERTIES TIMEOUT 60)
endif()
