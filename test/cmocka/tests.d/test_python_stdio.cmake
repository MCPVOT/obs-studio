# Python scripting's sys.stdout/sys.stderr are file-like (obsproject/obs-studio#4628).
# Runs OBS's startup script in an embedded interpreter with a stub obspython,
# so it needs only the Python headers, not ENABLE_SCRIPTING or an obs core.
# Linux and the BSDs only: there OBS links Python at build time, as this test
# does; Windows and macOS load it at runtime (obs-scripting-python-import.c).
if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
  find_package(Python 3.8 QUIET COMPONENTS Development.Embed)
  if(Python_Development.Embed_FOUND)
    add_executable(test_python_stdio test_python_stdio.c)
    target_include_directories(
      test_python_stdio
      PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/shared/obs-scripting"
    )
    target_link_libraries(test_python_stdio PRIVATE OBS::libobs Python::Python ${CMOCKA_LIBRARIES})
    add_dependencies(cmocka-tests test_python_stdio)
    add_test(test_python_stdio ${CMAKE_CURRENT_BINARY_DIR}/test_python_stdio)
  endif()
endif()
