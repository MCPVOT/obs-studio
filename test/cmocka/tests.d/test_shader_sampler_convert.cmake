# Calls the exported libobs symbol: unchanged assertions cover C reference and
# hybrid, including a later Rust replacement. No graphics device is needed.
add_executable(test_shader_sampler_convert test_shader_sampler_convert.c)
target_include_directories(test_shader_sampler_convert PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(test_shader_sampler_convert PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
add_dependencies(cmocka-tests test_shader_sampler_convert)
add_test(test_shader_sampler_convert ${CMAKE_CURRENT_BINARY_DIR}/test_shader_sampler_convert)
