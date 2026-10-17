# The crash sentinel directory must be created on a first launch, before
# MakeUserDirs has made its parent (obs-rust/obs-studio#154). Builds the Qt-free
# helper CrashHandler calls.
add_executable(
  test_crash_sentinel_dir
  test_crash_sentinel_dir.cpp
  "${CMAKE_SOURCE_DIR}/frontend/utility/crash-sentinel-dir.cpp.hpp"
)
target_include_directories(test_crash_sentinel_dir PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/frontend/utility")
target_compile_features(test_crash_sentinel_dir PRIVATE cxx_std_17)
target_link_libraries(test_crash_sentinel_dir PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
add_dependencies(cmocka-tests test_crash_sentinel_dir)
add_test(test_crash_sentinel_dir ${CMAKE_CURRENT_BINARY_DIR}/test_crash_sentinel_dir)
