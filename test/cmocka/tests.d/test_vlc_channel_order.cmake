# VLC Video Source surround channel order (upstream obsproject/obs-studio#12579).
# The end-to-end cases load the built vlc-video module, need core video (run
# under xvfb-run) and skip without a display or when libVLC is not installed;
# the test then exits with 77 and ctest reports it as skipped.
if(OS_LINUX AND TARGET vlc-video AND TARGET OBS::libobs-opengl)
  add_executable(test_vlc_channel_order test_vlc_channel_order.c)
  target_compile_definitions(
    test_vlc_channel_order
    PRIVATE
      VLC_MODULE_PATH="$<TARGET_FILE:vlc-video>"
      GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>"
      VLC_DATA_PATH="${CMAKE_SOURCE_DIR}/plugins/vlc-video/data"
  )
  target_include_directories(
    test_vlc_channel_order
    PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/plugins/vlc-video"
  )
  target_link_libraries(
    test_vlc_channel_order
    PRIVATE OBS::libobs ${CMOCKA_LIBRARIES} $<$<NOT:$<PLATFORM_ID:Windows>>:m>
  )
  add_dependencies(test_vlc_channel_order vlc-video libobs-opengl)
  add_dependencies(cmocka-tests test_vlc_channel_order)
  # libobs finds its effect files relative to the working directory.
  add_test(NAME test_vlc_channel_order COMMAND test_vlc_channel_order WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>")
  set_tests_properties(test_vlc_channel_order PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
endif()
