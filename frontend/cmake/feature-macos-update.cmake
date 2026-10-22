include_guard(DIRECTORY)

find_package(nlohmann_json 3.11 REQUIRED)

if(NOT TARGET OBS::blake2)
  add_subdirectory("${CMAKE_SOURCE_DIR}/deps/blake2" "${CMAKE_BINARY_DIR}/deps/blake2")
endif()

obs_frontend_add_sources(MACOS_UPDATE)

target_link_libraries(
  obs-studio
  PRIVATE "$<LINK_LIBRARY:FRAMEWORK,Security.framework>" nlohmann_json::nlohmann_json OBS::blake2
)
