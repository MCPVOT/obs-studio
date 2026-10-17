# linux-capture: the XComposite capture backs off its window search while the
# window is unmapped (obs-rust/obs-studio#1357). Pure decision helpers, no X.
add_executable(test_xcomposite_search_backoff test_xcomposite_search_backoff.c)
target_include_directories(test_xcomposite_search_backoff PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
target_link_libraries(test_xcomposite_search_backoff PRIVATE ${CMOCKA_LIBRARIES})
add_dependencies(cmocka-tests test_xcomposite_search_backoff)
add_test(test_xcomposite_search_backoff ${CMAKE_CURRENT_BINARY_DIR}/test_xcomposite_search_backoff)
set_tests_properties(test_xcomposite_search_backoff PROPERTIES TIMEOUT 30)
