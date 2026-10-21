/* Pins the current C behavior of the scene coordinate conversion.
 *
 * obs-scene.c.h still reads the canvas and still decides two things before
 * it calls this file: whether to nudge onto a half pixel, and whether a
 * group or nested scene keeps its scale. The test passes the canvas size
 * and those flags in, so it needs no display and no scene.
 *
 * None of these symbols are exported from libobs. This executable compiles
 * libobs/obs-scene-coords.c.h itself and characterizes that C. It cannot
 * see a later Rust swap until the symbols are reachable from the test.
 *
 * Cases marked characterized, not endorsed, are what the C does today,
 * including the odd parts. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>

#include <obs-scene-coords.h>

static void expect_vec2(const struct vec2 *got, float x, float y, const char *what)
{
	if (got->x == x && got->y == y)
		return;

	fprintf(stderr, "%s: (%g, %g), expected (%g, %g)\n", what, got->x, got->y, x, y);
	fail();
}

/* floor(v * 2 + 0.5) / 2. A value already on a half stays. 0.25 and 0.75
 * move up. -0.25 moves to 0, and -0.75 moves to -0.5, so a negative tie does
 * not mirror the positive one. Characterized, not endorsed. */
static void nudge_abs_rounds_to_a_half(void **state)
{
	struct vec2 v;

	(void)state;

	vec2_set(&v, 0.0f, 1.0f);
	obs_scene_nudge_abs(&v, &v);
	expect_vec2(&v, 0.0f, 1.0f, "on a pixel");

	vec2_set(&v, 0.5f, 1.5f);
	obs_scene_nudge_abs(&v, &v);
	expect_vec2(&v, 0.5f, 1.5f, "on a half");

	vec2_set(&v, 0.25f, 0.75f);
	obs_scene_nudge_abs(&v, &v);
	expect_vec2(&v, 0.5f, 1.0f, "positive ties go up");

	vec2_set(&v, -0.25f, -0.75f);
	obs_scene_nudge_abs(&v, &v);
	expect_vec2(&v, 0.0f, -0.5f, "negative ties");

	vec2_set(&v, -0.5f, -1.0f);
	obs_scene_nudge_abs(&v, &v);
	expect_vec2(&v, -0.5f, -1.0f, "negative halves stay");
}

static void nudge_skips_a_group(void **state)
{
	(void)state;

	assert_true(obs_scene_coords_nudge(false, false));
	assert_false(obs_scene_coords_nudge(true, false));
	assert_false(obs_scene_coords_nudge(false, true));
	assert_false(obs_scene_coords_nudge(true, true));
}

static void keep_scale_for_a_group_or_nested_scene(void **state)
{
	(void)state;

	assert_false(obs_scene_coords_keep_scale(false, false));
	assert_true(obs_scene_coords_keep_scale(true, false));
	assert_true(obs_scene_coords_keep_scale(false, true));
	assert_true(obs_scene_coords_keep_scale(true, true));
}

/* 1920x1080. Height is [-1, 1], so the center is (0, 0) and a corner is an
 * axis of 1. These land on whole pixels, so the nudge does not move them. */
static void pos_to_absolute_on_a_1080p_canvas(void **state)
{
	struct vec2 v;

	(void)state;

	vec2_set(&v, 0.0f, 0.0f);
	obs_scene_pos_to_absolute(&v, &v, 1920.0f, 1080.0f, true);
	expect_vec2(&v, 960.0f, 540.0f, "center");

	vec2_set(&v, 1.0f, 1.0f);
	obs_scene_pos_to_absolute(&v, &v, 1920.0f, 1080.0f, true);
	expect_vec2(&v, 1500.0f, 1080.0f, "bottom right of the relative box");

	vec2_set(&v, -1.0f, -1.0f);
	obs_scene_pos_to_absolute(&v, &v, 1920.0f, 1080.0f, true);
	expect_vec2(&v, 420.0f, 0.0f, "top left of the relative box");
}

/* Canvas width 0 and height 6. (0.25, 0) is x = 0.75 before the nudge, and
 * y = 3. The nudge moves 0.75 to 1 and leaves 3. Without the nudge, 0.75
 * stays. Characterized, not endorsed. */
static void pos_to_absolute_nudge_moves_a_quarter(void **state)
{
	struct vec2 v;

	(void)state;

	vec2_set(&v, 0.25f, 0.0f);
	obs_scene_pos_to_absolute(&v, &v, 0.0f, 6.0f, true);
	expect_vec2(&v, 1.0f, 3.0f, "nudged");

	vec2_set(&v, 0.25f, 0.0f);
	obs_scene_pos_to_absolute(&v, &v, 0.0f, 6.0f, false);
	expect_vec2(&v, 0.75f, 3.0f, "group keeps 0.75");
}

/* The way back does not nudge. 0.75 pixels on that same canvas is 0.25
 * again. Running the nudge first does not come back to 0.25. */
static void pos_from_absolute_does_not_nudge(void **state)
{
	struct vec2 v;

	(void)state;

	vec2_set(&v, 0.75f, 3.0f);
	obs_scene_pos_from_absolute(&v, &v, 0.0f, 6.0f);
	expect_vec2(&v, 0.25f, 0.0f, "0.75 pixels");

	vec2_set(&v, 960.0f, 540.0f);
	obs_scene_pos_from_absolute(&v, &v, 1920.0f, 1080.0f);
	expect_vec2(&v, 0.0f, 0.0f, "1080p center");

	vec2_set(&v, 1.0f, 3.0f);
	obs_scene_pos_from_absolute(&v, &v, 0.0f, 6.0f);
	expect_vec2(&v, 1.0f / 3.0f, 0.0f, "after a nudge");
}

/* Relative size 2 is the full canvas height. A negative size is a flip, and
 * the nudge of -0.75 is -0.5. */
static void size_to_absolute_and_back(void **state)
{
	struct vec2 v;

	(void)state;

	vec2_set(&v, 2.0f, 2.0f);
	obs_scene_size_to_absolute(&v, &v, 1080.0f, true);
	expect_vec2(&v, 1080.0f, 1080.0f, "full height");

	vec2_set(&v, 1080.0f, 1080.0f);
	obs_scene_size_from_absolute(&v, &v, 1080.0f);
	expect_vec2(&v, 2.0f, 2.0f, "full height back");

	vec2_set(&v, -0.1875f, 0.5f);
	obs_scene_size_to_absolute(&v, &v, 8.0f, true);
	expect_vec2(&v, -0.5f, 2.0f, "flipped quarter nudged");

	vec2_set(&v, -0.1875f, 0.5f);
	obs_scene_size_to_absolute(&v, &v, 8.0f, false);
	expect_vec2(&v, -0.75f, 2.0f, "flipped quarter kept");

	vec2_set(&v, -0.75f, 2.0f);
	obs_scene_size_from_absolute(&v, &v, 8.0f);
	expect_vec2(&v, -0.1875f, 0.5f, "size back does not nudge");
}

/* Doubling the canvas height doubles the scale, including a flip. Halving
 * it halves the scale. The relative function is that in reverse. */
static void canvas_scale_follows_the_height(void **state)
{
	struct vec2 v;

	(void)state;

	vec2_set(&v, 1.5f, -2.0f);
	obs_scene_item_canvas_scale(&v, &v, 2160.0f, 1080.0f);
	expect_vec2(&v, 3.0f, -4.0f, "canvas grew");

	vec2_set(&v, 3.0f, -4.0f);
	obs_scene_item_relative_scale(&v, &v, 2160.0f, 1080.0f);
	expect_vec2(&v, 1.5f, -2.0f, "back to the reference");

	vec2_set(&v, 2.0f, 4.0f);
	obs_scene_item_canvas_scale(&v, &v, 540.0f, 1080.0f);
	expect_vec2(&v, 1.0f, 2.0f, "canvas shrank");

	vec2_set(&v, 1.0f, -1.0f);
	obs_scene_item_canvas_scale(&v, &v, 1080.0f, 1080.0f);
	expect_vec2(&v, 1.0f, -1.0f, "same canvas");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(nudge_abs_rounds_to_a_half),
		cmocka_unit_test(nudge_skips_a_group),
		cmocka_unit_test(keep_scale_for_a_group_or_nested_scene),
		cmocka_unit_test(pos_to_absolute_on_a_1080p_canvas),
		cmocka_unit_test(pos_to_absolute_nudge_moves_a_quarter),
		cmocka_unit_test(pos_from_absolute_does_not_nudge),
		cmocka_unit_test(size_to_absolute_and_back),
		cmocka_unit_test(canvas_scale_follows_the_height),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
