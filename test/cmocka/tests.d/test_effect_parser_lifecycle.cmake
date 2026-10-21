# ep_free is hidden: characterize the extracted original C source, not a
# future Rust replacement. libobs supplies the allocator and lexer helpers.
add_executable(
  test_effect_parser_lifecycle
  test_effect_parser_lifecycle.c
  "${CMAKE_SOURCE_DIR}/libobs/graphics/effect-parser-lifecycle.c.h"
)
target_include_directories(test_effect_parser_lifecycle PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(test_effect_parser_lifecycle PRIVATE OBS::libobs OBS::caption ${CMOCKA_LIBRARIES})
obs_test_sanitize(test_effect_parser_lifecycle "address,undefined")
add_dependencies(cmocka-tests test_effect_parser_lifecycle)
add_test(test_effect_parser_lifecycle ${CMAKE_CURRENT_BINARY_DIR}/test_effect_parser_lifecycle)
