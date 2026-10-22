# Characterization of the effect name and index lookups before their port (#1822).
add_executable(test_effect_lookups test_effect_lookups.c)
target_include_directories(test_effect_lookups PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(test_effect_lookups PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})

add_dependencies(cmocka-tests test_effect_lookups)
add_test(test_effect_lookups ${CMAKE_CURRENT_BINARY_DIR}/test_effect_lookups)
