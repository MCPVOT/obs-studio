# Placement half of calculate_bounds_data (alignment, crop, flip). Builds
# libobs/obs-scene-bounds-place.c.h and the scale half it calls, on their own:
# no display and no libobs core, so it runs on every platform. The symbols are
# not exported, so this characterizes that C source.
add_executable(
  test_scene_bounds_place
  test_scene_bounds_place.c
  "${CMAKE_SOURCE_DIR}/libobs/obs-scene-bounds-place.c.h"
  "${CMAKE_SOURCE_DIR}/libobs/obs-scene-bounds.c.h"
)
target_include_directories(test_scene_bounds_place PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(test_scene_bounds_place PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
if(NOT MSVC)
  target_link_libraries(test_scene_bounds_place PRIVATE m)
endif()

add_dependencies(cmocka-tests test_scene_bounds_place)
add_test(test_scene_bounds_place ${CMAKE_CURRENT_BINARY_DIR}/test_scene_bounds_place)
