option(ENABLE_IDIAN_PLAYGROUND "Enable building custom idian widget demo window" OFF)

if(ENABLE_IDIAN_PLAYGROUND)
  obs_frontend_add_sources(IDIAN_PLAYGROUND)
  target_enable_feature(obs-studio "Idian Playground" ENABLE_IDIAN_PLAYGROUND)
else()
  target_disable_feature(obs-studio "Idian Playground")
endif()
