cmake_minimum_required(VERSION 3.28...3.30)

option(ENABLE_AJA "Build OBS with aja support" ON)

if(NOT ENABLE_AJA)
  return()
endif()

find_package(LibAJANTV2 REQUIRED)

add_library(aja MODULE)
add_library(OBS::aja ALIAS aja)

add_library(aja-support OBJECT)
add_library(OBS::aja-support ALIAS aja-support)

target_sources(
  aja-support
  PRIVATE build/aja.support+.hpp.cpp
  PUBLIC
    aja-card-manager.hpp
    aja-common.hpp
    aja-enums.hpp
    aja-presets.hpp
    aja-props.hpp
    aja-routing.hpp
    aja-ui-props.hpp
    aja-vpid-data.hpp
    aja-widget-io.hpp
)

target_link_libraries(aja-support PUBLIC OBS::libobs AJA::LibAJANTV2)

set_target_properties(aja-support PROPERTIES FOLDER plugins/aja POSITION_INDEPENDENT_CODE TRUE)

target_sources(
  aja
  PRIVATE
    build/aja.core+.hpp.cpp
    build/aja.audio-repack+.h.c
    aja-output.hpp
    aja-source.hpp
    audio-repack.h
    audio-repack.hpp
)

target_compile_options(
  aja-support
  PUBLIC
    $<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang,GNU>:-Wno-deprecated-declarations>
    $<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:-Wno-unused-variable>
)

target_link_libraries(aja PRIVATE OBS::aja-support)

if(OS_WINDOWS)
  configure_file(cmake/windows/obs-module.rc.in win-aja.rc)
  target_sources(aja PRIVATE win-aja.rc)
  set_property(TARGET aja PROPERTY COMPILE_WARNING_AS_ERROR FALSE)
endif()

set_target_properties_obs(aja PROPERTIES FOLDER plugins/aja PREFIX "")
