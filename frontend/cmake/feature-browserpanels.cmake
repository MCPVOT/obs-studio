if(TARGET OBS::browser-panels)
  target_enable_feature(obs-studio "Browser panels" BROWSER_AVAILABLE)

  target_link_libraries(obs-studio PRIVATE OBS::browser-panels)

  target_sources(
    obs-studio
    PRIVATE
      build/frontend.browserpanels+.hpp.cpp
      dialogs/OBSExtraBrowsers.hpp
      docks/BrowserDock.hpp
      utility/ExtraBrowsersDelegate.hpp
      utility/ExtraBrowsersModel.hpp
  )
endif()
