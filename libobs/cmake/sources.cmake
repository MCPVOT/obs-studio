# libobs source lists, relative to libobs/. The real build (libobs/CMakeLists.txt
# and cmake/os-<os>.cmake) and the cross-compile check (cmake/cross/) both read
# them, so a cross check compiles exactly the files that host's build would.
#
# Per list:
# - LIBOBS_SOURCES: built on every host, no Rust port.
# - LIBOBS_PORTED_SOURCES: C with a Rust port in rust/libobs-rust. The C build
#   compiles them; ENABLE_RUST_LIBOBS links the staticlib instead.
# - LIBOBS_RUST_SOURCES: the C halves only the Rust build adds (C variadics).
# - LIBOBS_<HOST>_SOURCES / _PORTED_SOURCES: the same split, per host.
# - LIBOBS_<HOST>_NOOP_SOURCES: obs-host/noop/<capability>.c for each libobs
#   core host capability that host's own file doesn't implement (#1051).
# <HOST> is WINDOWS, MACOS or LINUX; LINUX also covers FreeBSD and OpenBSD,
# which build obs-nix.c the same way.

set(
  LIBOBS_SOURCES
  obs-audio-controls.c
  obs-audio-controls.h
  obs-audio.c
  obs-av1.h
  obs-avc.h
  obs-canvas.c
  obs-config.h
  obs-core-modules-loader.h
  obs-data.h
  obs-defs.h
  obs-display.c
  obs-encoder.c
  obs-encoder.h
  obs-ffmpeg-compat.h
  obs-hotkey.c
  obs-hotkey.h
  obs-hotkeys.h
  obs-host/com.h
  obs-host/default-monitoring-device.h
  obs-host/defs.h
  obs-host/display-clear-workaround.h
  obs-host/display-srgb-clear.h
  obs-host/graphics-thread.h
  obs-host/graphics-thread-hooks.h
  obs-host/module-paths.h
  obs-host/module-symbols.h
  obs-host/shared-texture.h
  obs-interaction.h
  obs-internal.h
  obs-missing-files.c
  obs-missing-files.h
  obs-module.c
  obs-module.h
  obs-nal.h
  obs-output-delay.c
  obs-output.c
  obs-output.h
  obs-properties.h
  obs-scene-bounds.c
  obs-scene-bounds.h
  obs-scene.c
  obs-scene.h
  obs-service.c
  obs-service.h
  obs-source-deinterlace.c
  obs-source-transition.c
  obs-source.c
  obs-source.h
  obs-video-gpu-encode.c
  obs-video.c
  obs-view.c
  obs.c
  obs.h
  obs.hpp
  util/array-serializer.h
  util/base.h
  util/bitstream.h
  util/bmem.h
  util/buffered-file-serializer.h
  util/c99defs.h
  util/cf-lexer.c
  util/cf-lexer.h
  util/cf-parser.c
  util/cf-parser.h
  util/config-file.h
  util/crc32.h
  util/curl/curl-helper.h
  util/darray.h
  util/deque.h
  util/dstr-libc.c
  util/dstr.h
  util/file-serializer.h
  util/lexer.h
  util/pipe.h
  util/platform.c
  util/platform.h
  util/profiler-snapshot.h
  util/profiler-io.c
  util/profiler.h
  util/profiler.hpp
  util/serializer.h
  util/source-profiler.c
  util/source-profiler.h
  util/sse-intrin.h
  util/task.h
  util/text-lookup.h
  util/threading.h
  util/utf8.h
  util/uthash.h
  util/util.hpp
  util/util_uint128.h
  util/util_uint64.h
  callback/calldata.c
  callback/calldata.h
  callback/decl.c
  callback/decl.h
  callback/proc.c
  callback/proc.h
  callback/signal.c
  callback/signal.h
  media-io/audio-io.h
  media-io/audio-math.h
  media-io/audio-resampler-ffmpeg.c
  media-io/audio-resampler.h
  media-io/format-conversion.h
  media-io/frame-rate.h
  media-io/media-io-defs.h
  media-io/media-remux.c
  media-io/media-remux.h
  media-io/video-frame.h
  media-io/video-io.h
  media-io/video-scaler-ffmpeg.c
  media-io/video-scaler.h
  graphics/axisang.h
  graphics/bounds.h
  graphics/device-exports.h
  graphics/effect-parser.c
  graphics/effect-parser.h
  graphics/effect.c
  graphics/effect.h
  graphics/graphics-ffmpeg.c
  graphics/graphics-imports.c
  graphics/graphics-internal.h
  graphics/graphics.c
  graphics/graphics.h
  graphics/half.h
  graphics/image-file.c
  graphics/image-file.h
  graphics/input.h
  graphics/libnsgif/gif.c
  graphics/libnsgif/lzw.c
  graphics/libnsgif/lzw.h
  graphics/libnsgif/nsgif.h
  graphics/math-defs.h
  graphics/math-extra.h
  graphics/matrix3.h
  graphics/matrix4.h
  graphics/plane.h
  graphics/quat.h
  graphics/shader-parser.c
  graphics/shader-parser.h
  graphics/srgb.h
  graphics/texture-render.c
  graphics/vec2.h
  graphics/vec3.h
  graphics/vec4.h
)

set(
  LIBOBS_PORTED_SOURCES
  obs-av1.c
  obs-avc.c
  obs-data.c
  obs-hotkey-name-map.c
  obs-nal.c
  obs-properties.c
  util/array-serializer.c
  util/base.c
  util/bitstream.c
  util/bmem.c
  util/buffered-file-serializer.c
  util/cf-tokenizer.c
  util/config-file.c
  util/crc32.c
  util/dstr.c
  util/file-serializer.c
  util/lexer.c
  util/path-extension.c
  util/pipe.c
  util/platform-number.c
  util/profiler-snapshot.c
  util/profiler.c
  util/task.c
  util/text-lookup.c
  util/utf8.c
  media-io/audio-io.c
  media-io/format-conversion.c
  media-io/video-fourcc.c
  media-io/video-frame.c
  media-io/video-io.c
  media-io/video-matrices.c
  graphics/axisang.c
  graphics/bounds.c
  graphics/math-extra.c
  graphics/matrix3.c
  graphics/matrix4.c
  graphics/plane.c
  graphics/quat.c
  graphics/shader-param-names.c
  graphics/vec2.c
  graphics/vec3.c
  graphics/vec4.c
)

set(LIBOBS_RUST_SOURCES util/base-variadic.c util/pipe-variadic.c util/profiler-variadic.c)

if(ENABLE_HEVC)
  list(APPEND LIBOBS_SOURCES obs-hevc.h)
  list(APPEND LIBOBS_PORTED_SOURCES obs-hevc.c)
endif()

set(
  LIBOBS_WINDOWS_SOURCES
  audio-monitoring/win32/wasapi-enum-devices.c
  audio-monitoring/win32/wasapi-monitoring-available.c
  audio-monitoring/win32/wasapi-output.c
  audio-monitoring/win32/wasapi-output.h
  obs-core-modules-loader-win.c
  obs-win-crash-handler.c
  obs-windows.c
  util/threading-windows.h
  util/windows/CoTaskMemPtr.hpp
  util/windows/device-enum.c
  util/windows/device-enum.h
  util/windows/HRError.hpp
  util/windows/win-registry.h
  util/windows/win-version.h
  util/windows/window-helpers.c
  util/windows/window-helpers.h
)
set(LIBOBS_WINDOWS_PORTED_SOURCES util/pipe-windows.c util/platform-windows.c util/threading-windows.c)
set(
  LIBOBS_WINDOWS_NOOP_SOURCES
  obs-host/noop/default-monitoring-device.c
  obs-host/noop/display-srgb-clear.c
  obs-host/noop/graphics-thread.c
  obs-host/noop/module-paths.c
)

set(
  LIBOBS_MACOS_SOURCES
  audio-monitoring/osx/coreaudio-enum-devices.c
  audio-monitoring/osx/coreaudio-monitoring-available.c
  audio-monitoring/osx/coreaudio-output.c
  audio-monitoring/osx/mac-helpers.h
  obs-cocoa.m
  obs-core-modules-loader-mac.m
  util/apple/cfstring-utils.h
  util/threading-posix.h
)
set(LIBOBS_MACOS_PORTED_SOURCES util/pipe-posix.c util/platform-cocoa.m util/platform-nix.c util/threading-posix.c)
set(
  LIBOBS_MACOS_NOOP_SOURCES
  obs-host/noop/com.c
  obs-host/noop/display-clear-workaround.c
  obs-host/noop/graphics-thread-hooks.c
  obs-host/noop/module-symbols.c
  obs-host/noop/shared-texture.c
)

# Linux and the BSDs. Optional parts (PulseAudio, GIO, Wayland) stay in
# cmake/os-linux.cmake and cmake/os-freebsd.cmake, next to their find_package.
set(
  LIBOBS_LINUX_SOURCES
  obs-core-modules-loader-nix.c
  obs-nix-platform.c
  obs-nix-platform.h
  obs-nix-x11-keymap.h
  obs-nix-x11.c
  obs-nix.c
  util/platform-nix-inhibit.c
  util/threading-posix.h
)
set(LIBOBS_LINUX_PORTED_SOURCES util/pipe-posix.c util/platform-nix.c util/threading-posix.c)
set(
  LIBOBS_LINUX_NOOP_SOURCES
  obs-host/noop/com.c
  obs-host/noop/default-monitoring-device.c
  obs-host/noop/display-clear-workaround.c
  obs-host/noop/display-srgb-clear.c
  obs-host/noop/graphics-thread.c
  obs-host/noop/graphics-thread-hooks.c
  obs-host/noop/module-paths.c
  obs-host/noop/module-symbols.c
  obs-host/noop/shared-texture.c
)

# Adds a host's lists to `target`: its sources, its no-ops, and either its
# ported C or, with ENABLE_RUST_LIBOBS, nothing (the staticlib replaces them).
macro(libobs_add_host_sources target host)
  target_sources(${target} PRIVATE ${LIBOBS_${host}_SOURCES} ${LIBOBS_${host}_NOOP_SOURCES})
  if(NOT ENABLE_RUST_LIBOBS)
    target_sources(${target} PRIVATE ${LIBOBS_${host}_PORTED_SOURCES})
  endif()
endmacro()
