# Provision the same checksum-verified soldr used by native builds.
include("${CMAKE_CURRENT_LIST_DIR}/soldr.cmake")
obs_soldr_find(soldr)
get_filename_component(bin "${soldr}" DIRECTORY)
if(DEFINED ENV{GITHUB_PATH})
  file(APPEND "$ENV{GITHUB_PATH}" "${bin}\n")
endif()
