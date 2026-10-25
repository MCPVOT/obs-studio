/* linux-capture: the XSHM screen capture keeps capturing the monitor it was
 * set to when another monitor is unplugged or plugged in
 * (obs-rust/obs-studio#2261, the Linux form of upstream
 * obsproject/obs-studio#5502). It saved the monitor as its position in the
 * RandR monitor list, so unplugging a monitor earlier in the list made it
 * capture the next one, silently.
 *
 * This drives the real source (the linux-capture unity file is compiled in)
 * against the Xvfb display ctest runs under. It adds three RandR 1.5
 * monitors of different sizes, A, B and C, as xrandr --setmonitor does,
 * captures B, then removes A and B in turn. The monitors' sizes tell which
 * one the source captures. It changes the monitors everyone on that display
 * sees, so the test runs alone (RUN_SERIAL). Skips without a display,
 * RandR 1.5 or MIT-SHM. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/shm.h>

#include <obs.h>
#include <util/platform.h>

extern struct obs_source_info xshm_input_v2;

static bool obs_up;
static xcb_connection_t *conn;
static xcb_window_t root;
static xcb_randr_output_t output;

struct monitor {
	const char *name;
	int16_t x;
	uint16_t w, h;
	bool takes_output;
	xcb_atom_t atom;
};

/* Different sizes, so the source's size names the monitor it captures. */
static struct monitor mon_a = {"OBSTEST-A", 0, 640, 360, true, XCB_NONE};
static struct monitor mon_b = {"OBSTEST-B", 640, 320, 200, false, XCB_NONE};
static struct monitor mon_c = {"OBSTEST-C", 960, 400, 300, false, XCB_NONE};

static xcb_atom_t intern(const char *name)
{
	xcb_intern_atom_reply_t *r =
		xcb_intern_atom_reply(conn, xcb_intern_atom(conn, false, (uint16_t)strlen(name), name), NULL);
	xcb_atom_t atom = r ? r->atom : XCB_NONE;
	free(r);
	return atom;
}

static bool add_monitor(struct monitor *m)
{
	m->atom = intern(m->name);
	xcb_randr_monitor_info_t info = {0};
	info.name = m->atom;
	info.automatic = 0;
	info.x = m->x;
	info.y = 0;
	info.width = m->w;
	info.height = m->h;
	info.width_in_millimeters = m->w * 254 / 960;
	info.height_in_millimeters = m->h * 254 / 960;
	info.nOutput = m->takes_output ? 1 : 0;

	/* The request carries the outputs after the fixed part. */
	size_t size = sizeof(info) + info.nOutput * sizeof(xcb_randr_output_t);
	xcb_randr_monitor_info_t *buf = calloc(1, size);
	memcpy(buf, &info, sizeof(info));
	if (m->takes_output)
		*(xcb_randr_output_t *)(buf + 1) = output;
	xcb_generic_error_t *err = xcb_request_check(conn, xcb_randr_set_monitor_checked(conn, root, buf));
	free(buf);
	free(err);
	return !err;
}

static void remove_monitor(struct monitor *m)
{
	if (m->atom == XCB_NONE)
		return;
	free(xcb_request_check(conn, xcb_randr_delete_monitor_checked(conn, root, m->atom)));
}

static int group_setup(void **state)
{
	(void)state;

	const char *display = getenv("DISPLAY");
	if (!display || !*display)
		return 0;

	conn = xcb_connect(NULL, NULL);
	if (xcb_connection_has_error(conn)) {
		fprintf(stderr, "cannot connect to the X display\n");
		return -1;
	}
	root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;

	const xcb_query_extension_reply_t *randr = xcb_get_extension_data(conn, &xcb_randr_id);
	const xcb_query_extension_reply_t *shm = xcb_get_extension_data(conn, &xcb_shm_id);
	if (!randr || !randr->present || !shm || !shm->present) {
		fprintf(stderr, "the X server has no RandR or MIT-SHM, skipping\n");
		return 0;
	}
	xcb_randr_query_version_reply_t *ver =
		xcb_randr_query_version_reply(conn, xcb_randr_query_version(conn, 1, 5), NULL);
	const bool monitors = ver && (ver->major_version > 1 || ver->minor_version >= 5);
	free(ver);
	if (!monitors) {
		fprintf(stderr, "the X server has no RandR 1.5 monitors, skipping\n");
		return 0;
	}

	xcb_randr_get_screen_resources_reply_t *res =
		xcb_randr_get_screen_resources_reply(conn, xcb_randr_get_screen_resources(conn, root), NULL);
	if (!res || !res->num_outputs) {
		fprintf(stderr, "the X server has no RandR output, skipping\n");
		free(res);
		return 0;
	}
	output = xcb_randr_get_screen_resources_outputs(res)[0];
	free(res);

	if (!obs_startup("en-US", NULL, NULL))
		return 0;

	struct obs_video_info ovi = {
		.graphics_module = GRAPHICS_MODULE_PATH,
		.fps_num = 30,
		.fps_den = 1,
		.base_width = 320,
		.base_height = 240,
		.output_width = 320,
		.output_height = 240,
		.output_format = VIDEO_FORMAT_BGRA,
		.colorspace = VIDEO_CS_SRGB,
		.range = VIDEO_RANGE_FULL,
		.scale_type = OBS_SCALE_BICUBIC,
	};
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS) {
		obs_shutdown();
		return 0;
	}
	obs_register_source(&xshm_input_v2);
	obs_up = true;
	return 0;
}

static int group_teardown(void **state)
{
	(void)state;
	if (obs_up) {
		obs_shutdown();
		obs_up = false;
	}
	if (conn)
		xcb_disconnect(conn);
	return 0;
}

static int setup(void **state)
{
	(void)state;
	if (!obs_up)
		return 0;
	if (!add_monitor(&mon_a) || !add_monitor(&mon_b) || !add_monitor(&mon_c)) {
		fprintf(stderr, "cannot add RandR monitors\n");
		return -1;
	}
	return 0;
}

static int teardown(void **state)
{
	(void)state;
	if (!obs_up)
		return 0;
	remove_monitor(&mon_a);
	remove_monitor(&mon_b);
	remove_monitor(&mon_c);
	xcb_flush(conn);
	return 0;
}

static bool has_size(obs_source_t *source, const struct monitor *m)
{
	return obs_source_get_width(source) == m->w && obs_source_get_height(source) == m->h;
}

/* Milliseconds until the source has m's size, or -1 after timeout_ms. */
static int wait_monitor(obs_source_t *source, const struct monitor *m, int timeout_ms)
{
	const uint64_t start = os_gettime_ns();
	for (;;) {
		const int elapsed = (int)((os_gettime_ns() - start) / 1000000);
		if (has_size(source, m))
			return elapsed;
		if (elapsed > timeout_ms)
			return -1;
		os_sleep_ms(10);
	}
}

static obs_source_t *capture(int screen)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "screen", screen);
	obs_data_set_bool(settings, "show_cursor", false);
	obs_source_t *source = obs_source_create_private("xshm_input_v2", "xshm hotplug", settings);
	obs_data_release(settings);
	obs_source_inc_showing(source);
	return source;
}

static void release(obs_source_t *source)
{
	obs_source_dec_showing(source);
	obs_source_release(source);
}

static void print_size(const char *when, obs_source_t *source)
{
	fprintf(stderr, "%s: source %ux%u\n", when, obs_source_get_width(source), obs_source_get_height(source));
}

/* Unplugging the monitor left of the captured one keeps the capture on it,
 * and so does plugging it back in (now listed last). */
static void unplug_other_monitor_keeps_capture(void **state)
{
	(void)state;
	if (!obs_up)
		skip();

	obs_source_t *source = capture(1);
	assert_non_null(source);
	assert_true(wait_monitor(source, &mon_b, 2000) >= 0);
	print_size("captures B", source);

	remove_monitor(&mon_a);
	xcb_flush(conn);
	/* Give the source time to see the RandR event and follow it. */
	os_sleep_ms(500);
	print_size("A unplugged", source);
	const bool kept_after_unplug = has_size(source, &mon_b);

	assert_true(add_monitor(&mon_a));
	os_sleep_ms(500);
	print_size("A plugged back", source);
	const bool kept_after_replug = has_size(source, &mon_b);

	release(source);
	assert_true(kept_after_unplug);
	assert_true(kept_after_replug);
}

/* When the captured monitor itself goes away, the source does not move to
 * another monitor. */
static void unplug_captured_monitor_shows_no_other(void **state)
{
	(void)state;
	if (!obs_up)
		skip();

	obs_source_t *source = capture(1);
	assert_non_null(source);
	assert_true(wait_monitor(source, &mon_b, 2000) >= 0);

	remove_monitor(&mon_b);
	xcb_flush(conn);
	const int ms = wait_monitor(source, &mon_c, 1000);
	print_size("B unplugged", source);

	release(source);
	assert_int_equal(ms, -1);
}

/* Choosing another monitor in the properties still switches to it. */
static void choosing_another_monitor_switches(void **state)
{
	(void)state;
	if (!obs_up)
		skip();

	obs_source_t *source = capture(1);
	assert_non_null(source);
	assert_true(wait_monitor(source, &mon_b, 2000) >= 0);

	obs_data_t *settings = obs_source_get_settings(source);
	obs_data_set_int(settings, "screen", 2);
	obs_source_update(source, settings);
	obs_data_release(settings);
	const int ms = wait_monitor(source, &mon_c, 2000);
	print_size("chose C", source);

	release(source);
	assert_true(ms >= 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(unplug_other_monitor_keeps_capture, setup, teardown),
		cmocka_unit_test_setup_teardown(unplug_captured_monitor_shows_no_other, setup, teardown),
		cmocka_unit_test_setup_teardown(choosing_another_monitor_switches, setup, teardown),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
