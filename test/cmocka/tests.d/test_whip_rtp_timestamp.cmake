# WHIP output RTP timestamps (upstream obsproject/obs-studio#13970). The
# helper is header-only, so this runs without libdatachannel.
add_executable(test_whip_rtp_timestamp test_whip_rtp_timestamp.c)
target_include_directories(
  test_whip_rtp_timestamp
  PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/plugins/obs-webrtc"
)
target_link_libraries(test_whip_rtp_timestamp PRIVATE ${CMOCKA_LIBRARIES} $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
add_dependencies(cmocka-tests test_whip_rtp_timestamp)
add_test(test_whip_rtp_timestamp ${CMAKE_CURRENT_BINARY_DIR}/test_whip_rtp_timestamp)
