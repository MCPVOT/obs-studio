/* linux-capture: the XSHM screen capture (Screen Capture (XSHM)) moves its
 * cursor with the captured monitor when the monitor moves in the X screen
 * without changing size (#2265). The capture area followed the move, but
 * the cursor offset was only updated on a resize, so the cursor was drawn
 * relative to the monitor's old place.
 *
 * The linux-capture unity file is included here so the test can read the
 * source's state. It adds a RandR monitor to the Xvfb screen ctest runs
 * under, starts the source on it, moves the monitor without resizing it, and
 * checks that the cursor offset follows the capture area. Skips without a
 * display, RandR 1.5 monitors or MIT-SHM. */

#include "plugins/linux-capture/build/linux-capture.nix+.h.c"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <util/platform.h>

#define MON_W 320
#define MON_H 240

static bool obs_up;
static xcb_connection_t *conn;
static xcb_window_t root;
static xcb_atom_t mon_name;

static bool set_monitor(int16_t x)
{
	xcb_randr_monitor_info_t info = {0};
	info.name = mon_name;
	info.x = x;
	info.y = 0;
	info.width = MON_W;
	info.height = MON_H;
	info.width_in_millimeters = MON_W * 254 / 960;
	info.height_in_millimeters = MON_H * 254 / 960;
	info.nOutput = 0;

	xcb_generic_error_t *err = xcb_request_check(conn, xcb_randr_set_monitor_checked(conn, root, &info));
	free(err);
	xcb_flush(conn);
	return !err;
}

static void delete_monitor(void)
{
	free(xcb_request_check(conn, xcb_randr_delete_monitor_checked(conn, root, mon_name)));
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
	if (!randr || !randr->present || !shm || !shm->present || !randr_has_monitors(conn)) {
		fprintf(stderr, "the X server has no RandR monitors or MIT-SHM, skipping\n");
		return 0;
	}

	xcb_intern_atom_reply_t *atom =
		xcb_intern_atom_reply(conn, xcb_intern_atom(conn, 0, strlen("OBS-TEST-B"), "OBS-TEST-B"), NULL);
	if (!atom)
		return 0;
	mon_name = atom->atom;
	free(atom);

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
	if (conn) {
		if (mon_name)
			delete_monitor();
		xcb_disconnect(conn);
	}
	return 0;
}

/* The RandR screen index of the test monitor at x, or -1. */
static int find_monitor(int x)
{
	int count = randr_screen_count(conn);
	for (int i = 0; i < count; i++) {
		int_fast32_t mx, my, mw, mh;
		if (randr_screen_geo(conn, i, &mx, &my, &mw, &mh, NULL, NULL) < 0)
			continue;
		if (mx == x && my == 0 && mw == MON_W && mh == MON_H)
			return i;
	}
	return -1;
}

/* Milliseconds until the capture area starts at x, or -1 after timeout_ms. */
static int wait_area_x(struct xshm_data *data, int x, int timeout_ms)
{
	const uint64_t start = os_gettime_ns();
	for (;;) {
		const int elapsed = (int)((os_gettime_ns() - start) / 1000000);
		if (data->adj_x_org == x && data->adj_width == MON_W)
			return elapsed;
		if (elapsed > timeout_ms)
			return -1;
		os_sleep_ms(10);
	}
}

static void cursor_follows_monitor_move(void **state)
{
	(void)state;

	if (!obs_up)
		skip();

	assert_true(set_monitor(640));
	const int screen = find_monitor(640);
	if (screen < 0) {
		fprintf(stderr, "the test monitor is not listed, skipping\n");
		delete_monitor();
		skip();
	}

	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "screen", screen);
	obs_data_set_bool(settings, "show_cursor", true);
	obs_source_t *source = obs_source_create_private("xshm_input_v2", "xshm move", settings);
	obs_data_release(settings);
	assert_non_null(source);
	obs_source_inc_showing(source);

	struct xshm_data *data = obs_obj_get_data(source);
	assert_non_null(data);
	assert_true(wait_area_x(data, 640, 2000) >= 0);
	assert_non_null(data->cursor);
	fprintf(stderr, "start: area x %d, cursor x %d\n", (int)data->adj_x_org, data->cursor->x_org);
	assert_int_equal(data->cursor->x_org, 640);

	/* Same size, new place. */
	delete_monitor();
	assert_true(set_monitor(320));
	const int ms = wait_area_x(data, 320, 2000);
	os_sleep_ms(100);
	fprintf(stderr, "moved: area x %d after %d ms, cursor x %d\n", (int)data->adj_x_org, ms, data->cursor->x_org);
	const int area_x = (int)data->adj_x_org;
	const int cursor_x = data->cursor->x_org;

	obs_source_dec_showing(source);
	obs_source_release(source);
	delete_monitor();

	assert_true(ms >= 0);
	assert_int_equal(cursor_x, area_x);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(cursor_follows_monitor_move),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
