# OBS CMake unity groups module (BUILD.md)
#
# A converted component lists group files (name+.h.c, name+.hpp.cpp,
# name+.h.m, name+.hpp.mm) as its sources. Each group #includes, first, its
# precompiled header (a <family>.pch.h next to it), then the component's
# implementation files (foo.c.h, foo.cpp.hpp, ...). Every implementation
# file includes that same header and nothing else (ci/test_build_groups.py).
#
# Grouped targets build one way only: unity and precompiled headers on,
# whatever ENABLE_UNITY_BUILD and ENABLE_PCH say (those still steer the
# targets not yet converted). Per language, the header most of a target's
# groups use is precompiled; a group of another family includes its own
# textually, as does a group with compile options of its own (-fobjc-arc), which
# a header precompiled without them would not match. Xcode allows one prefix
# header per target, so there only the language with the most groups gets one.
#
# A target that compiles an implementation file directly (a C test building
# one libobs file) gets it in its language, with the owning target's include
# directories and definitions for that file, since its header comes from
# there.
#
# _obs_groups_all runs before the deferred pass in pch.cmake.

include_guard(GLOBAL)

set(_OBS_GROUP_REGEX "\\+\\.(h\\.c|hpp\\.cpp|h\\.m|hpp\\.mm)$")
set(_OBS_IMPL_REGEX "\\.(c\\.h|cpp\\.hpp|m\\.h|mm\\.hpp)$")

# _obs_file_language: the language a group or implementation file compiles as.
function(_obs_file_language file out)
  if(file MATCHES "(\\.c\\.h|\\.h\\.c)$")
    set(${out} C PARENT_SCOPE)
  elseif(file MATCHES "(\\.cpp\\.hpp|\\.hpp\\.cpp)$")
    set(${out} CXX PARENT_SCOPE)
  elseif(file MATCHES "(\\.m\\.h|\\.h\\.m)$")
    set(${out} OBJC PARENT_SCOPE)
  elseif(file MATCHES "(\\.mm\\.hpp|\\.hpp\\.mm)$")
    set(${out} OBJCXX PARENT_SCOPE)
  else()
    set(${out} "" PARENT_SCOPE)
  endif()
endfunction()

# _obs_group_read: <group>'s precompiled header and implementation files
# (absolute paths).
function(_obs_group_read group out_pch out_members)
  set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${group}")
  file(STRINGS "${group}" _lines REGEX "^#include \"[^\"]+\"")
  cmake_path(GET group PARENT_PATH _dir)
  set(_pch "")
  set(_members "")
  foreach(_line IN LISTS _lines)
    string(REGEX REPLACE "^#include \"([^\"]+)\".*" "\\1" _path "${_line}")
    cmake_path(ABSOLUTE_PATH _path BASE_DIRECTORY "${_dir}" NORMALIZE OUTPUT_VARIABLE _absolute)
    if(_path MATCHES "${_OBS_IMPL_REGEX}")
      list(APPEND _members "${_absolute}")
    elseif(_path MATCHES "\\.pch\\.(h|hpp)$")
      set(_pch "${_absolute}")
    endif()
  endforeach()
  if(NOT _pch)
    message(FATAL_ERROR "${group}: no precompiled header (BUILD.md)")
  endif()
  set(${out_pch} "${_pch}" PARENT_SCOPE)
  set(${out_members} ${_members} PARENT_SCOPE)
endfunction()

# _obs_target_groups: set up <target>'s groups; marks it OBS_GROUPED.
function(_obs_target_groups target)
  get_target_property(_sources ${target} SOURCES)
  get_target_property(_source_dir ${target} SOURCE_DIR)
  set(_groups "")
  foreach(_source IN LISTS _sources)
    if(_source MATCHES "${_OBS_GROUP_REGEX}" AND NOT _source MATCHES "[<>$]")
      cmake_path(ABSOLUTE_PATH _source BASE_DIRECTORY "${_source_dir}" NORMALIZE OUTPUT_VARIABLE _absolute)
      list(APPEND _groups "${_absolute}")
    endif()
  endforeach()
  if(NOT _groups)
    return()
  endif()
  set_property(TARGET ${target} PROPERTY OBS_GROUPED TRUE)

  set(_languages "")
  set(_own_options "")
  foreach(_group IN LISTS _groups)
    _obs_group_read("${_group}" _pch _members)
    _obs_file_language("${_group}" _language)
    list(APPEND _languages ${_language})
    # A group with its own compile options (-fobjc-arc, warning switches) may
    # not match a header precompiled with the target's: it includes its
    # header textually and doesn't vote for the target's.
    get_source_file_property(_options "${_group}" TARGET_DIRECTORY ${target} COMPILE_OPTIONS)
    if(_options)
      list(APPEND _own_options "${_group}")
    else()
      list(APPEND _pchs_${_language} "${_pch}")
    endif()
    list(APPEND _groups_${_language} "${_group}|${_pch}")
    foreach(_member IN LISTS _members)
      set_property(GLOBAL PROPERTY "_OBS_IMPL_OWNER:${_member}" ${target})
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES _languages)

  # Per language, the header most groups share; on Xcode only the language
  # with the most groups.
  set(_best_language "")
  set(_best_count 0)
  foreach(_language IN LISTS _languages)
    list(LENGTH _pchs_${_language} _count)
    if(_count GREATER _best_count)
      set(_best_language ${_language})
      set(_best_count ${_count})
    endif()
    set(_main "")
    set(_main_count 0)
    set(_distinct "${_pchs_${_language}}")
    list(REMOVE_DUPLICATES _distinct)
    foreach(_pch IN LISTS _distinct)
      set(_n 0)
      foreach(_seen IN LISTS _pchs_${_language})
        if(_seen STREQUAL _pch)
          math(EXPR _n "${_n} + 1")
        endif()
      endforeach()
      if(_n GREATER _main_count)
        set(_main "${_pch}")
        set(_main_count ${_n})
      endif()
    endforeach()
    set(_main_${_language} "${_main}")
  endforeach()

  set(_skipped "")
  foreach(_language IN LISTS _languages)
    set(_precompile TRUE)
    if(CMAKE_GENERATOR STREQUAL "Xcode" AND NOT _language STREQUAL _best_language)
      set(_precompile FALSE)
    endif()
    if(_precompile AND _main_${_language})
      target_precompile_headers(${target} PRIVATE "$<$<COMPILE_LANGUAGE:${_language}>:${_main_${_language}}>")
    endif()
    foreach(_entry IN LISTS _groups_${_language})
      string(REPLACE "|" ";" _parts "${_entry}")
      list(GET _parts 0 _group)
      list(GET _parts 1 _pch)
      if(NOT _precompile OR NOT _pch STREQUAL _main_${_language} OR _group IN_LIST _own_options)
        list(APPEND _skipped "${_group}")
      endif()
    endforeach()
  endforeach()
  if(_skipped)
    set_source_files_properties(${_skipped} TARGET_DIRECTORY ${target} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  endif()
  _obs_pch_ccache(${target})
endfunction()

# _obs_target_impl_languages: implementation files <target> compiles
# directly compile in their language, with their owner's include directories
# and definitions.
function(_obs_target_impl_languages target)
  get_target_property(_sources ${target} SOURCES)
  get_target_property(_source_dir ${target} SOURCE_DIR)
  foreach(_source IN LISTS _sources)
    if(NOT _source MATCHES "${_OBS_IMPL_REGEX}" OR _source MATCHES "[<>$]")
      continue()
    endif()
    cmake_path(ABSOLUTE_PATH _source BASE_DIRECTORY "${_source_dir}" NORMALIZE OUTPUT_VARIABLE _absolute)
    _obs_file_language("${_absolute}" _language)
    set_source_files_properties("${_absolute}" TARGET_DIRECTORY ${target} PROPERTIES LANGUAGE ${_language})
    get_property(_owner GLOBAL PROPERTY "_OBS_IMPL_OWNER:${_absolute}")
    if(_owner AND NOT _owner STREQUAL target)
      set_property(
        SOURCE "${_absolute}" TARGET_DIRECTORY ${target}
        APPEND
        PROPERTY INCLUDE_DIRECTORIES "$<TARGET_PROPERTY:${_owner},INCLUDE_DIRECTORIES>"
      )
      set_property(
        SOURCE "${_absolute}" TARGET_DIRECTORY ${target}
        APPEND
        PROPERTY COMPILE_DEFINITIONS "$<TARGET_PROPERTY:${_owner},COMPILE_DEFINITIONS>"
      )
    endif()
  endforeach()
endfunction()

# _obs_groups_all: _obs_target_groups for every target of <directory> and
# its subdirectories.
function(_obs_groups_all directory)
  get_property(_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(_target IN LISTS _targets)
    get_target_property(_imported ${_target} IMPORTED)
    get_target_property(_type ${_target} TYPE)
    if(NOT _imported AND NOT _type MATCHES "^(INTERFACE_LIBRARY|UTILITY)$")
      _obs_target_groups(${_target})
    endif()
  endforeach()
  get_property(_subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(_subdirectory IN LISTS _subdirectories)
    _obs_groups_all("${_subdirectory}")
  endforeach()
endfunction()
