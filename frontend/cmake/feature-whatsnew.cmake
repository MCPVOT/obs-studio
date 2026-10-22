include_guard(DIRECTORY)

# Off by default: the dialog fetches upstream release notes from obsproject.com (#1732).
option(ENABLE_WHATSNEW "Enable WhatsNew dialog" OFF)

if(ENABLE_WHATSNEW AND TARGET OBS::browser-panels)
  obs_frontend_platform_whatsnew()

  target_enable_feature(obs-studio "What's New panel" WHATSNEW_ENABLED)
endif()
