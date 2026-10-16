# The bundled JetBrainsMono Nerd Font resolves through the Linux Fontconfig
# backend without being installed (obs-rust/obs-studio#1012).
if(OS_LINUX AND TARGET OBS::text-freetype2)
  add_executable(
    test_text_freetype2_bundled_font
    test_text_freetype2_bundled_font.c
    "${CMAKE_SOURCE_DIR}/plugins/text-freetype2/find-font-unix.c"
    "${CMAKE_SOURCE_DIR}/plugins/text-freetype2/text-freetype2-host-posix.c"
  )
  target_compile_definitions(
    test_text_freetype2_bundled_font
    PRIVATE BUNDLED_FONT_DIR="${CMAKE_SOURCE_DIR}/plugins/text-freetype2/data/fonts"
  )
  target_include_directories(
    test_text_freetype2_bundled_font
    PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/plugins/text-freetype2"
  )
  target_link_libraries(
    test_text_freetype2_bundled_font
    PRIVATE OBS::libobs Freetype::Freetype Fontconfig::Fontconfig ${CMOCKA_LIBRARIES}
  )
  add_dependencies(cmocka-tests test_text_freetype2_bundled_font)
  add_test(test_text_freetype2_bundled_font ${CMAKE_CURRENT_BINARY_DIR}/test_text_freetype2_bundled_font)
endif()
