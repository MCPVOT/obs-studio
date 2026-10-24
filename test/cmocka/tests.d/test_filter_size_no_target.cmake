# A filter's size asked while it has a parent but no target, from inside
# obs_source_restore_filters() (#2172). obs_startup() needs a display on
# Linux, so there the case skips without one.
add_executable(test_filter_size_no_target test_filter_size_no_target.c)
target_include_directories(test_filter_size_no_target PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(test_filter_size_no_target PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
add_dependencies(cmocka-tests test_filter_size_no_target)
add_test(test_filter_size_no_target ${CMAKE_CURRENT_BINARY_DIR}/test_filter_size_no_target)
