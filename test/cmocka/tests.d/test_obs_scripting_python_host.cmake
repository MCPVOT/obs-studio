# shared/obs-scripting: the Python hosts' start-up choices and sys.path
# directories (#882). Each executable compiles one host file; all three are
# plain C, so they build and run on every non-Windows test host.
if(NOT OS_WINDOWS)
  foreach(_host IN ITEMS windows macos posix)
    set(_test test_obs_scripting_python_host_${_host})
    string(TOUPPER "${_host}" _HOST)
    add_executable(${_test} test_obs_scripting_python_host.c)
    target_include_directories(${_test} PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
    target_compile_definitions(
      ${_test}
      PRIVATE
        TEST_HOST_${_HOST}
        SCRIPT_DIR="test-python-scripts-${_host}"
        OBS_PYTHON_HOST_SOURCE="shared/obs-scripting/obs-scripting-python-host-${_host}.c.h"
    )
    target_link_libraries(${_test} PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
    add_dependencies(cmocka-tests ${_test})
    add_test(${_test} ${CMAKE_CURRENT_BINARY_DIR}/${_test})
    # The macOS hosts create and remove ../PlugIns next to the test binaries,
    # so the Lua and Python host tests must not run at the same time.
    set_tests_properties(${_test} PROPERTIES RESOURCE_LOCK obs-scripting-host-plugins-dir)
  endforeach()
endif()
