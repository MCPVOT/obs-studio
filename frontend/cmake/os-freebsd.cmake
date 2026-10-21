# Selected once by frontend/CMakeLists.txt. Macros retain caller scope and
# run at the original feature/source stages; including this file does no setup.

macro(obs_frontend_platform_qt)
  find_package(Qt6 REQUIRED Gui DBus)
endmacro()

macro(obs_frontend_platform_plugin_paths)
  # Use the shared absolute installation paths.
endmacro()

macro(obs_frontend_platform_plugin_manager)
  configure_file(plugin-manager/LoaderPaths_BSD.hpp.in LoaderPaths_BSD.hpp @ONLY)
  target_sources(obs-studio PRIVATE build/frontend.plugin-manager-bsd+.hpp.cpp LoaderPaths_BSD.hpp)
endmacro()

macro(obs_frontend_platform_whatsnew)
  # No additional platform setup for this feature.
endmacro()

macro(obs_frontend_platform_sources)
  target_sources(obs-studio PRIVATE build/frontend.signal-posix+.hpp.cpp)
  target_sources(
    obs-studio
    PRIVATE build/frontend.freebsd+.hpp.cpp build/frontend.native-event-filter+.hpp.cpp build/frontend.x11+.hpp.cpp
  )
  target_compile_definitions(obs-studio PRIVATE OBS_INSTALL_PREFIX="${OBS_INSTALL_PREFIX}")
  target_link_libraries(obs-studio PRIVATE Qt::DBus procstat)

  find_package(Libpci REQUIRED)
  target_link_libraries(obs-studio PRIVATE Libpci::pci)

  if(TARGET OBS::python)
    find_package(Python REQUIRED COMPONENTS Interpreter Development)
    target_link_libraries(obs-studio PRIVATE Python::Python)
    target_link_options(obs-studio PRIVATE LINKER:-no-as-needed)
  endif()

  configure_file(cmake/linux/com.obsproject.Studio.metainfo.xml.in com.obsproject.Studio.metainfo.xml)

  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/com.obsproject.Studio.metainfo.xml"
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
    RENAME com.obsproject.Studio.png
  )

  install(
    FILES cmake/linux/icons/obs-logo-256.png
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps"
    RENAME com.obsproject.Studio.png
  )

  install(
    FILES cmake/linux/icons/obs-logo-512.png
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/512x512/apps"
    RENAME com.obsproject.Studio.png
  )

  install(
    FILES cmake/linux/icons/obs-logo-scalable.svg
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps"
    RENAME com.obsproject.Studio.svg
  )
endmacro()
