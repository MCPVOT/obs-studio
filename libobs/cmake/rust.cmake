# Link completed Rust ports into the production hybrid libobs. The C reference
# never includes this archive; the public C ABI and export set stay identical.

include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/rust-target.cmake")

# Build the staticlib for the architecture libobs targets, not the host's.
if(NOT Rust_CARGO_TARGET)
  obs_rust_cargo_target(_obs_rust_target "${OS_MACOS}" "${CMAKE_OSX_ARCHITECTURES}")
  if(_obs_rust_target)
    set(Rust_CARGO_TARGET "${_obs_rust_target}" CACHE STRING "Rust target triple for libobs-rust")
  endif()
endif()

FetchContent_Declare(Corrosion GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git GIT_TAG v0.5.2)
FetchContent_MakeAvailable(Corrosion)

# Pin the release profile so the staticlib always builds with
# panic=abort (set only for [profile.release] in Cargo.toml): without
# PROFILE, Corrosion picks dev for Debug or unset CMAKE_BUILD_TYPE, and a
# panic in a no_mangle shim would then unwind across the C ABI (UB).
corrosion_import_crate(
  MANIFEST_PATH "${CMAKE_SOURCE_DIR}/Cargo.toml"
  CRATES libobs-rust
  CRATE_TYPES staticlib
  PROFILE release
)
obs_serialize_cargo_build(libobs-rust)

# Whole-archive so every exported C symbol ends up in ${OBS_LIBOBS_RUST_TARGET}, including ones
# libobs itself never calls (e.g. bitstream_reader_r16, used by plugins).
target_link_libraries(${OBS_LIBOBS_RUST_TARGET} PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,libobs_rust-static>")

# Keep ${OBS_LIBOBS_RUST_TARGET} exporting the same symbol set as libobs:
# - ELF: Rust staticlib internals have default visibility; hide them with a
#   version script.
# - Windows: Rust no_mangle symbols in a staticlib are not dllexport, so export
#   the listed shims explicitly (otherwise its obs.dll and import lib miss them).
# - macOS: hide Rust internals with ld64 -unexported_symbols_list.
set(_obs_rust_export_map "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rust-exports.map")
set(_obs_rust_unexports "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rust-unexports-macos.txt")
if(NOT ENABLE_HEVC)
  # The aggregate archive contains codec shims even when the application turns
  # HEVC off. Keep the same public surface as that configuration's C baseline.
  if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
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
  elseif(OS_MACOS)
    file(READ "${_obs_rust_unexports}" _unexports)
    string(
      APPEND _unexports
      "\n_obs_hevc_keyframe\n_obs_parse_hevc_packet\n_obs_parse_hevc_packet_priority\n_obs_extract_hevc_headers\n"
    )
    set(_obs_rust_unexports "${CMAKE_CURRENT_BINARY_DIR}/rust-unexports-no-hevc.txt")
    file(CONFIGURE OUTPUT "${_obs_rust_unexports}" CONTENT "${_unexports}" @ONLY)
  endif()
endif()

if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
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
elseif(OS_WINDOWS)
  # One fragment per FFI module (cmake/rust-exports.d/README.md).
  file(GLOB _obs_rust_export_fragments CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rust-exports.d/*.txt")
  list(SORT _obs_rust_export_fragments)
  foreach(fragment IN LISTS _obs_rust_export_fragments)
    if(NOT ENABLE_HEVC AND fragment MATCHES "obs-codec-hevc[.]txt$")
      continue()
    endif()
    file(STRINGS "${fragment}" _obs_rust_exports REGEX "^[A-Za-z_]")
    foreach(sym IN LISTS _obs_rust_exports)
      # "name unix" shims are not compiled on Windows; the C file exports them.
      if(sym MATCHES " unix$")
        continue()
      endif()
      target_link_options(${OBS_LIBOBS_RUST_TARGET} PRIVATE "LINKER:/EXPORT:${sym}")
    endforeach()
    set_property(TARGET ${OBS_LIBOBS_RUST_TARGET} APPEND PROPERTY LINK_DEPENDS "${fragment}")
  endforeach()
elseif(OS_MACOS)
  # Corrosion 0.5.2 injects the CLT SDK's libraries ahead of CMake's selected
  # sysroot. If CLT is newer than Xcode, its .tbd files may name architectures
  # Xcode's linker cannot read (#777). Let the linker use the selected SDK;
  # preserve any other native library search paths on the imported target.
  get_target_property(_obs_rust_link_dirs libobs_rust-static INTERFACE_LINK_DIRECTORIES)
  if(_obs_rust_link_dirs)
    list(REMOVE_ITEM _obs_rust_link_dirs "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib")
    set_property(TARGET libobs_rust-static PROPERTY INTERFACE_LINK_DIRECTORIES "${_obs_rust_link_dirs}")
  endif()

  target_link_options(${OBS_LIBOBS_RUST_TARGET} PRIVATE "LINKER:-unexported_symbols_list,${_obs_rust_unexports}")
  set_property(TARGET ${OBS_LIBOBS_RUST_TARGET} APPEND PROPERTY LINK_DEPENDS "${_obs_rust_unexports}")
endif()
