# rtmp-services ingest refresh wait (#1284): the header-only helper shared by
# service-ingest and dacast, against libobs threading.
add_executable(test_ingest_refresh_wait test_ingest_refresh_wait.c)
target_include_directories(test_ingest_refresh_wait PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
target_link_libraries(test_ingest_refresh_wait PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
add_dependencies(cmocka-tests test_ingest_refresh_wait)
add_test(test_ingest_refresh_wait ${CMAKE_CURRENT_BINARY_DIR}/test_ingest_refresh_wait)
set_tests_properties(test_ingest_refresh_wait PROPERTIES TIMEOUT 60)
