# Duplicated scenes (Studio Mode program, "Duplicate Scene", ungroup) must draw
# items at the same size as the original (obsproject/obs-studio#12171). Renders
# scenes, so this needs core video and runs (under xvfb-run) on Linux only.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_scene_duplicate_scale_ref test_scene_duplicate_scale_ref.c)
  target_compile_definitions(
    test_scene_duplicate_scale_ref
    PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>"
  )
  target_include_directories(test_scene_duplicate_scale_ref PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_scene_duplicate_scale_ref PRIVATE OBS::libobs ${CMOCKA_LIBRARIES} m)
  add_dependencies(test_scene_duplicate_scale_ref libobs-opengl)
  add_dependencies(cmocka-tests test_scene_duplicate_scale_ref)
  # libobs finds its effect files relative to the working directory.
  add_test(
    NAME test_scene_duplicate_scale_ref
    COMMAND test_scene_duplicate_scale_ref
    WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
  )
  set_tests_properties(test_scene_duplicate_scale_ref PROPERTIES TIMEOUT 120)
endif()
