# "Use <key> in OBS" for Wayland background hotkeys the desktop runs on
# another key (obs-rust/obs-studio#401). Pure logic from the frontend, like
# test_hotkey_background_ui: no Qt, no display, no desktop.
if(OS_LINUX OR OS_FREEBSD)
  find_package(nlohmann_json 3.11 QUIET)
  if(TARGET nlohmann_json::nlohmann_json)
    add_executable(
      test_hotkey_background_use_key
      test_hotkey_background_use_key.cpp
      "${CMAKE_SOURCE_DIR}/frontend/settings/OBSHotkeyBackground.cpp.hpp"
    )
    target_include_directories(
      test_hotkey_background_use_key
      PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/frontend/settings"
    )
    target_link_libraries(
      test_hotkey_background_use_key
      PRIVATE OBS::libobs nlohmann_json::nlohmann_json ${CMOCKA_LIBRARIES}
    )
    add_dependencies(cmocka-tests test_hotkey_background_use_key)
    add_test(test_hotkey_background_use_key ${CMAKE_CURRENT_BINARY_DIR}/test_hotkey_background_use_key)
  endif()
endif()
