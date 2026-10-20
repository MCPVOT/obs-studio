# Characterize the private C helper without Lua, a display or a graphics backend.
# Extract the production function unchanged, not a handwritten copy. Reconfigure
# when its source changes; fail closed if the boundaries are renamed or moved.
set(_lua_lock_source "${CMAKE_SOURCE_DIR}/shared/obs-scripting/obs-scripting-lua-source.c.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_lua_lock_source}")
file(READ "${_lua_lock_source}" _lua_lock_text)
string(FIND "${_lua_lock_text}" "static bool lock_in_graphics(pthread_mutex_t *mutex)" _lua_lock_start)
string(FIND "${_lua_lock_text}" "#define lock_script_in_graphics(skip)" _lua_lock_end)
if(_lua_lock_start LESS 0 OR _lua_lock_end LESS_EQUAL _lua_lock_start)
  message(FATAL_ERROR "Cannot locate production lock_in_graphics for characterization")
endif()
math(EXPR _lua_lock_length "${_lua_lock_end} - ${_lua_lock_start}")
string(SUBSTRING "${_lua_lock_text}" ${_lua_lock_start} ${_lua_lock_length} _lua_lock_function)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/lua-graphics-lock-under-test.h" "${_lua_lock_function}")

add_executable(test_lua_graphics_lock test_lua_graphics_lock.c)
target_include_directories(test_lua_graphics_lock PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(test_lua_graphics_lock PRIVATE ${CMOCKA_LIBRARIES})
add_dependencies(cmocka-tests test_lua_graphics_lock)
add_test(NAME test_lua_graphics_lock COMMAND test_lua_graphics_lock)
set_tests_properties(test_lua_graphics_lock PROPERTIES TIMEOUT 10)
