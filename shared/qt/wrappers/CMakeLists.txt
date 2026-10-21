cmake_minimum_required(VERSION 3.28...3.30)

find_package(Qt6 REQUIRED Core Widgets)

add_library(qt-wrappers INTERFACE)
add_library(OBS::qt-wrappers ALIAS qt-wrappers)

target_sources(qt-wrappers INTERFACE build/qt-wrappers.core+.hpp.cpp qt-wrappers.hpp)
target_include_directories(qt-wrappers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(qt-wrappers INTERFACE Qt::Core Qt::Widgets OBS::libobs)

# The selected implementation is compiled once by each interface consumer,
# alongside the existing shared wrapper group.
if(OS_WINDOWS)
  target_sources(qt-wrappers INTERFACE build/qt-wrappers.host-windows+.hpp.cpp)
elseif(OS_MACOS)
  target_sources(qt-wrappers INTERFACE build/qt-wrappers.host-macos+.hpp.cpp)
else()
  target_sources(qt-wrappers INTERFACE build/qt-wrappers.host-posix+.hpp.cpp)
endif()
