cmake_minimum_required(VERSION 3.28...3.30)

find_package(Qt6 REQUIRED Core Widgets)

add_library(qt-obs-spinbox INTERFACE)
add_library(OBS::qt-obs-spinbox ALIAS qt-obs-spinbox)

target_sources(
  qt-obs-spinbox
  INTERFACE build/qt-obs-spinbox.core+.hpp.cpp AutoSelectLineEdit.hpp DoubleSpinBox.hpp SpinBox.hpp
)
target_include_directories(qt-obs-spinbox INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(qt-obs-spinbox INTERFACE Qt::Core Qt::Widgets)
