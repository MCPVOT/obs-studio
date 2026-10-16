# OBS CMake unity build module
#
# With ENABLE_UNITY_BUILD, each target compiles its sources in batches of
# OBS_UNITY_BUILD_BATCH_SIZE concatenated translation units. File-local names
# (static functions and variables, file-scope macros) must then be unique
# across a target. cmake/cross builds Windows' and macOS' platform-only
# sources the same way from Linux; targets whose sources can't share a
# translation unit (graphics-hook: one Direct3D version per source) set
# OBS_DISABLE_UNITY_BUILD.
# obs_target_unity runs with obs_target_pch for every first-party target.

include_guard(GLOBAL)

# On by default where every target is verified (Linux). Windows and macOS
# still have collisions in targets cmake/cross doesn't build (libobs-d3d11,
# obs-browser, nv-filters, ...): pass -DENABLE_UNITY_BUILD=ON to try them.
if(OS_LINUX)
  option(ENABLE_UNITY_BUILD "Compile each target as batches of concatenated sources" ON)
else()
  option(ENABLE_UNITY_BUILD "Compile each target as batches of concatenated sources" OFF)
endif()
set(OBS_UNITY_BUILD_BATCH_SIZE 8 CACHE STRING "Sources per unity translation unit")

# Sources that duplicate another source of the same target wholesale (an
# alternative backend, a kept legacy version, a fork: window-capture.c next to
# duplicator-monitor-capture.c, obs-nix-wayland.c next to obs-nix-x11.c,
# obs-ffmpeg-mpegts.c next to obs-ffmpeg-output.c, media-playback cache.c next to
# media.c): they compile on their own
# rather than being renamed symbol by symbol.
#
# platform-windows.c needs the full <windows.h> (NTSTATUS, ...): in a batch
# an earlier source may have included it with WIN32_LEAN_AND_MEAN (on Windows
# arm64, util/sse-intrin.h leaves that defined), making its own include a
# no-op.
set(
  _OBS_UNITY_SKIP_SOURCES
  libobs-opengl/gl-wayland-egl.c
  plugins/image-source/obs-slideshow.c
  plugins/win-capture/window-capture.c
  libobs/obs-nix-wayland.c
  plugins/obs-ffmpeg/obs-ffmpeg-mpegts.c
  shared/media-playback/media-playback/cache.c
  libobs/util/platform-windows.c
)

# obs_target_unity: turn on unity builds for <target>.
function(obs_target_unity target)
  if(NOT ENABLE_UNITY_BUILD)
    return()
  endif()
  get_target_property(_type ${target} TYPE)
  if(_type MATCHES "^(INTERFACE_LIBRARY|UTILITY)$")
    return()
  endif()
  get_target_property(_disabled ${target} OBS_DISABLE_UNITY_BUILD)
  if(_disabled)
    return()
  endif()

  set_target_properties(
    ${target}
    PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE ${OBS_UNITY_BUILD_BATCH_SIZE} UNITY_BUILD_UNIQUE_ID OBS_UNITY_ID
  )

  # The PCH-exempt sources define macros before their includes, so they must
  # be the first thing their translation unit sees; the duplicates above
  # can't share one. Keep both out of batches.
  _obs_target_sources_matching(${target} _skipped ${_OBS_PCH_SKIP_SOURCES} ${_OBS_UNITY_SKIP_SOURCES})
  # Third-party sources (deps/, e.g. libdshowcapture through its INTERFACE
  # library) aren't written to share a translation unit with ours. Nor are
  # Objective-C sources: without the OBJC language enabled CMake files .m
  # under C, and Xcode would then compile them inside a C batch.
  _obs_target_sources_where(${target} _third_party "^${CMAKE_SOURCE_DIR}/deps/")
  _obs_target_sources_where(${target} _objc "\\.(m|mm)$")
  list(APPEND _skipped ${_third_party} ${_objc})
  if(_skipped)
    set_source_files_properties(${_skipped} TARGET_DIRECTORY ${target} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
  endif()
endfunction()
