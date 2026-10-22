# Scene group transform (a point through a matrix, and the scale/rotation
# fold). Builds libobs/obs-scene-group-transform.c.h on its own: no display
# and no libobs core, so it runs on every platform. The symbols are not
# exported, so this characterizes that C source.
add_executable(
  test_scene_group_transform
  test_scene_group_transform.c
  "${CMAKE_SOURCE_DIR}/libobs/obs-scene-group-transform.c.h"
)
target_include_directories(test_scene_group_transform PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(test_scene_group_transform PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
if(NOT MSVC)
  target_link_libraries(test_scene_group_transform PRIVATE m)
endif()

add_dependencies(cmocka-tests test_scene_group_transform)
add_test(test_scene_group_transform ${CMAKE_CURRENT_BINARY_DIR}/test_scene_group_transform)
