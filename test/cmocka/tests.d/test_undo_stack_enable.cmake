# Edit > Undo stayed enabled on an empty undo stack after any disable/enable
# span (#2222). Builds the real frontend/utility/undo_stack.cpp.hpp with
# stand-ins for the OBSBasic.ui header and the frontend's precompiled header
# (undo-stack-stubs/) on Qt's offscreen platform: needs Qt Widgets only, no
# display, no frontend build and no libobs.
# Linux only, like test_properties_undo_coalesce: on Windows the offscreen
# platform plugin is not deployed and the test hung to its timeout.
if(OS_LINUX)
  find_package(Qt6 QUIET COMPONENTS Widgets)
  if(TARGET Qt6::Widgets)
    qt_wrap_cpp(_undo_stack_moc "${CMAKE_SOURCE_DIR}/frontend/utility/undo_stack.hpp")
    add_executable(
      test_undo_stack_enable
      test_undo_stack_enable.cpp
      "${CMAKE_SOURCE_DIR}/frontend/utility/undo_stack.cpp.hpp"
      ${_undo_stack_moc}
    )
    set_source_files_properties("${CMAKE_SOURCE_DIR}/frontend/utility/undo_stack.cpp.hpp" PROPERTIES LANGUAGE CXX)
    target_include_directories(
      test_undo_stack_enable
      BEFORE
      PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/undo-stack-stubs"
        "${CMAKE_SOURCE_DIR}/frontend/utility"
        ${CMOCKA_INCLUDE_DIR}
    )
    target_link_libraries(test_undo_stack_enable PRIVATE Qt6::Widgets ${CMOCKA_LIBRARIES})
    add_dependencies(cmocka-tests test_undo_stack_enable)
    add_test(test_undo_stack_enable ${CMAKE_CURRENT_BINARY_DIR}/test_undo_stack_enable)
    set_tests_properties(test_undo_stack_enable PROPERTIES TIMEOUT 120)
    cmocka_qt_runtime(test_undo_stack_enable Qt6::Core)
    unset(_undo_stack_moc)
  endif()
endif()
