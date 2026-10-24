/* A filter's size asked while it has a parent but no target (#2172).
 *
 * A filter without a size callback reports the size of its target, and
 * get_base_width()/get_base_height() followed filter_target without checking
 * it. obs_source_restore_filters() sets each filter's parent as it loads it,
 * and the filter's target only when it loads the next one (the last filter's
 * after the loop), so while the next filter loads, the previous one is a
 * filter with a parent and no target: asking its size then dereferenced
 * NULL. The Filters dialog, a script or a scene item reading the size from
 * another thread in that window crashed the same way.
 *
 * Racing another thread against the restore would not be deterministic, so
 * the second filter's create callback asks the first filter's size itself:
 * obs_source_restore_filters() loads the second filter inside that window.
 * The size must be 0 there (no target to ask), and once the restore is done
 * both filters report the source's size, as before. Needs no video;
 * obs_startup() needs a display on Linux, so the case skips without one. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>

#include <obs.h>

#define BOX_ID "filter_size_no_target_box"
#define PASS_ID "filter_size_no_target_pass"
#define PROBE_ID "filter_size_no_target_probe"

static bool obs_up;

/* The first filter, recorded by its create callback. */
static obs_source_t *first_filter;
static bool probed;
static uint32_t probed_cx;
static uint32_t probed_cy;

static const char *type_name(void *unused)
{
	(void)unused;
	return "filter size no target";
}

static void *data_create(obs_data_t *settings, obs_source_t *source)
{
	(void)settings;
	return source;
}

static void *pass_create(obs_data_t *settings, obs_source_t *source)
{
	(void)settings;
	first_filter = source;
	return source;
}

/* Runs inside obs_source_restore_filters(), after the first filter got its
 * parent and before it got its target. */
static void *probe_create(obs_data_t *settings, obs_source_t *source)
{
	(void)settings;
	if (first_filter) {
		probed_cx = obs_source_get_width(first_filter);
		probed_cy = obs_source_get_height(first_filter);
		probed = true;
	}
	return source;
}

static void data_destroy(void *data)
{
	(void)data;
}

static uint32_t box_width(void *data)
{
	(void)data;
	return 64;
}

static uint32_t box_height(void *data)
{
	(void)data;
	return 48;
}

static void register_types(void)
{
	struct obs_source_info box = {
		.id = BOX_ID,
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_VIDEO,
		.get_name = type_name,
		.create = data_create,
		.destroy = data_destroy,
		.get_width = box_width,
		.get_height = box_height,
	};
	/* No size callbacks: both filters report their target's size. */
	struct obs_source_info pass = {
		.id = PASS_ID,
		.type = OBS_SOURCE_TYPE_FILTER,
		.output_flags = OBS_SOURCE_VIDEO,
		.get_name = type_name,
		.create = pass_create,
		.destroy = data_destroy,
	};
	struct obs_source_info probe = pass;
	probe.id = PROBE_ID;
	probe.create = probe_create;
	obs_register_source(&box);
	obs_register_source(&pass);
	obs_register_source(&probe);
}

static void push_filter(obs_data_array_t *array, const char *id, const char *name)
{
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "id", id);
	obs_data_set_string(data, "name", name);
	obs_data_array_push_back(array, data);
	obs_data_release(data);
}

static void restore_asks_size_of_filter_without_target(void **state)
{
	(void)state;
	if (!obs_up)
		skip();

	obs_source_t *box = obs_source_create(BOX_ID, "box", NULL, NULL);
	assert_non_null(box);
	obs_data_array_t *array = obs_data_array_create();
	push_filter(array, PASS_ID, "pass");
	push_filter(array, PROBE_ID, "probe");

	obs_source_restore_filters(box, array);
	obs_data_array_release(array);

	assert_true(probed);
	assert_int_equal(probed_cx, 0);
	assert_int_equal(probed_cy, 0);

	/* Attached, both filters report the box's size. */
	obs_source_t *pass = obs_source_get_filter_by_name(box, "pass");
	obs_source_t *probe = obs_source_get_filter_by_name(box, "probe");
	assert_non_null(pass);
	assert_non_null(probe);
	assert_int_equal(obs_source_get_width(pass), 64);
	assert_int_equal(obs_source_get_height(pass), 48);
	assert_int_equal(obs_source_get_width(probe), 64);
	assert_int_equal(obs_source_get_height(probe), 48);
	assert_int_equal(obs_source_get_width(box), 64);
	assert_int_equal(obs_source_get_height(box), 48);
	obs_source_release(pass);
	obs_source_release(probe);

	first_filter = NULL;
	obs_source_release(box);
}

static int group_setup(void **state)
{
	(void)state;
	obs_up = obs_startup("en-US", NULL, NULL);
	if (obs_up)
		register_types();
	else
		fprintf(stderr, "obs_startup failed (no display?), skipping\n");
	return 0;
}

static int group_teardown(void **state)
{
	(void)state;
	if (obs_up)
		obs_shutdown();
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(restore_asks_size_of_filter_without_target),
	};
	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
