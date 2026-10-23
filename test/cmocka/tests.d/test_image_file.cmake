# Characterization of gs_image_file_ex_* before its port to obs-graphics. A
# fake device in the test records the texture calls; no GPU, no display.
add_executable(test_image_file test_image_file.c)
target_compile_definitions(test_image_file PRIVATE IMAGE_FILE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_include_directories(test_image_file PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(test_image_file PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})

add_dependencies(cmocka-tests test_image_file)
add_test(test_image_file ${CMAKE_CURRENT_BINARY_DIR}/test_image_file)
