include_guard(GLOBAL)

set(OBS_DESKTOP_ID com.obsproject.Studio)
set(OBS_MACOS_BUNDLE_ID com.obsproject.obs-studio)
set(OBS_MACOS_BUNDLE_NAME OBS)
set(OBS_UNIX_GUI_NAME obs)
set(OBS_APP_DISPLAY_NAME "OBS Studio")
# Developer and links in the Linux metainfo and .deb (#2302).
set(OBS_PROJECT_DEVELOPER_ID com.obsproject)
set(OBS_PROJECT_DEVELOPER_NAME "OBS Project")
set(OBS_PROJECT_HOMEPAGE https://obsproject.com)
set(OBS_PROJECT_REPOSITORY https://github.com/obsproject/obs-studio)

if(NOT DEFINED OBS_BRAND_RUST)
  set(OBS_BRAND_RUST "${ENABLE_RUST_LIBOBS}")
endif()

if(OBS_BRAND_RUST)
  set(OBS_DESKTOP_ID io.github.obs_rust.OBSStudioRust)
  set(OBS_MACOS_BUNDLE_ID io.github.obs_rust.OBSStudioRust)
  set(OBS_MACOS_BUNDLE_NAME OBS-Studio-Rust)
  # obs-studio-rust is already the websocket CLI in this repository.
  set(OBS_UNIX_GUI_NAME obs-studio-rust-gui)
  set(OBS_APP_DISPLAY_NAME OBS-Studio-Rust)
  set(OBS_PROJECT_DEVELOPER_ID io.github.obs_rust)
  set(OBS_PROJECT_DEVELOPER_NAME obs-rust)
  set(OBS_PROJECT_HOMEPAGE https://github.com/obs-rust/obs-studio)
  set(OBS_PROJECT_REPOSITORY https://github.com/obs-rust/obs-studio)
endif()

set(OBS_CAMERA_EXTENSION_ID "${OBS_MACOS_BUNDLE_ID}.mac-camera-extension")
