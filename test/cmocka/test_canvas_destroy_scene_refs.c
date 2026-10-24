/* Destroying a canvas that holds the last references to its scenes must not
 * read a scene it just freed (#2049).
 *
 * A SCENE_REF canvas holds a reference to each scene in it, and
 * obs_canvas_destroy() releases them while it walks its source list. When
 * that release was the last reference, the scene is destroyed, and the walk
 * read the next list entry out of it. libobs frees a source on its
 * destruction task thread, so the read raced that free, and lost it now and
 * then in test_context_names (#2049).
 *
 * The free stays on another thread, so no public API makes this race
 * deterministic. The case runs it many times and M_PERTURB makes glibc fill
 * freed memory, so a lost race follows a garbage pointer instead of reading
 * the old bytes. Public API only, so it runs against libobs, the hybrid
 * libobs and libobs-c-reference. obs_startup() needs a display on Linux;
 * without one the case skips. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <malloc.h>
#include <stdio.h>

#include <obs.h>

static void test_destroy_releases_the_last_scene_refs(void **state)
{
	(void)state;
	mallopt(M_PERTURB, 0xa5);
	if (!obs_startup("en-US", NULL, NULL)) {
		fprintf(stderr, "skipped: obs_startup failed (no display?)\n");
		skip();
	}

	for (int round = 0; round < 100; round++) {
		obs_canvas_t *canvas = obs_canvas_create_private("scene refs", NULL, SCENE_REF | EPHEMERAL);
		assert_non_null(canvas);

		/* The canvas's references are the only ones left after these. */
		for (int i = 0; i < 16; i++) {
			char name[16];
			snprintf(name, sizeof(name), "scene %d", i);
			obs_scene_t *scene = obs_canvas_scene_create(canvas, name);
			assert_non_null(scene);
			obs_scene_release(scene);
		}

		obs_canvas_remove(canvas);
		obs_canvas_release(canvas);
	}

	obs_shutdown();
	mallopt(M_PERTURB, 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_destroy_releases_the_last_scene_refs),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
