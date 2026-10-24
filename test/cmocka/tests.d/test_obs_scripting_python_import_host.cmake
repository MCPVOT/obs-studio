# shared/obs-scripting: the Windows and macOS Python library names (#882).
# Each executable compiles one host file; both are plain C, so they build and
# run on every non-Windows test host.
if(NOT OS_WINDOWS)
  foreach(_host IN ITEMS windows macos)
    set(_test test_obs_scripting_python_import_host_${_host})
    string(TOUPPER "${_host}" _HOST)
    add_executable(${_test} test_obs_scripting_python_import_host.c)
    target_include_directories(${_test} PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
    target_compile_definitions(
      ${_test}
      PRIVATE
        TEST_HOST_${_HOST}
        OBS_PYTHON_IMPORT_HOST_SOURCE="shared/obs-scripting/obs-scripting-python-import-host-${_host}.c.h"
    )
    target_link_libraries(${_test} PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
    add_dependencies(cmocka-tests ${_test})
    add_test(${_test} ${CMAKE_CURRENT_BINARY_DIR}/${_test})
  endforeach()
endif()
