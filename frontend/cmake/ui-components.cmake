if(NOT TARGET OBS::qt-slider-ignorewheel)
  add_subdirectory(
    "${CMAKE_SOURCE_DIR}/shared/qt/slider-ignorewheel"
    "${CMAKE_BINARY_DIR}/shared/qt/slider-ignorewheel"
  )
endif()

target_link_libraries(obs-studio PRIVATE OBS::qt-slider-ignorewheel)

obs_frontend_add_sources(UI_COMPONENTS)
