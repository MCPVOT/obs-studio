if(NOT ENABLE_HEVC)
  file(READ "${_obs_rust_export_map}" _exports)
  string(
    REPLACE
    "local:"
    "local:\n    obs_hevc_keyframe;\n    obs_parse_hevc_packet;\n    obs_parse_hevc_packet_priority;\n    obs_extract_hevc_headers;"
    _exports
    "${_exports}"
  )
  set(_obs_rust_export_map "${CMAKE_CURRENT_BINARY_DIR}/rust-exports-no-hevc.map")
  file(CONFIGURE OUTPUT "${_obs_rust_export_map}" CONTENT "${_exports}" @ONLY)
endif()

target_link_options(${OBS_LIBOBS_RUST_TARGET} PRIVATE "LINKER:--version-script=${_obs_rust_export_map}")
set_property(TARGET ${OBS_LIBOBS_RUST_TARGET} APPEND PROPERTY LINK_DEPENDS "${_obs_rust_export_map}")
# The obs-data glue calls json_delete from inside the staticlib.
# jansson is first linked earlier, in libobs/CMakeLists.txt. On
# Ubuntu, GNU ld --as-needed drops that first occurrence because
# nothing references jansson yet, and every cmocka executable then
# fails to link. Link it again so a later occurrence follows the
# archive.
if(OS_LINUX)
  target_link_libraries(${OBS_LIBOBS_RUST_TARGET} PRIVATE jansson::jansson)
endif()
