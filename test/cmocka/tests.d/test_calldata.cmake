# Characterization of libobs/callback/calldata.c.h before its port (#1439).
add_executable(test_calldata test_calldata.c)
target_include_directories(test_calldata PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(test_calldata PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})

add_dependencies(cmocka-tests test_calldata)
add_test(test_calldata ${CMAKE_CURRENT_BINARY_DIR}/test_calldata)
