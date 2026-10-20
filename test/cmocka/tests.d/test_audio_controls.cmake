# Characterization of libobs/obs-audio-controls.c.h before its port (#1488).
add_executable(test_audio_controls test_audio_controls.c)
target_include_directories(test_audio_controls PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(test_audio_controls PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})

add_dependencies(cmocka-tests test_audio_controls)
# obs_startup needs an X display on Linux.
find_program(XVFB_RUN xvfb-run)
if(XVFB_RUN)
  add_test(test_audio_controls ${XVFB_RUN} -a ${CMAKE_CURRENT_BINARY_DIR}/test_audio_controls)
else()
  add_test(test_audio_controls ${CMAKE_CURRENT_BINARY_DIR}/test_audio_controls)
endif()
