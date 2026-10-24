# gs_draw_sprite() and a full-size gs_draw_sprite_subregion() must honour
# GS_FLIP_U and GS_FLIP_V each on its own axis (obs-rust/obs-studio#2124:
# GS_FLIP_U drew a 2D texture upside down). Needs core video, so it runs
# (under xvfb-run) on Linux and skips without a display.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_sprite_flip_render test_sprite_flip_render.c)
  target_compile_definitions(test_sprite_flip_render PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>")
  target_include_directories(test_sprite_flip_render PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_sprite_flip_render PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(test_sprite_flip_render libobs-opengl)
  add_dependencies(cmocka-tests test_sprite_flip_render)
  # libobs finds its effect files relative to the working directory.
  add_test(NAME test_sprite_flip_render COMMAND test_sprite_flip_render WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>")
  set_tests_properties(test_sprite_flip_render PROPERTIES TIMEOUT 120)
endif()
