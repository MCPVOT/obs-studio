# plugins/obs-ffmpeg: the muxer output's macOS and POSIX hosts (#853). Each
# executable compiles one host file; both are plain C, so they build and run
# on every non-Windows test host.
if(NOT OS_WINDOWS)
  foreach(_host IN ITEMS macos posix)
    set(_test test_obs_ffmpeg_mux_host_${_host})
    add_executable(${_test} test_obs_ffmpeg_mux_host.c)
    target_include_directories(${_test} PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
    target_compile_definitions(
      ${_test}
      PRIVATE OBS_FFMPEG_MUX_HOST_SOURCE="plugins/obs-ffmpeg/obs-ffmpeg-mux-host-${_host}.c.h"
    )
    target_link_libraries(${_test} PRIVATE ${CMOCKA_LIBRARIES})
    add_dependencies(cmocka-tests ${_test})
    add_test(${_test} ${CMAKE_CURRENT_BINARY_DIR}/${_test})
  endforeach()
endif()
