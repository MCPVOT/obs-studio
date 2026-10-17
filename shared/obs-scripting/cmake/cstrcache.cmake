add_library(obs-cstrcache INTERFACE)
add_library(OBS::cstrcache ALIAS obs-cstrcache)

target_sources(obs-cstrcache INTERFACE build/obs-scripting.cstrcache+.hpp.cpp cstrcache.h)
target_include_directories(obs-cstrcache INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
