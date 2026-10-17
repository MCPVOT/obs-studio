if(
  YOUTUBE_CLIENTID
  AND YOUTUBE_SECRET
  AND YOUTUBE_CLIENTID_HASH MATCHES "^(0|[a-fA-F0-9]+)$"
  AND YOUTUBE_SECRET_HASH MATCHES "^(0|[a-fA-F0-9]+)$"
  AND TARGET OBS::browser-panels
)
  target_sources(
    obs-studio
    PRIVATE
      build/frontend.youtube+.hpp.cpp
      dialogs/OBSYoutubeActions.hpp
      docks/YouTubeAppDock.hpp
      docks/YouTubeChatDock.hpp
      forms/OBSYoutubeActions.ui
      oauth/YoutubeAuth.hpp
      utility/YoutubeApiWrappers.hpp
  )

  target_enable_feature(obs-studio "YouTube API connection" YOUTUBE_ENABLED)
else()
  target_disable_feature(obs-studio "YouTube API connection")
  set(YOUTUBE_SECRET_HASH 0)
  set(YOUTUBE_CLIENTID_HASH 0)
endif()
