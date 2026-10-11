# AMF3 class-name and member-name logging, through librtmp's decoder under
# ASan: both names are length-prefixed AVals, and logging one with %s reads
# past the allocation end.
if(TARGET OBS::outputs)
  add_executable(
    test_amf3_classname
    test_amf3_classname.c
    "${CMAKE_SOURCE_DIR}/plugins/obs-outputs/librtmp/amf.c"
    "${CMAKE_SOURCE_DIR}/plugins/obs-outputs/librtmp/log.c"
  )
  target_compile_definitions(test_amf3_classname PRIVATE NO_CRYPTO)
  target_include_directories(
    test_amf3_classname
    PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/plugins/obs-outputs"
  )
  target_link_libraries(test_amf3_classname PRIVATE OBS::libobs ${CMOCKA_LIBRARIES} $<$<PLATFORM_ID:Windows>:ws2_32>)
  if(NOT MSVC)
    target_compile_options(test_amf3_classname PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(test_amf3_classname PRIVATE -fsanitize=address)
  endif()
  add_dependencies(cmocka-tests test_amf3_classname)
  add_test(test_amf3_classname ${CMAKE_CURRENT_BINARY_DIR}/test_amf3_classname)
endif()
