find_package(nlohmann_json 3.11 REQUIRED)

set(OBS_PLATFORM_INSTALL_PATH "${CMAKE_INSTALL_PREFIX}/")
set(OBS_PLATFORM_LIBRARY_PATH "${CMAKE_INSTALL_LIBDIR}/")
set(OBS_PLATFORM_DATA_PATH "${CMAKE_INSTALL_DATAROOTDIR}/")
set(OBS_PLATFORM_CORE_MODULE_PATH "${CMAKE_INSTALL_PREFIX}/${OBS_PLUGIN_DESTINATION}/core")
set(OBS_PLATFORM_CORE_DATA_PATH "${CMAKE_INSTALL_PREFIX}/${OBS_DATA_DESTINATION}/obs-modules/core")
set(OBS_PLATFORM_PLUGIN_MODULE_PATH "${CMAKE_INSTALL_PREFIX}/${OBS_PLUGIN_DESTINATION}/plugins")
set(OBS_PLATFORM_PLUGIN_DATA_PATH "${CMAKE_INSTALL_PREFIX}/${OBS_DATA_DESTINATION}/obs-modules/plugins")
obs_frontend_platform_plugin_paths()

target_sources(
  obs-studio
  PRIVATE
    build/frontend.plugin-manager-installedpluginrow+.hpp.cpp
    plugin-manager/InstalledPluginRow.hpp
    build/frontend.plugin-manager-pluginmanager+.hpp.cpp
    plugin-manager/PluginManager.hpp
    build/frontend.plugin-manager-pluginmanagerwindow+.hpp.cpp
    plugin-manager/PluginManagerWindow.hpp
    plugin-manager/PluginModuleLoader.hpp
)

obs_frontend_platform_plugin_manager()

target_link_libraries(obs-studio PRIVATE nlohmann_json::nlohmann_json)
