# libobs/graphics: the Linux entry points in graphics-functions-linux.h stay
# exported (#872).
if(OS_LINUX)
  add_executable(test_graphics_host_functions test_graphics_host_functions.c)
  target_include_directories(test_graphics_host_functions PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_graphics_host_functions PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_graphics_host_functions)
  add_test(test_graphics_host_functions ${CMAKE_CURRENT_BINARY_DIR}/test_graphics_host_functions)
endif()
