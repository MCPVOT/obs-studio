# Double-rate deinterlacing must show the fields of an interlaced source in
# capture order for both field orders (upstream obsproject/obs-studio#6061),
# also when the field rate differs from the canvas rate
# (upstream obsproject/obs-studio#3402).
# Needs core video, so it runs (under xvfb-run) on Linux and skips without a
# display.
if(OS_LINUX AND TARGET OBS::libobs-opengl)
  add_executable(test_deinterlace_2x_field_order test_deinterlace_2x_field_order.c)
  target_compile_definitions(
    test_deinterlace_2x_field_order
    PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>"
  )
  target_include_directories(test_deinterlace_2x_field_order PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_deinterlace_2x_field_order PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(test_deinterlace_2x_field_order libobs-opengl)
  add_dependencies(cmocka-tests test_deinterlace_2x_field_order)
  # One entry per rate (cmocka's CMOCKA_TEST_FILTER, every case name ends in
  # its rate), so CI can spread them over runners (#2072);
  # ci/test_deinterlace_schedule.py checks every case's rate is listed.
  foreach(
    rate
    IN
    ITEMS 60i_into_60 50i_into_50 50i_into_60 50i_into_30 60i_into_50
  )
    # libobs finds its effect files relative to the working directory.
    add_test(
      NAME test_deinterlace_2x_field_order_${rate}
      COMMAND test_deinterlace_2x_field_order
      WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
    )
    # Even with bounded renderer workers, concurrent CI dropped fields (#1643).
    # Keep this real-time assertion isolated; the other tests can overlap.
    set_tests_properties(
      test_deinterlace_2x_field_order_${rate}
      PROPERTIES RUN_SERIAL TRUE TIMEOUT 120 ENVIRONMENT "CMOCKA_TEST_FILTER=*_${rate}"
    )
  endforeach()
endif()
