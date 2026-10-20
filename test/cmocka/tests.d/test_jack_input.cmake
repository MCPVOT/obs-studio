# linux-jack: an unchanged settings update must not reopen the JACK client,
# and the fallback timestamp must use the server's sample rate (split from
# obs-rust/obs-studio#1464). The real jack-input.c and jack-wrapper.c are
# compiled into the test against jack-stubs/jack/jack.h and fake libjack
# functions, so JACK itself (ENABLE_JACK is off by default) is not needed.
if(OS_LINUX)
  add_executable(test_jack_input test_jack_input.c)
  target_include_directories(
    test_jack_input
    PRIVATE
      ${CMOCKA_INCLUDE_DIR}
      "${CMAKE_CURRENT_SOURCE_DIR}/jack-stubs"
      "${CMAKE_SOURCE_DIR}"
      "${CMAKE_SOURCE_DIR}/plugins/linux-jack"
  )
  target_compile_options(test_jack_input PRIVATE $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-error=sign-compare>)
  target_link_libraries(test_jack_input PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  target_link_options(
    test_jack_input
    PRIVATE "LINKER:--wrap=obs_source_get_name,--wrap=obs_source_set_async_compensation,--wrap=obs_source_output_audio"
  )
  add_dependencies(cmocka-tests test_jack_input)
  add_test(test_jack_input ${CMAKE_CURRENT_BINARY_DIR}/test_jack_input)
  set_tests_properties(test_jack_input PROPERTIES TIMEOUT 60)
endif()
