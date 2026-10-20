# Preserve the original C characterization unchanged after the intentional
# #1275 behavior correction. This target now exercises the frozen baseline;
# test_lua_graphics_lock_bounded compiles and tests the production helper.
configure_file(
  "${CMAKE_SOURCE_DIR}/test/cmocka/fixtures/lua-graphics-lock-original.h"
  "${CMAKE_CURRENT_BINARY_DIR}/lua-graphics-lock-under-test.h"
  COPYONLY
)

add_executable(test_lua_graphics_lock test_lua_graphics_lock.c)
target_include_directories(test_lua_graphics_lock PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(test_lua_graphics_lock PRIVATE ${CMOCKA_LIBRARIES})
add_dependencies(cmocka-tests test_lua_graphics_lock)
add_test(NAME test_lua_graphics_lock COMMAND test_lua_graphics_lock)
set_tests_properties(test_lua_graphics_lock PROPERTIES TIMEOUT 10)
