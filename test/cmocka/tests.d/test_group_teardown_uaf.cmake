# Scene/group teardown must not free a scene item after its source (a use
# after free that crashed Windows). Needs no graphics; skips if obs_startup
# fails. Run it under ASan/valgrind to see the report.
add_executable(test_group_teardown_uaf test_group_teardown_uaf.c)
target_include_directories(test_group_teardown_uaf PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(test_group_teardown_uaf PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
add_dependencies(cmocka-tests test_group_teardown_uaf)
add_test(test_group_teardown_uaf ${CMAKE_CURRENT_BINARY_DIR}/test_group_teardown_uaf)
