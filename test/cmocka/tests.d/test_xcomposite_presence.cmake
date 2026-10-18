# linux-capture: the XComposite capture keeps its window's state from X events
# instead of a round trip per frame (obs-rust/obs-studio#1356). Pure helpers, no X.
add_executable(test_xcomposite_presence test_xcomposite_presence.c)
target_include_directories(test_xcomposite_presence PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
target_link_libraries(test_xcomposite_presence PRIVATE ${CMOCKA_LIBRARIES})
add_dependencies(cmocka-tests test_xcomposite_presence)
add_test(test_xcomposite_presence ${CMAKE_CURRENT_BINARY_DIR}/test_xcomposite_presence)
set_tests_properties(test_xcomposite_presence PROPERTIES TIMEOUT 30)
