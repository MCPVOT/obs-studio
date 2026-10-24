# plugins/obs-ffmpeg: the module's Linux and POSIX hosts (#853). Each
# executable compiles one host file against libobs's util.
if(NOT OS_WINDOWS)
  foreach(_host IN ITEMS linux posix)
    set(_test test_obs_ffmpeg_host_${_host})
    add_executable(${_test} test_obs_ffmpeg_host.c)
    target_include_directories(${_test} PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
    target_compile_definitions(
      ${_test}
      PRIVATE OBS_FFMPEG_HOST_SOURCE="plugins/obs-ffmpeg/obs-ffmpeg-host-${_host}.c.h"
    )
    target_link_libraries(${_test} PRIVATE ${CMOCKA_LIBRARIES} OBS::libobs)
    add_dependencies(cmocka-tests ${_test})
    add_test(${_test} ${CMAKE_CURRENT_BINARY_DIR}/${_test})
  endforeach()
endif()
