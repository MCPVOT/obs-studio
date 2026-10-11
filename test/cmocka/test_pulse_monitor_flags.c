#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include "pulseaudio-stream-flags.h"

/* The audio monitoring device chosen in Settings -> Audio -> Advanced does
 * not stick on PulseAudio: switching the system's default output moves
 * OBS's monitoring stream along with every other stream, so monitored audio
 * ends up on the new default sink although the settings still name another
 * device (obsproject/obs-studio#10342, reported by gentoo-root).
 *
 * The playback stream is connected to the chosen sink by name, but without
 * PA_STREAM_DONT_MOVE the server (pipewire-pulse with WirePlumber, or
 * PulseAudio) is free to move it when the default changes. The capture side
 * already asks it not to (linux-pulseaudio: "Use DONT_MOVE for non-default
 * devices", obsproject/obs-studio#3211).
 *
 * "default" is resolved to the current default sink's name before the
 * stream connects, so a stream for "default" must stay movable: following
 * the default sink is what the user asked for.
 *
 * A stream that cannot move does not follow its sink away either: when the
 * chosen sink is unplugged, pipewire-pulse leaves it unlinked and PulseAudio
 * kills it, and it stays silent after the sink returns. Without
 * PA_STREAM_DONT_MOVE the server moved it to the default sink and back once
 * the sink returned, so OBS must now do that itself on sink events. */

#define BASE_FLAGS (PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_START_CORKED)

static void check_base_flags(const char *id, pa_stream_flags_t flags)
{
	if ((flags & BASE_FLAGS) != BASE_FLAGS) {
		fprintf(stderr, "monitoring stream for '%s' lost its timing/corked flags: 0x%x\n", id, (unsigned)flags);
		fail();
	}
}

static void chosen_sink_is_not_moved(void **state)
{
	(void)state;
	const char *ids[] = {"alsa_output.usb-headphones.analog-stereo", "obs_record", "sinkB"};

	for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
		pa_stream_flags_t flags = pulseaudio_monitor_stream_flags(ids[i]);
		check_base_flags(ids[i], flags);
		if (!(flags & PA_STREAM_DONT_MOVE)) {
			fprintf(stderr,
				"monitoring stream for chosen sink '%s' lacks PA_STREAM_DONT_MOVE (flags 0x%x): "
				"it follows the default sink instead of staying on the chosen device\n",
				ids[i], (unsigned)flags);
			fail();
		}
	}
}

static void default_device_follows_default_sink(void **state)
{
	(void)state;
	pa_stream_flags_t flags = pulseaudio_monitor_stream_flags("default");

	check_base_flags("default", flags);
	if (flags & PA_STREAM_DONT_MOVE) {
		fprintf(stderr,
			"monitoring stream for 'default' must follow the default sink, "
			"but has PA_STREAM_DONT_MOVE (flags 0x%x)\n",
			(unsigned)flags);
		fail();
	}
}

#define SINK_NEW (PA_SUBSCRIPTION_EVENT_SINK | PA_SUBSCRIPTION_EVENT_NEW)
#define SINK_CHANGE (PA_SUBSCRIPTION_EVENT_SINK | PA_SUBSCRIPTION_EVENT_CHANGE)
#define SINK_REMOVE (PA_SUBSCRIPTION_EVENT_SINK | PA_SUBSCRIPTION_EVENT_REMOVE)
#define SINK_INPUT_REMOVE (PA_SUBSCRIPTION_EVENT_SINK_INPUT | PA_SUBSCRIPTION_EVENT_REMOVE)

static void check_reset(const char *what, bool want, pa_subscription_event_type_t event, uint32_t event_idx,
			uint32_t sink_idx, bool fallback)
{
	bool got = pulseaudio_monitor_reset_on_sink_event(event, event_idx, sink_idx, fallback);
	if (got != want) {
		fprintf(stderr, "%s: monitoring stream %s be reopened (event 0x%x for sink %u, on sink %u%s)\n", what,
			want ? "must" : "must not", (unsigned)event, event_idx, sink_idx, fallback ? ", fallback" : "");
		fail();
	}
}

static void unplugged_sink_falls_back_to_default(void **state)
{
	(void)state;
	check_reset("chosen sink removed", true, SINK_REMOVE, 7, 7, false);
}

static void replugged_sink_is_used_again(void **state)
{
	(void)state;
	check_reset("sink added while on the fallback", true, SINK_NEW, 9, 3, true);
}

static void unrelated_sink_events_are_ignored(void **state)
{
	(void)state;
	check_reset("another sink removed", false, SINK_REMOVE, 8, 7, false);
	check_reset("sink added while on the chosen sink", false, SINK_NEW, 9, 7, false);
	check_reset("chosen sink changed", false, SINK_CHANGE, 7, 7, false);
	/* The fallback stream can move, the server takes care of it. */
	check_reset("fallback sink removed", false, SINK_REMOVE, 3, 3, true);
	check_reset("sink input removed", false, SINK_INPUT_REMOVE, 7, 7, false);
	check_reset("no stream", false, SINK_REMOVE, 7, PA_INVALID_INDEX, false);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(chosen_sink_is_not_moved),
		cmocka_unit_test(default_device_follows_default_sink),
		cmocka_unit_test(unplugged_sink_falls_back_to_default),
		cmocka_unit_test(replugged_sink_is_used_again),
		cmocka_unit_test(unrelated_sink_events_are_ignored),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
