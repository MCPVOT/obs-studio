# One unmappable character must not drop the rest of its caption row
# (obs-rust/obs-studio#376). Round-trips text through the vendored libcaption
# the way obs-output.c emits captions; needs no display.
add_executable(test_caption_unmappable test_caption_unmappable.c)
target_include_directories(test_caption_unmappable PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/deps/libcaption")
target_link_libraries(test_caption_unmappable PRIVATE OBS::caption ${CMOCKA_LIBRARIES})

add_dependencies(cmocka-tests test_caption_unmappable)
add_test(test_caption_unmappable ${CMAKE_CURRENT_BINARY_DIR}/test_caption_unmappable)
