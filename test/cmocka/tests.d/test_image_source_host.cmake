# plugins/image-source: the POSIX host's JPEG XR support, file dialog filters
# and stat() sub-second modification time (#858). Compiles
# plugins/image-source/image-source-host-posix.c.h on its own.
if(NOT OS_WINDOWS AND NOT OS_MACOS)
  add_executable(test_image_source_host test_image_source_host.c)
  target_include_directories(test_image_source_host PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_image_source_host PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_image_source_host)
  add_test(test_image_source_host ${CMAKE_CURRENT_BINARY_DIR}/test_image_source_host)
endif()

# The image-source tests that compile its shared files on their own
# (test_image_file_version*, test_slideshow_*) need the host those files call
# now (#858). Attached at the end of this directory, once every tests.d
# fragment has defined its targets, so their own files stay unchanged.
function(_image_source_host_attach)
  foreach(
    _test
    IN
    ITEMS test_image_file_version test_image_file_version_filetime test_slideshow_empty test_slideshow_manual_end
  )
    if(TARGET ${_test})
      target_sources(
        ${_test}
        PRIVATE
          $<$<PLATFORM_ID:Darwin>:${CMAKE_SOURCE_DIR}/plugins/image-source/build/image-source.macos+.h.c>
          $<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:${CMAKE_SOURCE_DIR}/plugins/image-source/build/image-source.posix+.h.c>
          $<$<PLATFORM_ID:Windows>:${CMAKE_SOURCE_DIR}/plugins/image-source/image-source-host-windows.c.h>
      )
      target_include_directories(${_test} PRIVATE "${CMAKE_SOURCE_DIR}")
    endif()
  endforeach()
endfunction()
cmake_language(DEFER CALL _image_source_host_attach)
