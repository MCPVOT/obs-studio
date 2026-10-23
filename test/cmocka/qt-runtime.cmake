# cmocka_qt_runtime(<test> <Qt target>): lets a Qt-linked test find Qt at run
# time outside the app bundle, through the test's environment: macOS gets the
# directory holding the Qt frameworks on DYLD_FRAMEWORK_PATH (a build rpath
# does not reach the binary under the Xcode generator), Windows gets Qt's bin
# directory on PATH and its plugins directory (next to bin, as
# find_qt_plugins() expects) on QT_PLUGIN_PATH. Linux finds Qt through the
# system loader. The path is resolved here: tests added with
# add_test(<name> <cmd>) get no generator expressions in their properties.
function(cmocka_qt_runtime test qt_target)
  get_target_property(qt_location ${qt_target} LOCATION)
  if(NOT qt_location)
    return()
  endif()
  if(OS_MACOS)
    # .../lib/QtCore.framework/Versions/A/QtCore -> .../lib
    string(REGEX REPLACE "/[^/]+\\.framework/.*$" "" qt_dir "${qt_location}")
    set_property(
      TEST ${test}
      APPEND
      PROPERTY ENVIRONMENT_MODIFICATION "DYLD_FRAMEWORK_PATH=path_list_prepend:${qt_dir}"
    )
  elseif(OS_WINDOWS)
    # .../bin/Qt6Core.dll -> .../bin and .../plugins
    get_filename_component(qt_dir "${qt_location}" DIRECTORY)
    set_property(TEST ${test} APPEND PROPERTY ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${qt_dir}")
    get_filename_component(qt_prefix "${qt_dir}" DIRECTORY)
    if(IS_DIRECTORY "${qt_prefix}/plugins")
      set_property(
        TEST ${test}
        APPEND
        PROPERTY ENVIRONMENT_MODIFICATION "QT_PLUGIN_PATH=path_list_prepend:${qt_prefix}/plugins"
      )
    endif()
  endif()
endfunction()
