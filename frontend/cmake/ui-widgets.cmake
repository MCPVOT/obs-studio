if(NOT TARGET OBS::qt-vertical-scroll-area)
  add_subdirectory(
    "${CMAKE_SOURCE_DIR}/shared/qt/vertical-scroll-area"
    "${CMAKE_BINARY_DIR}/shared/qt/vertical-scroll-area"
  )
endif()

target_link_libraries(obs-studio PRIVATE OBS::qt-vertical-scroll-area)

target_sources(
  obs-studio
  PRIVATE
    build/frontend.widgets+.hpp.cpp
    widgets/AudioMixer.hpp
    widgets/ColorSelect.hpp
    widgets/OBSBasic.hpp
    build/frontend.widgets-2+.hpp.cpp
    build/frontend.widgets-3+.hpp.cpp
    build/frontend.widgets-4+.hpp.cpp
    widgets/OBSBasicControls.hpp
    widgets/OBSBasicPreview.hpp
    widgets/OBSBasicStats.hpp
    widgets/OBSBasicStatusBar.hpp
    widgets/OBSMainWindow.hpp
    widgets/OBSProjector.hpp
    build/frontend.widgets-obsqtdisplay+.hpp.cpp
    widgets/OBSQTDisplay.hpp
    widgets/StatusBarWidget.hpp
)
