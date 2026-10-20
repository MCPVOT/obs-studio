# plugins/obs-ffmpeg: a missing FFmpeg video encoder reports its error
# through a zero-initialized dstr (split from #1464). Includes
# obs-ffmpeg-video-encoders.c.h; no display or OBS core is needed.
if(OS_LINUX)
  add_executable(
    test_ffmpeg_encoder_missing
    test_ffmpeg_encoder_missing.c
    "${CMAKE_SOURCE_DIR}/shared/opts-parser/opts-parser.c.h"
  )
  target_include_directories(
    test_ffmpeg_encoder_missing
    PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/shared/opts-parser"
  )
  target_link_libraries(
    test_ffmpeg_encoder_missing
    PRIVATE OBS::libobs FFmpeg::avcodec FFmpeg::avformat FFmpeg::avutil ${CMOCKA_LIBRARIES}
  )
  add_dependencies(cmocka-tests test_ffmpeg_encoder_missing)
  add_test(test_ffmpeg_encoder_missing ${CMAKE_CURRENT_BINARY_DIR}/test_ffmpeg_encoder_missing)
  set_tests_properties(test_ffmpeg_encoder_missing PROPERTIES TIMEOUT 60)
endif()
