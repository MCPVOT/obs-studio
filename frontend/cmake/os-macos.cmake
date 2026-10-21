# Selected once by frontend/CMakeLists.txt. Macros retain caller scope and
# run at the original feature/source stages; including this file does no setup.

macro(obs_frontend_platform_qt)
  # Qt 6.11 Cocoa's default-icon focus callback needs a private adapter.
  if(Qt6_VERSION VERSION_GREATER_EQUAL 6.11 AND Qt6_VERSION VERSION_LESS 6.12)
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
    target_link_libraries(obs-studio PRIVATE Qt6::GuiPrivate)
  endif()
endmacro()

macro(obs_frontend_platform_plugin_paths)
  # Use the shared absolute installation paths.
endmacro()

macro(obs_frontend_platform_plugin_manager)
  target_sources(obs-studio PRIVATE build/frontend.plugin-manager-mm+.hpp.mm)
endmacro()

macro(obs_frontend_platform_whatsnew)
  include(cmake/feature-macos-update.cmake)
endmacro()

macro(obs_frontend_platform_sources)
  target_sources(obs-studio PRIVATE build/frontend.signal-posix+.hpp.cpp)
  include(cmake/feature-sparkle.cmake)

  target_sources(
    obs-studio
    PRIVATE
      build/frontend.macos+.hpp.cpp
      dialogs/OBSPermissions.hpp
      forms/OBSPermissions.ui
      build/frontend.macos-arc-mm+.hpp.mm
      build/frontend.macos-mm+.hpp.mm
      build/frontend.native-event-filter+.hpp.cpp
  )
  target_compile_options(obs-studio PRIVATE -Wno-quoted-include-in-framework-header -Wno-comma)

  list(APPEND _frontend_objcxx_compile_options -fobjc-arc -fmodules -fcxx-modules)

  set_property(
    SOURCE build/frontend.macos-arc-mm+.hpp.mm
    APPEND
    PROPERTY COMPILE_OPTIONS ${_frontend_objcxx_compile_options}
  )

  if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 14.0.3)
    target_compile_options(obs-studio PRIVATE -Wno-error=unqualified-std-cast-call)
  endif()

  target_link_libraries(
    obs-studio
    PRIVATE
      "$<LINK_LIBRARY:FRAMEWORK,AppKit.framework>"
      "$<LINK_LIBRARY:FRAMEWORK,ApplicationServices.framework>"
      "$<LINK_LIBRARY:FRAMEWORK,AVFoundation.framework>"
  )
endmacro()
