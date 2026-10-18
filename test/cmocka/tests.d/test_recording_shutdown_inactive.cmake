# Post-deactivate inactivity regression (#1391); real Qt event loop, no display.
if(OS_LINUX)
  find_package(Qt6 QUIET COMPONENTS Core)
  if(TARGET Qt6::Core)
    add_executable(test_recording_shutdown_inactive test_recording_shutdown_inactive.cpp)
    target_include_directories(
      test_recording_shutdown_inactive
      PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/frontend/widgets"
    )
    target_link_libraries(test_recording_shutdown_inactive PRIVATE OBS::libobs Qt6::Core ${CMOCKA_LIBRARIES})
    add_dependencies(cmocka-tests test_recording_shutdown_inactive)
    add_test(test_recording_shutdown_inactive ${CMAKE_CURRENT_BINARY_DIR}/test_recording_shutdown_inactive)
    set_tests_properties(test_recording_shutdown_inactive PROPERTIES TIMEOUT 15)
  endif()
endif()
