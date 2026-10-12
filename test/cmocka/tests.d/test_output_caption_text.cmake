# Caption text pagination: obs_output_output_caption_text2() must word-wrap
# to 32 columns and paginate at 15 rows instead of truncating at 128 bytes.
# Reads the caption queue through obs-internal.h, so it needs the private
# libobs and libcaption include dirs.
add_executable(test_output_caption_text test_output_caption_text.c)
target_include_directories(
  test_output_caption_text
  PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/libobs" "${CMAKE_SOURCE_DIR}/deps/libcaption"
)
target_link_libraries(test_output_caption_text PRIVATE OBS::libobs OBS::caption ${CMOCKA_LIBRARIES})

add_dependencies(cmocka-tests test_output_caption_text)
add_test(test_output_caption_text ${CMAKE_CURRENT_BINARY_DIR}/test_output_caption_text)
