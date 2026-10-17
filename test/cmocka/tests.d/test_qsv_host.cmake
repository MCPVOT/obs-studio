# plugins/obs-qsv11: the Linux host's texture validity check (#860). Compiles
# plugins/obs-qsv11/qsv-host-linux.c.h on its own.
if(OS_LINUX)
  add_executable(test_qsv_host test_qsv_host.c)
  target_include_directories(test_qsv_host PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_qsv_host PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_qsv_host)
  add_test(test_qsv_host ${CMAKE_CURRENT_BINARY_DIR}/test_qsv_host)
endif()
