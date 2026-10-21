#pragma once

#if defined(__linux__)
#include "plugins/decklink/linux/decklink-sdk/DeckLinkAPI.h"
#include "plugins/decklink/linux/decklink-sdk/DeckLinkAPIVersion.h"
typedef bool decklink_bool_t;
typedef const char *decklink_string_t;
#endif
