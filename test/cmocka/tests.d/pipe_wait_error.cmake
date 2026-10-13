# Seed the status slot around the real waitpid to make a failed-wait status
# read deterministic. The SIGCHLD/ECHILD path uses a real Linux child.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(test_pipe_wait_error test_pipe_wait_error.c "${CMAKE_SOURCE_DIR}/libobs/util/pipe-posix.c")
  target_link_libraries(test_pipe_wait_error PRIVATE OBS::libobs)
  target_link_options(test_pipe_wait_error PRIVATE "LINKER:--wrap=waitpid")
  add_dependencies(cmocka-tests test_pipe_wait_error)
  add_test(test_pipe_wait_error ${CMAKE_CURRENT_BINARY_DIR}/test_pipe_wait_error)
  set_tests_properties(test_pipe_wait_error PROPERTIES TIMEOUT 30)
endif()
