# linux-v4l2: the virtual camera continues a frame after a short write and
# retries an interrupted one (split from obs-rust/obs-studio#1464). Runs the
# real v4l2-output.c with write() wrapped, so no v4l2loopback device is needed.
if(OS_LINUX AND TARGET OBS::v4l2)
  add_executable(test_v4l2_vcam_write test_v4l2_vcam_write.c "${CMAKE_SOURCE_DIR}/plugins/linux-v4l2/vcam-format.c.h")
  target_include_directories(
    test_v4l2_vcam_write
    PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/plugins/linux-v4l2"
  )
  target_link_libraries(test_v4l2_vcam_write PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  target_link_options(test_v4l2_vcam_write PRIVATE "LINKER:--wrap=write")
  add_dependencies(cmocka-tests test_v4l2_vcam_write)
  add_test(test_v4l2_vcam_write ${CMAKE_CURRENT_BINARY_DIR}/test_v4l2_vcam_write)
  set_tests_properties(test_v4l2_vcam_write PROPERTIES TIMEOUT 60)
endif()
