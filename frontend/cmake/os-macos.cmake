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
  obs_frontend_add_sources(MACOS_PLUGIN_MANAGER)
endmacro()

macro(obs_frontend_platform_whatsnew)
  include(cmake/feature-macos-update.cmake)
  target_compile_definitions(obs-studio PRIVATE WHATSNEW_URL=MAC_WHATSNEW_URL)
endmacro()

macro(obs_frontend_platform_sources)
  obs_frontend_add_sources(POSIX_SIGNALS)
  include(cmake/feature-sparkle.cmake)

  obs_frontend_add_sources(MACOS)
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
