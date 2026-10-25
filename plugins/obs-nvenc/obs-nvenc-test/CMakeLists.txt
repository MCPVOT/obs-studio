cmake_minimum_required(VERSION 3.28...3.30)

find_package(FFnvcodec 12 REQUIRED)
# std::thread and dlopen: before glibc 2.34 they live in libpthread and libdl, not libc
find_package(Threads REQUIRED)

add_executable(obs-nvenc-test)

target_sources(obs-nvenc-test PRIVATE ${_obs_nvenc_helper})
target_link_libraries(obs-nvenc-test FFnvcodec::FFnvcodec Threads::Threads ${CMAKE_DL_LIBS})

if(OBS_NVENC_TEST_NAME)
  set_target_properties(obs-nvenc-test PROPERTIES OUTPUT_NAME "${OBS_NVENC_TEST_NAME}")
endif()

set_target_properties_obs(obs-nvenc-test PROPERTIES FOLDER plugins/obs-nvenc)
