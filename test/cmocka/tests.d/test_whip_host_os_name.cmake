# plugins/obs-webrtc: the POSIX host names the OS in the WHIP User-Agent from
# uname() (#861). Compiles plugins/obs-webrtc/whip-host-posix.cpp.hpp on its
# own; no libdatachannel or libobs needed.
if(NOT OS_WINDOWS AND NOT OS_MACOS)
  add_executable(test_whip_host_os_name test_whip_host_os_name.cpp)
  target_include_directories(test_whip_host_os_name PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_whip_host_os_name PRIVATE ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_whip_host_os_name)
  add_test(test_whip_host_os_name ${CMAKE_CURRENT_BINARY_DIR}/test_whip_host_os_name)
endif()
