# Exercise the original private buffering helper without a display or clocks.
# Linux section GC discards unrelated functions from the included core source.
if(OS_LINUX)
  add_executable(test_audio_buffering_subsample test_audio_buffering_subsample.c)
  target_include_directories(test_audio_buffering_subsample PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(
    test_audio_buffering_subsample
    PRIVATE OBS::libobs OBS::caption Uthash::Uthash ZLIB::ZLIB ${CMOCKA_LIBRARIES} m
  )
  target_compile_options(test_audio_buffering_subsample PRIVATE -ffunction-sections -fdata-sections)
  target_link_options(test_audio_buffering_subsample PRIVATE -Wl,--gc-sections)
  include(CheckCCompilerFlag)
  check_c_compiler_flag(-ftrivial-auto-var-init=pattern HAVE_C_TRIVIAL_AUTO_VAR_INIT_PATTERN)
  if(HAVE_C_TRIVIAL_AUTO_VAR_INIT_PATTERN)
    target_compile_options(test_audio_buffering_subsample PRIVATE -ftrivial-auto-var-init=pattern)
  endif()
  add_dependencies(cmocka-tests test_audio_buffering_subsample)
  add_test(NAME test_audio_buffering_subsample COMMAND test_audio_buffering_subsample)
endif()
