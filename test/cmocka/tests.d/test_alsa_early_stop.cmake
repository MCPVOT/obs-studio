# linux-alsa: a stop that comes before the capture thread starts must not be
# overwritten by the thread, or destroying the source hangs (split from
# obs-rust/obs-studio#1464). pthread_create/pthread_join are wrapped to hold
# the capture thread until the stop is joining it; the capture code runs on
# ALSA's null PCM, so no sound card or display is needed.
if(TARGET OBS::alsa)
  find_package(ALSA REQUIRED)

  add_executable(test_alsa_early_stop test_alsa_early_stop.c "${CMAKE_SOURCE_DIR}/plugins/linux-alsa/alsa-input.c.h")
  target_include_directories(test_alsa_early_stop PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_alsa_early_stop PRIVATE OBS::libobs ALSA::ALSA ${CMOCKA_LIBRARIES})
  target_link_options(
    test_alsa_early_stop
    PRIVATE
      "LINKER:--wrap=obs_source_set_async_compensation,--wrap=obs_source_output_audio,--wrap=pthread_create,--wrap=pthread_join"
  )
  add_dependencies(cmocka-tests test_alsa_early_stop)
  add_test(test_alsa_early_stop ${CMAKE_CURRENT_BINARY_DIR}/test_alsa_early_stop)
  set_tests_properties(test_alsa_early_stop PROPERTIES TIMEOUT 60)
endif()
