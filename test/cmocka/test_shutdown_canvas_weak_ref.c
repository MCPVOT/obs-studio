/* obs_shutdown() must free the weak canvas reference of every source it
 * destroys (#2038).
 *
 * obs_free_data() releases the main canvas before it destroys the sources
 * the host left alive. obs_canvas_remove_source() released a source's weak
 * canvas reference only while the canvas was still alive, so each leftover
 * source leaked it, and with it the canvas's control block. The canvas also
 * never freed its source name table while sources were left in it (ASan:
 * obs-canvas.c.h obs_canvas_insert_source and obs_canvas_create_internal).
 *
 * Both are bmalloc allocations, so bnum_allocs() after obs_shutdown() shows
 * them. Public API only, so it runs against libobs, the hybrid libobs and
 * libobs-c-reference. obs_startup() needs a display on Linux; without one
 * the case skips. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>

#include <obs.h>
#include <util/bmem.h>

#define INPUT_ID "shutdown_canvas_weak_ref_input"

static const char *input_name(void *type_data)
{
	(void)type_data;
	return "Shutdown canvas weak ref input";
}

static void *input_create(obs_data_t *settings, obs_source_t *source)
{
	(void)settings;
	return source;
}

static void input_destroy(void *data)
{
	(void)data;
}

static void test_sources_left_alive_free_their_canvas_ref(void **state)
{
	(void)state;
	long before = bnum_allocs();
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
	};
	obs_register_source(&input);

	/* Left alive on purpose: obs_free_data() destroys both. */
	assert_non_null(obs_source_create(INPUT_ID, "left alive", NULL, NULL));
	assert_non_null(obs_scene_create("left in the main canvas"));

	obs_shutdown();

	/* What stays is by design: obs_free_data() destroys a source the host
	 * left alive without freeing its control block, which the host's
	 * reference still points to. One per source, two here. */
	assert_int_equal(bnum_allocs() - before, 2);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_sources_left_alive_free_their_canvas_ref),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
