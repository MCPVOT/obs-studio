# The Accept-Encoding our outgoing HTTP calls send: only codings the linked
# libcurl can decode, never br (obs-rust/obs-studio#2453, #2213). No network.
find_package(CURL QUIET)
if(TARGET CURL::libcurl)
  add_executable(test_curl_accept_encoding test_curl_accept_encoding.c)
  target_include_directories(test_curl_accept_encoding PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_curl_accept_encoding PRIVATE CURL::libcurl ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_curl_accept_encoding)
  add_test(test_curl_accept_encoding ${CMAKE_CURRENT_BINARY_DIR}/test_curl_accept_encoding)
endif()
