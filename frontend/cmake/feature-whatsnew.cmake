include_guard(DIRECTORY)

option(ENABLE_WHATSNEW "Enable WhatsNew dialog" ON)

if(ENABLE_WHATSNEW AND TARGET OBS::browser-panels)
  obs_frontend_platform_whatsnew()

  target_enable_feature(obs-studio "What's New panel" WHATSNEW_ENABLED)
endif()
