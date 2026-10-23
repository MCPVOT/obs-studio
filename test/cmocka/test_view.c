/* Characterization of libobs/obs-view.c.h (#1933): obs_view_create(),
 * _destroy(), _get_source(), _set_source(), _render(), _add(), _remove() and
 * _enum_video_info(). It pins the CURRENT behaviour before the Rust port,
 * through the public API only, so the same test runs on libobs with the C
 * backend, the hybrid libobs and libobs-c-reference. Cases marked
 * "characterized, not endorsed" are odd but real.
 *
 * No video is reset, so no graphics device exists: the cases never render a
 * live source and never create a mix. obs_view_render() is covered for a
 * removed source, which it drops without rendering.
 *
 * obs_startup() needs a display on Linux, so the test runs under xvfb-run
 * there. It skips only when no display is set at all; with one set, a failed
 * obs_startup() fails the test. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <obs.h>
#include <util/base.h>
#include <util/bmem.h>

#define SRC_ID "test_view_source"

static int destroys;

static const char *src_name(void *type_data)
{
	(void)type_data;
	return "View test source";
}

static void *src_create(obs_data_t *settings, obs_source_t *source)
{
	(void)settings;
	(void)source;
	return bzalloc(1);
}

static void src_destroy(void *data)
{
	destroys++;
	bfree(data);
}

static uint32_t src_size(void *data)
{
	(void)data;
	return 0;
}

static bool obs_up;

static int group_setup(void **state)
{
	(void)state;
	obs_up = obs_startup("en-US", NULL, NULL);
	if (!obs_up) {
		if (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY")) {
			fprintf(stderr, "obs_startup failed with a display set\n");
			return -1;
		}
		fprintf(stderr, "skipped: obs_startup failed (no display?)\n");
		return 0;
	}

	struct obs_source_info info = {
		.id = SRC_ID,
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_VIDEO,
		.get_name = src_name,
		.create = src_create,
		.destroy = src_destroy,
		.get_width = src_size,
		.get_height = src_size,
	};
	obs_register_source(&info);
	/* A rejected registration would leave a "missing" placeholder source
	 * that never calls destroy. */
	if (!obs_source_get_display_name(SRC_ID)) {
		fprintf(stderr, "obs_register_source failed\n");
		return -1;
	}
	return 0;
}

static int group_teardown(void **state)
{
	(void)state;
	if (obs_up)
		obs_shutdown();
	obs_up = false;
	return 0;
}

/* Sources are destroyed on libobs's destruction thread: the cases wait for
 * it before counting. */
static int setup(void **state)
{
	(void)state;
	if (obs_up)
		obs_wait_for_destroy_queue();
	destroys = 0;
	return 0;
}

#define REQUIRE_OBS()           \
	do {                    \
		if (!obs_up)    \
			skip(); \
	} while (false)

static obs_source_t *new_source(const char *name)
{
	obs_source_t *s = obs_source_create_private(SRC_ID, name, NULL);
	assert_non_null(s);
	return s;
}

static void test_create_destroy(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_view_t *view = obs_view_create();
	assert_non_null(view);
	for (uint32_t ch = 0; ch < MAX_CHANNELS; ch++)
		assert_null(obs_view_get_source(view, ch));
	obs_view_destroy(view);
	obs_view_destroy(NULL);
}

static void test_set_get_source(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_view_t *view = obs_view_create();
	obs_source_t *s = new_source("a");

	obs_view_set_source(view, 3, s);
	/* An aux view shows the source but does not activate it. */
	assert_true(obs_source_showing(s));
	assert_false(obs_source_active(s));

	obs_source_t *got = obs_view_get_source(view, 3);
	assert_ptr_equal(got, s);
	obs_source_release(got);
	assert_null(obs_view_get_source(view, 2));

	/* The view holds its own reference. */
	obs_source_release(s);
	obs_wait_for_destroy_queue();
	assert_int_equal(destroys, 0);

	obs_view_destroy(view);
	obs_wait_for_destroy_queue();
	assert_int_equal(destroys, 1);
}

static void test_replace_and_clear(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_view_t *view = obs_view_create();
	obs_source_t *a = new_source("a");
	obs_source_t *b = new_source("b");

	obs_view_set_source(view, 0, a);
	obs_view_set_source(view, 0, b);
	assert_false(obs_source_showing(a));
	assert_true(obs_source_showing(b));

	/* The same source in two channels shows twice, hides once per clear. */
	obs_view_set_source(view, 1, b);
	obs_view_set_source(view, 0, NULL);
	assert_true(obs_source_showing(b));
	obs_view_set_source(view, 1, NULL);
	assert_false(obs_source_showing(b));
	assert_null(obs_view_get_source(view, 0));

	/* Setting the source a channel already has keeps it showing. */
	obs_view_set_source(view, 5, a);
	obs_view_set_source(view, 5, a);
	assert_true(obs_source_showing(a));

	obs_source_release(a);
	obs_source_release(b);
	obs_wait_for_destroy_queue();
	assert_int_equal(destroys, 1);
	obs_view_destroy(view);
	obs_wait_for_destroy_queue();
	assert_int_equal(destroys, 2);
}

static void test_out_of_range_and_null(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_source_t *s = new_source("range");

	/* Release builds check the channel after the assert. */
#ifdef NDEBUG
	obs_view_t *view = obs_view_create();
	obs_view_set_source(view, MAX_CHANNELS, s);
	assert_false(obs_source_showing(s));
	assert_null(obs_view_get_source(view, MAX_CHANNELS));
	obs_view_destroy(view);
#endif

	obs_view_set_source(NULL, 0, s);
	assert_false(obs_source_showing(s));
	assert_null(obs_view_get_source(NULL, 0));
	obs_view_render(NULL);
	assert_null(obs_view_add(NULL));
	assert_null(obs_view_add2(NULL, NULL));
	obs_view_remove(NULL);

	obs_source_release(s);
}

static void test_render_drops_removed(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_view_t *view = obs_view_create();
	obs_source_t *s = new_source("removed");
	obs_view_set_source(view, 2, s);
	obs_source_remove(s);

	obs_view_render(view);
	/* Characterized, not endorsed: the dropped channel releases its ref but
	 * never deactivates, so the source stays "showing". */
	assert_null(obs_view_get_source(view, 2));
	assert_true(obs_source_showing(s));

	obs_source_release(s);
	obs_wait_for_destroy_queue();
	assert_int_equal(destroys, 1);
	obs_view_destroy(view);
}

static int enum_calls;
static bool count_mix(void *param, struct obs_video_info *ovi)
{
	(void)param;
	(void)ovi;
	enum_calls++;
	return true;
}

static void test_mixes_without_video(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_view_t *view = obs_view_create();
	/* No video reset: the main canvas has no mix to copy. */
	assert_null(obs_view_add(view));

	enum_calls = 0;
	obs_view_enum_video_info(view, count_mix, NULL);
	assert_int_equal(enum_calls, 0);
	obs_view_remove(view);
	obs_view_destroy(view);
}

#define CASE(fn) cmocka_unit_test_setup(fn, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		CASE(test_create_destroy),        CASE(test_set_get_source),       CASE(test_replace_and_clear),
		CASE(test_out_of_range_and_null), CASE(test_render_drops_removed), CASE(test_mixes_without_video),
	};
	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
