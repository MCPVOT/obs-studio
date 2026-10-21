# Tools > Remux: the queue model told views the insertion row went away (or
# came back) before rowCount() changed, and sent dataChanged ranges one past
# the last column and row. Builds the real utility/RemuxQueueModel.cpp against
# a stand-in for the frontend's precompiled header (remux-queue-stubs/) and
# runs it on Qt's offscreen platform: Qt Widgets, no display, no frontend.
find_package(Qt6 QUIET COMPONENTS Widgets)
if(TARGET Qt6::Widgets)
  add_executable(
    test_remux_queue_model
    test_remux_queue_model.cpp
    "${CMAKE_SOURCE_DIR}/frontend/utility/RemuxQueueModel.cpp.hpp"
    "${CMAKE_SOURCE_DIR}/frontend/utility/RemuxQueueModel.hpp"
  )
  set_target_properties(test_remux_queue_model PROPERTIES AUTOMOC ON)
  target_include_directories(
    test_remux_queue_model
    BEFORE
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/remux-queue-stubs" ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}"
  )
  target_compile_features(test_remux_queue_model PRIVATE cxx_std_17)
  target_link_libraries(test_remux_queue_model PRIVATE Qt6::Widgets ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_remux_queue_model)
  add_test(test_remux_queue_model ${CMAKE_CURRENT_BINARY_DIR}/test_remux_queue_model)
  # Runs in well under a second; a stuck Qt start fails the test, not the CI job.
  set_tests_properties(test_remux_queue_model PROPERTIES TIMEOUT 120)
  cmocka_qt_runtime(test_remux_queue_model Qt6::Core)
endif()
