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

#include <string.h>

#include "pulseaudio-stream-flags.h"

pa_stream_flags_t pulseaudio_monitor_stream_flags(const char *device_id)
{
	(void)device_id;
	return PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_START_CORKED;
}

bool pulseaudio_monitor_reset_on_sink_event(pa_subscription_event_type_t event, uint32_t event_idx, uint32_t sink_idx,
					    bool fallback)
{
	(void)event;
	(void)event_idx;
	(void)sink_idx;
	(void)fallback;
	return false;
}
