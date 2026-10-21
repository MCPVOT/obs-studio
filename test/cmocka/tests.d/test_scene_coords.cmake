# Scene coordinate conversion (relative position and size, the half-pixel
# nudge, canvas scale). Builds libobs/obs-scene-coords.c.h on its own: no
# display and no libobs core, so it runs on every platform. The symbols are
# not exported, so this characterizes that C source.
add_executable(test_scene_coords test_scene_coords.c "${CMAKE_SOURCE_DIR}/libobs/obs-scene-coords.c.h")
target_include_directories(test_scene_coords PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(test_scene_coords PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
if(NOT MSVC)
  target_link_libraries(test_scene_coords PRIVATE m)
endif()

add_dependencies(cmocka-tests test_scene_coords)
add_test(test_scene_coords ${CMAKE_CURRENT_BINARY_DIR}/test_scene_coords)
