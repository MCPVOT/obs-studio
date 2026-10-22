# Selected once by frontend/CMakeLists.txt. Macros retain caller scope and
# run at the original feature/source stages; including this file does no setup.

macro(obs_frontend_platform_qt)
  # No additional Qt platform dependencies.
endmacro()

macro(obs_frontend_platform_plugin_paths)
  # Use the shared absolute installation paths.
endmacro()

macro(obs_frontend_platform_plugin_manager)
  obs_frontend_add_sources(WINDOWS_PLUGIN_MANAGER)
endmacro()

macro(obs_frontend_platform_whatsnew)
  # No additional platform setup for this feature.
endmacro()

macro(obs_frontend_platform_sources)
  if(NOT TARGET OBS::blake2)
    add_subdirectory("${CMAKE_SOURCE_DIR}/deps/blake2" "${CMAKE_BINARY_DIR}/deps/blake2")
  endif()

  if(NOT TARGET OBS::w32-pthreads)
    add_subdirectory("${CMAKE_SOURCE_DIR}/deps/w32-pthreads" "${CMAKE_BINARY_DIR}/deps/w32-pthreads")
  endif()

  find_package(MbedTLS 3...<4 REQUIRED)
  find_package(Detours REQUIRED)
  find_package(nlohmann_json 3.11 REQUIRED)

  # The Rust build says so in its VERSIONINFO, with the full release version (#631).
  if(OBS_BRAND_RUST)
    set(OBS_RC_PRODUCT_NAME "OBS-Studio-Rust")
    set(OBS_RC_PRODUCT_VERSION "${OBS_RUST_VERSION}")
  else()
    set(OBS_RC_PRODUCT_NAME "OBS Studio")
    set(OBS_RC_PRODUCT_VERSION "${OBS_VERSION_CANONICAL}")
  endif()
  configure_file(cmake/windows/obs.rc.in obs.rc)

  obs_frontend_add_sources(WINDOWS)

  target_link_libraries(
    obs-studio
    PRIVATE crypt32 shell32 OBS::blake2 OBS::w32-pthreads MbedTLS::mbedtls nlohmann_json::nlohmann_json Detours::Detours
  )

  target_compile_definitions(obs-studio PRIVATE PSAPI_VERSION=2)

  target_link_options(obs-studio PRIVATE /IGNORE:4099 $<$<CONFIG:DEBUG>:/NODEFAULTLIB:MSVCRT>)

  # The updater installs certified obs-rust releases (AutoUpdateThread.cpp.hpp);
  # upstream's updater.exe in updater/ only speaks obsproject.com's CDN, so it
  # is not built (#1736).

  set_property(TARGET obs-studio APPEND PROPERTY AUTORCC_OPTIONS --format-version 1)

  set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT obs-studio)
  set_target_properties(
    obs-studio
    PROPERTIES
      WIN32_EXECUTABLE TRUE
      VS_DEBUGGER_COMMAND "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/bin/64bit/$<TARGET_FILE_NAME:obs-studio>"
      VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/bin/64bit"
  )
endmacro()
