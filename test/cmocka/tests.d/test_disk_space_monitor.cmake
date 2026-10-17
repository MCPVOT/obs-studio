# Free disk space is queried off the UI thread (obs-rust/obs-studio#1282):
# request() and latest() return at once while the free-space query blocks.
# Builds the Qt-free helper the recording timer and the stats window call.
find_package(Threads REQUIRED)
add_executable(
  test_disk_space_monitor
  test_disk_space_monitor.cpp
  "${CMAKE_SOURCE_DIR}/frontend/utility/disk-space-monitor.cpp.hpp"
)
target_include_directories(
  test_disk_space_monitor
  PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/frontend/utility"
)
target_compile_features(test_disk_space_monitor PRIVATE cxx_std_17)
target_link_libraries(test_disk_space_monitor PRIVATE OBS::libobs Threads::Threads ${CMOCKA_LIBRARIES})
add_dependencies(cmocka-tests test_disk_space_monitor)
add_test(test_disk_space_monitor ${CMAKE_CURRENT_BINARY_DIR}/test_disk_space_monitor)
