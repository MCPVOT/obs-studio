/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pulse/def.h>

/* Flags for the monitoring playback stream, kept free of context and
 * stream calls so they can be tested on their own.
 *
 * device_id is the monitoring device chosen in the settings: "default" to
 * play on whatever the default sink is, or the name of one sink. */
pa_stream_flags_t pulseaudio_monitor_stream_flags(const char *device_id);

/* Whether a sink event means the monitoring stream must be reopened.
 *
 * A stream on the chosen sink cannot move, so when that sink goes away
 * (unplugged) it is reopened on the default sink (fallback), as the server
 * would have moved it there. While on the fallback, a new sink may be the
 * chosen one coming back, so it is reopened on the chosen sink if present.
 *
 * event is the subscription event (facility and type), event_idx the sink
 * index it is about, sink_idx the sink the stream plays on
 * (PA_INVALID_INDEX if none). */
bool pulseaudio_monitor_reset_on_sink_event(pa_subscription_event_type_t event, uint32_t event_idx, uint32_t sink_idx,
					    bool fallback);
