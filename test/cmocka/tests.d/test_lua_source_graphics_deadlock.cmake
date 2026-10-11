# Lua video source scripts calling obs_enter_graphics deadlocked against the
# graphics thread (obsproject/obs-studio#6674). Loads a real Lua script through
# obs-scripting, so it needs Lua scripting support. Needs core video, so it
# runs (under xvfb-run) on Linux and skips without a display.
if(OS_LINUX AND TARGET OBS::libobs-opengl AND TARGET OBS::scripting AND TARGET OBS::lua)
  add_executable(test_lua_source_graphics_deadlock test_lua_source_graphics_deadlock.c)
  target_compile_definitions(
    test_lua_source_graphics_deadlock
    PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>"
  )
  target_include_directories(test_lua_source_graphics_deadlock PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_lua_source_graphics_deadlock PRIVATE OBS::libobs OBS::scripting ${CMOCKA_LIBRARIES})
  # The script calls back into the test through the LuaJIT FFI.
  set_target_properties(test_lua_source_graphics_deadlock PROPERTIES ENABLE_EXPORTS TRUE)
  add_dependencies(test_lua_source_graphics_deadlock libobs-opengl obslua)
  add_dependencies(cmocka-tests test_lua_source_graphics_deadlock)
  # libobs finds its effect files, and the script finds obslua, relative to
  # the working directory.
  add_test(
    NAME test_lua_source_graphics_deadlock
    COMMAND test_lua_source_graphics_deadlock
    WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
  )
  set_tests_properties(test_lua_source_graphics_deadlock PROPERTIES TIMEOUT 180)
endif()
