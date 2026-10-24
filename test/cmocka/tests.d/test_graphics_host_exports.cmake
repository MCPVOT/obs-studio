# libobs/graphics: the Linux export list in struct gs_exports and its imports
# (#872).
if(OS_LINUX)
  add_executable(test_graphics_host_exports test_graphics_host_exports.c)
  target_include_directories(
    test_graphics_host_exports
    PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/libobs"
  )
  target_link_libraries(test_graphics_host_exports PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_graphics_host_exports)
  add_test(test_graphics_host_exports ${CMAKE_CURRENT_BINARY_DIR}/test_graphics_host_exports)
endif()
