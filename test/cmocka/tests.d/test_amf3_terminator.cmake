# AMF3 dynamic-member terminator handling, through librtmp's decoder under
# ASan: on master the empty-name terminator is appended as a property and the
# stale union pointer from the preceding member is then freed twice.
if(TARGET OBS::outputs)
  add_executable(
    test_amf3_terminator
    test_amf3_terminator.c
    "${CMAKE_SOURCE_DIR}/plugins/obs-outputs/librtmp/amf.c"
    "${CMAKE_SOURCE_DIR}/plugins/obs-outputs/librtmp/log.c"
  )
  target_compile_definitions(test_amf3_terminator PRIVATE NO_CRYPTO)
  target_include_directories(
    test_amf3_terminator
    PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/plugins/obs-outputs"
  )
  target_link_libraries(test_amf3_terminator PRIVATE OBS::libobs ${CMOCKA_LIBRARIES} $<$<PLATFORM_ID:Windows>:ws2_32>)
  if(NOT MSVC)
    obs_test_sanitize(test_amf3_terminator address)
  endif()
  add_dependencies(cmocka-tests test_amf3_terminator)
  add_test(test_amf3_terminator ${CMAKE_CURRENT_BINARY_DIR}/test_amf3_terminator)
endif()
