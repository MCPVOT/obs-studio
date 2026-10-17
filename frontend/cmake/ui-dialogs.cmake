if(NOT TARGET OBS::idian)
  add_subdirectory("${CMAKE_SOURCE_DIR}/shared/qt/idian" "${CMAKE_BINARY_DIR}/shared/qt/idian")
endif()

target_link_libraries(obs-studio PRIVATE OBS::idian)

if(NOT TARGET OBS::properties-view)
  add_subdirectory("${CMAKE_SOURCE_DIR}/shared/properties-view" "${CMAKE_BINARY_DIR}/shared/properties-view")
endif()

target_link_libraries(obs-studio PRIVATE OBS::properties-view)

target_sources(
  obs-studio
  PRIVATE
    build/frontend.dialogs+.hpp.cpp
    dialogs/LogUploadDialog.hpp
    dialogs/NameDialog.hpp
    dialogs/OAuthLogin.hpp
    dialogs/OBSAbout.hpp
    dialogs/OBSBasicAdvAudio.hpp
    build/frontend.dialogs-obsbasicfilters+.hpp.cpp
    dialogs/OBSBasicFilters.hpp
    build/frontend.dialogs-obsbasicinteraction+.hpp.cpp
    dialogs/OBSBasicInteraction.hpp
    build/frontend.dialogs-obsbasicproperties+.hpp.cpp
    dialogs/OBSBasicProperties.hpp
    dialogs/OBSBasicSourceSelect.hpp
    dialogs/OBSBasicTransform.hpp
    dialogs/OBSBasicVCamConfig.hpp
    dialogs/OBSLogViewer.hpp
    dialogs/OBSMissingFiles.hpp
    build/frontend.dialogs-2+.hpp.cpp
    dialogs/OBSRemux.hpp
    dialogs/OBSWhatsNew.hpp
)
