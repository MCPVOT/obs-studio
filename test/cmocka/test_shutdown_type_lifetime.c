/* obs_shutdown() must not free a source type's registration while sources
 * of that type are still alive (#2017).
 *
 * A host that does not release every source before obs_shutdown() leaves
 * them to obs_free_data(), which destroys them. obs_shutdown() used to free
 * the registered types (their id strings and type_data) first, so those
 * destroys, and obs_canvas_destroy()'s obs_source_is_scene() on a scene the
 * main canvas still holds, read freed memory (ASan: heap-use-after-free in
 * test_context_names).
 *
 * The test registers an input type whose free_type_data marks the type as
 * gone and whose destroy records whether that already happened, leaves one
 * source of it alive, plus a scene in the main canvas, and shuts down.
 * Public API only, so it runs against libobs, the hybrid libobs and
 * libobs-c-reference. obs_startup() needs a display on Linux; without one
 * the case skips. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <string.h>

#include <obs.h>

#define INPUT_ID "shutdown_type_lifetime_input"

static int type_marker;
static bool type_freed;
static bool destroyed;
static bool destroyed_after_type_freed;
static bool id_readable_at_destroy;

static const char *input_name(void *type_data)
{
	(void)type_data;
	return "Shutdown type lifetime input";
}

static void *input_create(obs_data_t *settings, obs_source_t *source)
{
	(void)settings;
	return source;
}

static void input_destroy(void *data)
{
	obs_source_t *source = data;
	destroyed = true;
	destroyed_after_type_freed = type_freed;
	/* Reads the registration's id string, as obs_source_is_scene() does. */
	const char *id = obs_source_get_id(source);
	id_readable_at_destroy = id && strcmp(id, INPUT_ID) == 0;
}

static void free_type_data(void *type_data)
{
	assert_ptr_equal(type_data, &type_marker);
	type_freed = true;
}

static void test_sources_outlive_shutdown_keep_their_type(void **state)
{
	(void)state;
	if (!obs_startup("en-US", NULL, NULL)) {
		fprintf(stderr, "skipped: obs_startup failed (no display?)\n");
		skip();
	}

	struct obs_source_info input = {
		.id = INPUT_ID,
		.type = OBS_SOURCE_TYPE_INPUT,
		.get_name = input_name,
		.create = input_create,
		.destroy = input_destroy,
		.type_data = &type_marker,
		.free_type_data = free_type_data,
	};
	obs_register_source(&input);

	/* Left alive on purpose: obs_free_data() destroys both. */
	obs_source_t *source = obs_source_create(INPUT_ID, "left alive", NULL, NULL);
	assert_non_null(source);
	obs_scene_t *scene = obs_scene_create("left in the main canvas");
	assert_non_null(scene);

	obs_shutdown();

	assert_true(destroyed);
	assert_true(type_freed);
	assert_false(destroyed_after_type_freed);
	assert_true(id_readable_at_destroy);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_sources_outlive_shutdown_keep_their_type),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
