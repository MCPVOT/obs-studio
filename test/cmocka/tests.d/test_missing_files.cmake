# Characterization of libobs/obs-missing-files.c.h before its port (#1459).
add_executable(test_missing_files test_missing_files.c)
target_include_directories(test_missing_files PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(test_missing_files PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})

add_dependencies(cmocka-tests test_missing_files)
# The source-name case calls obs_startup, which needs an X display on Linux.
find_program(XVFB_RUN xvfb-run)
if(XVFB_RUN)
  add_test(test_missing_files ${XVFB_RUN} -a ${CMAKE_CURRENT_BINARY_DIR}/test_missing_files)
else()
  add_test(test_missing_files ${CMAKE_CURRENT_BINARY_DIR}/test_missing_files)
endif()
