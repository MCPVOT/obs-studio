# Selected once by frontend/CMakeLists.txt. Macros retain caller scope and
# run at the original feature/source stages; including this file does no setup.
set(_frontend_platform_cmake_dir "${CMAKE_CURRENT_LIST_DIR}")

macro(obs_frontend_platform_qt)
  find_package(Qt6 REQUIRED Gui DBus)
endmacro()

macro(obs_frontend_platform_plugin_paths)
  # A relocatable build (the AppImage) runs from wherever it was unpacked, so the
  # install prefix belongs to whatever else is installed there: a distribution's
  # OBS keeps plugins for its own libobs in <prefix>/lib/obs-plugins. Its system
  # plugin locations are its own tree, relative to the executable like its core
  # modules (PluginModuleLoader_Linux.cpp.hpp resolves them).
  if(ENABLE_RELOCATABLE AND NOT OS_FLATPAK)
    set(OBS_PLATFORM_INSTALL_PATH "../")
    set(OBS_PLATFORM_PLUGIN_MODULE_PATH "../${OBS_PLUGIN_DESTINATION}/plugins")
    set(OBS_PLATFORM_PLUGIN_DATA_PATH "../${OBS_DATA_DESTINATION}/obs-modules/plugins")
  endif()
endmacro()

macro(obs_frontend_platform_plugin_manager)
  if(OS_FLATPAK)
    configure_file(plugin-manager/LoaderPaths_Flatpak.hpp.in LoaderPaths_Flatpak.hpp @ONLY)
    obs_frontend_add_sources(FLATPAK_PLUGIN_MANAGER)
  else()
    configure_file(plugin-manager/LoaderPaths_Linux.hpp.in LoaderPaths_Linux.hpp @ONLY)
    obs_frontend_add_sources(LINUX_PLUGIN_MANAGER)
  endif()
endmacro()

macro(obs_frontend_platform_whatsnew)
  find_package(MbedTLS 3...<4 REQUIRED)
  find_package(nlohmann_json 3.11 REQUIRED)

  if(NOT TARGET OBS::blake2)
    add_subdirectory("${CMAKE_SOURCE_DIR}/deps/blake2" "${CMAKE_BINARY_DIR}/deps/blake2")
  endif()

  target_link_libraries(obs-studio PRIVATE MbedTLS::mbedtls nlohmann_json::nlohmann_json OBS::blake2)

  obs_frontend_add_sources(LINUX_WHATSNEW)
endmacro()

macro(obs_frontend_platform_sources)
  obs_frontend_add_sources(POSIX_SIGNALS)
  obs_frontend_add_sources(LINUX)
  target_compile_definitions(
    obs-studio
    PRIVATE OBS_INSTALL_PREFIX="${OBS_INSTALL_PREFIX}" $<$<BOOL:${ENABLE_PORTABLE_CONFIG}>:ENABLE_PORTABLE_CONFIG>
  )
  target_link_libraries(obs-studio PRIVATE Qt::DBus)

  find_package(Libpci REQUIRED)
  target_link_libraries(obs-studio PRIVATE Libpci::pci)

  if(TARGET OBS::python)
    find_package(Python REQUIRED COMPONENTS Interpreter Development)
    target_link_libraries(obs-studio PRIVATE Python::Python)
    target_link_options(obs-studio PRIVATE LINKER:-no-as-needed)
  endif()

  include("${_frontend_platform_cmake_dir}/linux/appdata-release-date.cmake")

  if(NOT DEFINED GIT_HASH)
    if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
      execute_process(
        COMMAND git rev-parse HEAD
        OUTPUT_VARIABLE GIT_HASH
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    else()
      set(GIT_HASH "master")
    endif()
  endif()

  # The Rust build's release is its own version, linked to its own release page (#631).
  set(_obs_metainfo_version "${OBS_VERSION}")
  if(OBS_BRAND_RUST)
    set(OBS_VERSION "${OBS_RUST_VERSION}")
  endif()
  configure_file(cmake/linux/com.obsproject.Studio.metainfo.xml.in ${OBS_DESKTOP_ID}.metainfo.xml @ONLY)
  set(OBS_VERSION "${_obs_metainfo_version}")
  if(OBS_BRAND_RUST)
    file(READ "${CMAKE_CURRENT_BINARY_DIR}/${OBS_DESKTOP_ID}.metainfo.xml" _obs_metainfo)
    string(
      REPLACE
      "https://github.com/obsproject/obs-studio/releases/tag/${OBS_RUST_VERSION}"
      "https://github.com/obs-rust/obs-studio/releases/tag/obs-studio-rust-v${OBS_RUST_VERSION}"
      _obs_metainfo
      "${_obs_metainfo}"
    )
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${OBS_DESKTOP_ID}.metainfo.xml" "${_obs_metainfo}")
  endif()

  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${OBS_DESKTOP_ID}.metainfo.xml"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo"
  )

  configure_file(cmake/linux/com.obsproject.Studio.desktop "${OBS_DESKTOP_ID}.desktop" @ONLY)
  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${OBS_DESKTOP_ID}.desktop"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
  )

  install(
    FILES cmake/linux/icons/obs-logo-128.png
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps"
    RENAME "${OBS_DESKTOP_ID}.png"
  )

  install(
    FILES cmake/linux/icons/obs-logo-256.png
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps"
    RENAME "${OBS_DESKTOP_ID}.png"
  )

  install(
    FILES cmake/linux/icons/obs-logo-512.png
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/512x512/apps"
    RENAME "${OBS_DESKTOP_ID}.png"
  )

  install(
    FILES cmake/linux/icons/obs-logo-scalable.svg
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps"
    RENAME "${OBS_DESKTOP_ID}.svg"
  )
endmacro()
