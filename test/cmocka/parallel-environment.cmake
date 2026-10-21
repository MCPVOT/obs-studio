# Eight concurrent test processes must share the software-rendering CPU budget.
# Mesa otherwise starts a worker pool sized to the machine for each renderer.
if(OS_LINUX)
  get_property(_cmocka_registered_tests DIRECTORY PROPERTY TESTS)
  if(_cmocka_registered_tests)
    set_property(TEST ${_cmocka_registered_tests} APPEND PROPERTY ENVIRONMENT "LP_NUM_THREADS=1")
  endif()
endif()
