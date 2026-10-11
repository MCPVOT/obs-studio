# Determine the AppStream release date without changing an explicit override.
if(NOT DEFINED APPDATA_RELEASE_DATE)
  if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
      COMMAND git log --tags -1 --pretty=%cd --date=short
      OUTPUT_VARIABLE APPDATA_RELEASE_DATE
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # GitHub forks may have no release tags. Use the latest commit date in that case.
    if(NOT APPDATA_RELEASE_DATE)
      execute_process(
        COMMAND git log -1 --pretty=%cd --date=short
        OUTPUT_VARIABLE APPDATA_RELEASE_DATE
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
    endif()
  endif()

  # Source archives and empty repositories have no Git commit to inspect.
  if(NOT APPDATA_RELEASE_DATE)
    if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/.CMakeBuildNumber")
      file(TIMESTAMP "${CMAKE_SOURCE_DIR}/cmake/.CMakeBuildNumber" APPDATA_RELEASE_DATE "%Y-%m-%d")
    else()
      file(TIMESTAMP "${CMAKE_SOURCE_DIR}/CMakeLists.txt" APPDATA_RELEASE_DATE "%Y-%m-%d")
    endif()
  endif()
endif()
