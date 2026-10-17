# A fresh profile has no legacy basic/scenes.json to migrate, so startup must
# not log a failed rename (obs-rust/obs-studio#155). Builds the Qt-free helper
# the frontend calls.
add_executable(
  test_scene_collection_migration
  test_scene_collection_migration.cpp
  "${CMAKE_SOURCE_DIR}/frontend/utility/scene-collection-migration.cpp.hpp"
)
target_include_directories(
  test_scene_collection_migration
  PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/frontend/utility"
)
target_compile_features(test_scene_collection_migration PRIVATE cxx_std_17)
target_link_libraries(test_scene_collection_migration PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
add_dependencies(cmocka-tests test_scene_collection_migration)
add_test(test_scene_collection_migration ${CMAKE_CURRENT_BINARY_DIR}/test_scene_collection_migration)
