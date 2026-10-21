/* Pins the current C behavior of obs_bounds_place() and add_alignment().
 *
 * obs_bounds_place() is the rest of calculate_bounds_data() after the scene
 * has turned the item's bounds into pixels. The scene still does that
 * (absolute coordinates, or size_to_absolute) and then calls this. The test
 * passes the pixels in directly, so it needs no display and no scene.
 *
 * Neither symbol is exported from libobs. This executable compiles
 * libobs/obs-scene-bounds-place.c.h and libobs/obs-scene-bounds.c.h itself
 * and characterizes that C. It cannot see a later Rust swap until the
 * symbols are reachable from the test.
 *
 * Cases marked characterized, not endorsed, are what the C does today,
 * including the odd parts. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>

#include <obs-scene-bounds.h>

/* Declared in obs-internal.h. Not exported, so the copy compiled into this
 * test is the one that runs. */
void add_alignment(struct vec2 *v, uint32_t align, int cx, int cy);

struct placed {
	struct vec2 origin;
	struct vec2 scale;
	uint32_t cx;
	uint32_t cy;
	struct obs_sceneitem_crop crop;
};

static struct placed place(enum obs_bounds_type type, float bounds_x, float bounds_y, float origin_x, float origin_y,
			   float scale_x, float scale_y, uint32_t cx, uint32_t cy, uint32_t align, bool crop_to_bounds)
{
	struct placed out;
	struct vec2 bounds;

	vec2_set(&bounds, bounds_x, bounds_y);
	vec2_set(&out.origin, origin_x, origin_y);
	vec2_set(&out.scale, scale_x, scale_y);
	out.cx = cx;
	out.cy = cy;
	/* Sentinel. A crop writes one axis and must leave the other alone. */
	out.crop.left = 7;
	out.crop.top = 7;
	out.crop.right = 7;
	out.crop.bottom = 7;
	obs_bounds_place(type, &bounds, &out.origin, &out.scale, &out.cx, &out.cy, align, crop_to_bounds, &out.crop);
	return out;
}

static void expect_placed(const struct placed *got, float origin_x, float origin_y, float scale_x, float scale_y,
			  uint32_t cx, uint32_t cy, int left, int top, int right, int bottom, const char *what)
{
	if (got->origin.x == origin_x && got->origin.y == origin_y && got->scale.x == scale_x &&
	    got->scale.y == scale_y && got->cx == cx && got->cy == cy && got->crop.left == left &&
	    got->crop.top == top && got->crop.right == right && got->crop.bottom == bottom)
		return;

	fprintf(stderr,
		"%s:\n  origin (%a, %a), expected (%a, %a)\n  scale (%a, %a), expected (%a, %a)\n"
		"  box %u x %u, expected %u x %u\n  crop %d,%d,%d,%d expected %d,%d,%d,%d\n",
		what, got->origin.x, got->origin.y, origin_x, origin_y, got->scale.x, got->scale.y, scale_x, scale_y,
		got->cx, got->cy, cx, cy, got->crop.left, got->crop.top, got->crop.right, got->crop.bottom, left, top,
		right, bottom);
	fail();
}

/* RIGHT is tested before LEFT, BOTTOM before TOP. Center is the else, and
 * cx/2 truncates toward zero. Characterized, not endorsed. */
static void add_alignment_picks_a_side(void **state)
{
	struct vec2 v;

	(void)state;

	vec2_set(&v, 1.0f, 2.0f);
	add_alignment(&v, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, 5, 5);
	assert_float_equal(v.x, 1.0f, 0.0);
	assert_float_equal(v.y, 2.0f, 0.0);

	vec2_set(&v, 1.0f, 2.0f);
	add_alignment(&v, OBS_ALIGN_RIGHT | OBS_ALIGN_BOTTOM, 5, 9);
	assert_float_equal(v.x, 6.0f, 0.0);
	assert_float_equal(v.y, 11.0f, 0.0);

	vec2_set(&v, 0.0f, 0.0f);
	add_alignment(&v, OBS_ALIGN_CENTER, 5, -5);
	assert_float_equal(v.x, 2.0f, 0.0);
	assert_float_equal(v.y, -2.0f, 0.0);

	vec2_set(&v, 0.0f, 0.0f);
	add_alignment(&v, OBS_ALIGN_LEFT | OBS_ALIGN_RIGHT, 8, 5);
	assert_float_equal(v.x, 8.0f, 0.0);
	assert_float_equal(v.y, 2.0f, 0.0);

	vec2_set(&v, 0.0f, 0.0f);
	add_alignment(&v, OBS_ALIGN_TOP | OBS_ALIGN_BOTTOM, 5, 8);
	assert_float_equal(v.x, 2.0f, 0.0);
	assert_float_equal(v.y, 8.0f, 0.0);
}

/* No bounds type, so the scale stays. The leftover on each axis is centered
 * with (int)-diff. 200x100 drawn into a 100x100 box leaves 100px of width
 * and nothing of height, and the center of 100 is 50. */
static void none_centers_the_leftover(void **state)
{
	struct placed p;

	(void)state;
	p = place(OBS_BOUNDS_NONE, 100.0f, 100.0f, 0.0f, 0.0f, 1.0f, 1.0f, 200, 100, OBS_ALIGN_CENTER, false);
	expect_placed(&p, 50.0f, 0.0f, 1.0f, 1.0f, 100, 100, 7, 7, 7, 7, "none center");
}

/* The crop flag is set and both axes overflow, but OBS_BOUNDS_NONE is not a
 * cropping type, so the sentinel crop is left alone and the alignment stays.
 * A cropping type decides the axis with `crop_width = (double)width_diff < -0.1`,
 * so a width overflow wins and the height crop is not written. A fit that
 * lands on one axis does not leave both diffs below -0.1 for ordinary
 * positive boxes; this test does not feed a negative box to the uint32_t
 * cast. Characterized, not endorsed. */
static void none_ignores_the_crop_flag_when_both_axes_overflow(void **state)
{
	struct placed p;

	(void)state;
	p = place(OBS_BOUNDS_NONE, 50.0f, 40.0f, 0.0f, 0.0f, 1.0f, 1.0f, 200, 100, OBS_ALIGN_CENTER, true);
	expect_placed(&p, 75.0f, 30.0f, 1.0f, 1.0f, 50, 40, 7, 7, 7, 7, "none crop flag");
}

static void stretch_matches_the_box(void **state)
{
	struct placed p;

	(void)state;
	p = place(OBS_BOUNDS_STRETCH, 100.0f, 100.0f, 0.0f, 0.0f, 1.0f, 1.0f, 200, 100, OBS_ALIGN_CENTER, false);
	expect_placed(&p, 0.0f, 0.0f, 0.5f, 1.0f, 100, 100, 7, 7, 7, 7, "stretch");
}

static void scale_inner_top_left_and_center(void **state)
{
	struct placed top_left, center;

	(void)state;
	top_left = place(OBS_BOUNDS_SCALE_INNER, 100.0f, 80.0f, 0.0f, 0.0f, 1.0f, 1.0f, 200, 100,
			 OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false);
	expect_placed(&top_left, 0.0f, 0.0f, 0.5f, 0.5f, 100, 80, 7, 7, 7, 7, "inner top left");

	/* 30px of height leftover. Center uses (int)-30 / 2, which is -15. */
	center =
		place(OBS_BOUNDS_SCALE_INNER, 100.0f, 80.0f, 0.0f, 0.0f, 1.0f, 1.0f, 200, 100, OBS_ALIGN_CENTER, false);
	expect_placed(&center, 0.0f, -15.0f, 0.5f, 0.5f, 100, 80, 7, 7, 7, 7, "inner center");
}

/* 80/100 in float32. The crop is roundf of each half of the overdraw. A
 * non-flipped crop then sets that axis of the origin to 0, so the alignment
 * on x is discarded. The y origin, and the height crop, stay. Characterized,
 * not endorsed. */
static void scale_outer_crop_drops_that_axis_of_alignment(void **state)
{
	struct placed p;
	const float scale = 0.8f;

	(void)state;
	p = place(OBS_BOUNDS_SCALE_OUTER, 50.0f, 80.0f, 9.0f, 3.0f, 1.0f, 1.0f, 200, 100, OBS_ALIGN_CENTER, true);
	expect_placed(&p, 0.0f, 3.0f, scale, scale, 50, 80, 69, 7, 69, 7, "outer crop");
}

/* Same fit with the crop flag off. Alignment is kept: 55px on x. */
static void scale_outer_without_crop_keeps_alignment(void **state)
{
	struct placed p;
	const float scale = 0.8f;

	(void)state;
	p = place(OBS_BOUNDS_SCALE_OUTER, 50.0f, 80.0f, 0.0f, 0.0f, 1.0f, 1.0f, 200, 100, OBS_ALIGN_CENTER, false);
	expect_placed(&p, 55.0f, 0.0f, scale, scale, 50, 80, 7, 7, 7, 7, "outer no crop");
}

/* Drawn width is negative. After alignment, the origin shifts by that width
 * so the flipped item stays in the box. */
static void flipped_item_stays_in_the_box(void **state)
{
	struct placed p;

	(void)state;
	p = place(OBS_BOUNDS_NONE, 10.0f, 10.0f, 0.0f, 0.0f, -1.0f, 1.0f, 10, 10, OBS_ALIGN_CENTER, false);
	expect_placed(&p, -10.0f, 0.0f, -1.0f, 1.0f, 10, 10, 7, 7, 7, 7, "flip none");
}

/* (int) of the negated leftover truncates toward zero, and the box stored
 * back is (uint32_t) of the bounds, which also truncates. 10.9 -> 10, and
 * the center of (int)-6.9 is -3. */
static void fractional_box_truncates_toward_zero(void **state)
{
	struct placed p;

	(void)state;
	p = place(OBS_BOUNDS_NONE, 10.9f, 4.0f, 0.0f, 0.0f, 1.0f, 1.0f, 4, 4, OBS_ALIGN_CENTER, false);
	expect_placed(&p, -3.0f, 0.0f, 1.0f, 1.0f, 10, 4, 7, 7, 7, 7, "fractional");
}

/* cy is 0, so the outer fit leaves the scale alone. Overdraw on the left
 * alignment is 0.5 on the right side, and roundf(0.5) is 1 (half away from
 * zero). Characterized, not endorsed. */
static void round_half_away_from_zero(void **state)
{
	struct placed p;

	(void)state;
	p = place(OBS_BOUNDS_SCALE_OUTER, 1.5f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f, 2, 0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP,
		  true);
	expect_placed(&p, 0.0f, 0.0f, 1.0f, 1.0f, 1, 10, 0, 7, 1, 7, "round half");
}

/* The comparison is strict: -0.0625 does not crop, -0.25 does. -0.25 still
 * rounds to a 0 crop. The branch shows up because it clears the sentinel and
 * zeroes that axis of the origin. Characterized, not endorsed. */
static void crop_threshold_is_strict(void **state)
{
	struct placed above, below;

	(void)state;
	above = place(OBS_BOUNDS_SCALE_TO_WIDTH, 16.0f, 15.9375f, 3.0f, 4.0f, 1.0f, 1.0f, 16, 16,
		      OBS_ALIGN_LEFT | OBS_ALIGN_TOP, true);
	expect_placed(&above, 3.0f, 4.0f, 1.0f, 1.0f, 16, 15, 7, 7, 7, 7, "above -0.1");

	below = place(OBS_BOUNDS_SCALE_TO_WIDTH, 16.0f, 15.75f, 3.0f, 4.0f, 1.0f, 1.0f, 16, 16,
		      OBS_ALIGN_LEFT | OBS_ALIGN_TOP, true);
	expect_placed(&below, 3.0f, 0.0f, 1.0f, 1.0f, 16, 15, 7, 0, 7, 0, "below -0.1");
}

/* Height is 0, so the outer fit does not change the scale. A flipped center
 * doubles the alignment origin and swaps the two crop sides (here they are
 * equal, 8 and 8). Left subtracts the diff from the origin before the swap.
 * Right does neither of those, then the flip shift runs. Characterized, not
 * endorsed. */
static void flipped_crop_center_left_and_right(void **state)
{
	struct placed center, left, right;

	(void)state;
	center = place(OBS_BOUNDS_SCALE_OUTER, 10.0f, 10.0f, 0.0f, 0.0f, -2.0f, 1.0f, 20, 0, OBS_ALIGN_CENTER, true);
	expect_placed(&center, -10.0f, -5.0f, -2.0f, 1.0f, 10, 10, 8, 7, 8, 7, "flipped center");

	left = place(OBS_BOUNDS_SCALE_OUTER, 10.0f, 10.0f, 0.0f, 0.0f, -1.0f, 1.0f, 20, 0,
		     OBS_ALIGN_LEFT | OBS_ALIGN_TOP, true);
	expect_placed(&left, -10.0f, 0.0f, -1.0f, 1.0f, 10, 10, 10, 7, 0, 7, "flipped left");

	right = place(OBS_BOUNDS_SCALE_OUTER, 10.0f, 10.0f, 0.0f, 0.0f, -1.0f, 1.0f, 20, 0,
		      OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, true);
	expect_placed(&right, -10.0f, 0.0f, -1.0f, 1.0f, 10, 10, 0, 7, 10, 7, "flipped right");
}

/* An empty source keeps its scale (obsproject/obs-studio#4245). Alignment
 * still runs against the box, so a 100x100 box centers to -50, -50. */
static void empty_source_keeps_its_scale(void **state)
{
	struct placed p;

	(void)state;
	p = place(OBS_BOUNDS_STRETCH, 100.0f, 100.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0, 0, OBS_ALIGN_CENTER, false);
	expect_placed(&p, -50.0f, -50.0f, 1.0f, 1.0f, 100, 100, 7, 7, 7, 7, "empty stretch");
}

/* Past the box, max-only scales like inner. It is not a cropping type, so
 * the crop flag does nothing. */
static void max_only_shrinks_without_cropping(void **state)
{
	struct placed p;

	(void)state;
	p = place(OBS_BOUNDS_MAX_ONLY, 100.0f, 100.0f, 0.0f, 0.0f, 1.0f, 1.0f, 200, 100, OBS_ALIGN_CENTER, true);
	expect_placed(&p, 0.0f, -25.0f, 0.5f, 0.5f, 100, 100, 7, 7, 7, 7, "max only");
}

/* Scale-to-height can overflow the width, and that is the axis that gets
 * cropped. 40/100 in float32, then roundf of each half of the overdraw. */
static void scale_to_height_crops_the_width(void **state)
{
	struct placed p;
	const float scale = 0.4f;

	(void)state;
	p = place(OBS_BOUNDS_SCALE_TO_HEIGHT, 50.0f, 40.0f, 0.0f, 0.0f, 1.0f, 1.0f, 200, 100, OBS_ALIGN_CENTER, true);
	expect_placed(&p, 0.0f, 0.0f, scale, scale, 50, 40, 38, 7, 38, 7, "to height");
}

/* Scale-to-width with a dyadic fit. The height overflows, the crop is 30 and
 * 30, and the non-flipped crop zeroes the y origin. */
static void scale_to_width_crops_the_height(void **state)
{
	struct placed p;

	(void)state;
	p = place(OBS_BOUNDS_SCALE_TO_WIDTH, 50.0f, 20.0f, 1.0f, 2.0f, 1.0f, 1.0f, 100, 100, OBS_ALIGN_CENTER, true);
	expect_placed(&p, 1.0f, 0.0f, 0.5f, 0.5f, 50, 20, 7, 30, 7, 30, "to width");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(add_alignment_picks_a_side),
		cmocka_unit_test(none_centers_the_leftover),
		cmocka_unit_test(none_ignores_the_crop_flag_when_both_axes_overflow),
		cmocka_unit_test(stretch_matches_the_box),
		cmocka_unit_test(scale_inner_top_left_and_center),
		cmocka_unit_test(scale_outer_crop_drops_that_axis_of_alignment),
		cmocka_unit_test(scale_outer_without_crop_keeps_alignment),
		cmocka_unit_test(flipped_item_stays_in_the_box),
		cmocka_unit_test(fractional_box_truncates_toward_zero),
		cmocka_unit_test(round_half_away_from_zero),
		cmocka_unit_test(crop_threshold_is_strict),
		cmocka_unit_test(flipped_crop_center_left_and_right),
		cmocka_unit_test(empty_source_keeps_its_scale),
		cmocka_unit_test(max_only_shrinks_without_cropping),
		cmocka_unit_test(scale_to_height_crops_the_width),
		cmocka_unit_test(scale_to_width_crops_the_height),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
