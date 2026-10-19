# Characterization of libobs/callback/proc.c.h before its port (#1440).
add_executable(test_proc test_proc.c)
target_include_directories(test_proc PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(test_proc PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})

add_dependencies(cmocka-tests test_proc)
add_test(test_proc ${CMAKE_CURRENT_BINARY_DIR}/test_proc)
