find_package(Qt6 REQUIRED Widgets Network Svg Xml)

obs_frontend_platform_qt()

if(NOT TARGET OBS::qt-wrappers)
  add_subdirectory("${CMAKE_SOURCE_DIR}/shared/qt/wrappers" "${CMAKE_BINARY_DIR}/shared/qt/wrappers")
endif()

target_link_libraries(
  obs-studio
  PRIVATE Qt::Widgets Qt::Svg Qt::Xml Qt::Network OBS::qt-wrappers
)

set_target_properties(
  obs-studio
  PROPERTIES AUTOMOC TRUE AUTOUIC TRUE AUTORCC TRUE AUTOGEN_PARALLEL AUTO
)

# https://doc.qt.io/qt-6/qtglobal.html#QT_ENABLE_STRICT_MODE_UP_TO
target_compile_definitions(obs-studio PRIVATE QT_ENABLE_STRICT_MODE_UP_TO=0xFF0000)

set_property(TARGET obs-studio APPEND PROPERTY AUTOUIC_SEARCH_PATHS forms forms/source-toolbar)

obs_frontend_add_sources(UI_QT)
