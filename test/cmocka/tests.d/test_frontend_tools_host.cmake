# plugins/frontend-tools: the POSIX and macOS hosts' Python settings and
# Python.framework path trim (#855). Each compiles one host file on its own;
# the macOS one is plain C, so it builds and runs on Linux too.
if(NOT OS_WINDOWS)
  foreach(_test IN ITEMS test_frontend_tools_host test_frontend_tools_host_macos)
    add_executable(${_test} ${_test}.c)
    target_include_directories(${_test} PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
    target_link_libraries(${_test} PRIVATE ${CMOCKA_LIBRARIES})
    add_dependencies(cmocka-tests ${_test})
    add_test(${_test} ${CMAKE_CURRENT_BINARY_DIR}/${_test})
  endforeach()
endif()
