/* Pins the current C behavior of the scene group transform.
 *
 * obs-scene.c.h still reads the item. It still converts absolute and
 * relative coordinates, still inverts the group's draw transform when an
 * item joins a group, and still calls update_item_transform. The test
 * passes the matrix and the rotation delta in, so it needs no display
 * and no scene.
 *
 * None of these symbols are exported from libobs. This executable compiles
 * libobs/obs-scene-group-transform.c.h itself and characterizes that C.
 * It cannot see a later Rust swap until the symbols are reachable from
 * the test.
 *
 * Cases marked characterized, not endorsed, are what the C does today,
 * including the odd parts. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <math.h>
#include <stdio.h>

#include <obs-scene-group-transform.h>

static void expect_vec2(const struct vec2 *got, float x, float y, const char *what)
{
	if (got->x == x && got->y == y)
		return;

	fprintf(stderr, "%s: (%g, %g), expected (%g, %g)\n", what, got->x, got->y, x, y);
	fail();
}

static void expect_neg_zero(float got, const char *what)
{
	if (got == 0.0f && signbit(got))
		return;

	fprintf(stderr, "%s: %g (signbit %d), expected -0\n", what, got, signbit(got));
	fail();
}

/* Rows. x is (0, -1), y is (1, 0): (1, 0) lands on (0, -1) before translation. */
static void rotate_90(struct matrix4 *m, float tx, float ty)
{
	matrix4_identity(m);
	vec4_set(&m->x, 0.0f, -1.0f, 0.0f, 0.0f);
	vec4_set(&m->y, 1.0f, 0.0f, 0.0f, 0.0f);
	vec4_set(&m->t, tx, ty, 0.0f, 1.0f);
}

static void identity_keeps_pos_and_scale_and_adds_rot(void **state)
{
	struct matrix4 m;
	struct vec2 pos;
	struct vec2 scale;
	float rot = 10.0f;

	(void)state;

	matrix4_identity(&m);
	vec2_set(&pos, 4.0f, 5.0f);
	vec2_set(&scale, 2.0f, 3.0f);
	obs_scene_group_transform(&m, &pos, &scale, &rot, 90.0f);

	expect_vec2(&pos, 4.0f, 5.0f, "identity position");
	expect_vec2(&scale, 2.0f, 3.0f, "identity scale");
	assert_float_equal(rot, 100.0f, 0.0f);
}

/* Translation moves the point. It is cleared before the scale multiply, so
 * the scale stays. The caller's matrix keeps its translation. */
static void translation_moves_pos_only(void **state)
{
	struct matrix4 m;
	struct vec2 pos;
	struct vec2 scale;
	float rot = 1.0f;

	(void)state;

	matrix4_identity(&m);
	vec4_set(&m.t, 10.0f, 20.0f, 0.0f, 1.0f);
	vec2_set(&pos, 1.0f, 2.0f);
	vec2_set(&scale, 3.0f, 4.0f);
	obs_scene_group_transform(&m, &pos, &scale, &rot, 0.0f);

	expect_vec2(&pos, 11.0f, 22.0f, "translated position");
	expect_vec2(&scale, 3.0f, 4.0f, "translation does not scale");
	assert_float_equal(rot, 1.0f, 0.0f);
	assert_float_equal(m.t.x, 10.0f, 0.0f);
	assert_float_equal(m.t.y, 20.0f, 0.0f);
	assert_float_equal(m.t.w, 1.0f, 0.0f);
}

static void axis_scale_multiplies_pos_and_scale(void **state)
{
	struct matrix4 m;
	struct vec2 pos;
	struct vec2 scale;
	float rot = 0.0f;

	(void)state;

	matrix4_identity(&m);
	vec4_set(&m.x, 2.0f, 0.0f, 0.0f, 0.0f);
	vec4_set(&m.y, 0.0f, 3.0f, 0.0f, 0.0f);
	vec2_set(&pos, 1.0f, 1.0f);
	vec2_set(&scale, 4.0f, 5.0f);
	obs_scene_group_transform(&m, &pos, &scale, &rot, 0.0f);

	expect_vec2(&pos, 2.0f, 3.0f, "scaled position");
	expect_vec2(&scale, 8.0f, 15.0f, "scaled axes");
}

/* The matrix rotation moves the point. Scale is the column length, so a
 * pure rotation leaves it alone. Rotation of the child is only rot_delta. */
static void rotation_moves_pos_and_keeps_scale(void **state)
{
	struct matrix4 m;
	struct vec2 pos;
	struct vec2 scale;
	float rot = 10.0f;

	(void)state;

	rotate_90(&m, 0.0f, 0.0f);
	vec2_set(&pos, 1.0f, 0.0f);
	vec2_set(&scale, 2.0f, 3.0f);
	obs_scene_group_transform(&m, &pos, &scale, &rot, 90.0f);

	expect_vec2(&pos, 0.0f, -1.0f, "rotated position");
	expect_vec2(&scale, 2.0f, 3.0f, "rotation does not scale");
	assert_float_equal(rot, 100.0f, 0.0f);
}

static void negative_scale_keeps_its_sign(void **state)
{
	struct matrix4 m;
	struct vec2 pos;
	struct vec2 scale;
	float rot = 0.0f;

	(void)state;

	matrix4_identity(&m);
	vec2_set(&pos, 1.0f, 1.0f);
	vec2_set(&scale, -2.0f, 3.0f);
	obs_scene_group_transform(&m, &pos, &scale, &rot, 0.0f);
	expect_vec2(&scale, -2.0f, 3.0f, "negative axis stays negative");

	vec4_set(&m.x, 2.0f, 0.0f, 0.0f, 0.0f);
	vec2_set(&scale, -2.0f, 3.0f);
	obs_scene_group_transform(&m, &pos, &scale, &rot, -90.0f);
	expect_vec2(&scale, -4.0f, 3.0f, "negative axis through a 2x scale");
	assert_float_equal(rot, -90.0f, 0.0f);
}

/* A scale that is not greater than zero takes the minus sign. The length of
 * a zero column is positive zero, so the product is negative zero. The same
 * for an input of -0. Characterized, not endorsed. */
static void zero_scale_is_negative_zero(void **state)
{
	struct matrix4 m;
	struct vec2 pos;
	struct vec2 scale;
	float rot = 7.0f;

	(void)state;

	matrix4_identity(&m);
	vec2_set(&pos, 1.0f, 2.0f);
	vec2_set(&scale, 0.0f, 0.0f);
	obs_scene_group_transform(&m, &pos, &scale, &rot, 0.0f);

	expect_vec2(&pos, 1.0f, 2.0f, "zero scale still translates nothing");
	expect_neg_zero(scale.x, "zero x");
	expect_neg_zero(scale.y, "zero y");
	assert_float_equal(rot, 7.0f, 0.0f);

	vec2_set(&scale, -0.0f, 4.0f);
	obs_scene_group_transform(&m, &pos, &scale, &rot, 0.0f);
	expect_neg_zero(scale.x, "negative zero stays negative zero");
	assert_float_equal(scale.y, 4.0f, 0.0f);
}

/* resize_group measures a point through the group's draw transform. (1, 0)
 * through the 90 degree matrix plus (5, 7) is (5, 6). */
static void transform_val_includes_translation(void **state)
{
	struct matrix4 m;
	struct vec2 point;

	(void)state;

	rotate_90(&m, 5.0f, 7.0f);
	vec2_set(&point, 1.0f, 0.0f);
	obs_scene_transform_val(&point, &m);
	expect_vec2(&point, 5.0f, 6.0f, "point through rotation and translation");
	assert_float_equal(m.t.x, 5.0f, 0.0f);
	assert_float_equal(m.t.y, 7.0f, 0.0f);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(identity_keeps_pos_and_scale_and_adds_rot),
		cmocka_unit_test(translation_moves_pos_only),
		cmocka_unit_test(axis_scale_multiplies_pos_and_scale),
		cmocka_unit_test(rotation_moves_pos_and_keeps_scale),
		cmocka_unit_test(negative_scale_keeps_its_sign),
		cmocka_unit_test(zero_scale_is_negative_zero),
		cmocka_unit_test(transform_val_includes_translation),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
