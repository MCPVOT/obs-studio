#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <graphics/bounds.h>
#include <graphics/matrix3.h>
#include <graphics/matrix4.h>
#include <graphics/plane.h>
#include <util/c99defs.h>

/* Characterization of the exported functions in graphics/bounds.c.
 * Odd lane and failure behavior is characterized, not endorsed.
 *
 * bounds_plane_test and bounds_under_plane read an uninitialized w lane
 * (vec3_plane_dist / vec3_dot multiply it). bounds_intersects_obb reads an
 * uninitialized inverse when matrix4_inv rejects the matrix. Those paths
 * are not called here. */

static void set_vec(struct vec3 *v, float x, float y, float z, float w)
{
	vec3_set(v, x, y, z);
	v->w = w;
}

static void set_bounds(struct bounds *b, float x0, float y0, float z0, float w0, float x1, float y1, float z1, float w1)
{
	set_vec(&b->min, x0, y0, z0, w0);
	set_vec(&b->max, x1, y1, z1, w1);
}

static void assert_vec(const struct vec3 *v, float x, float y, float z, float w)
{
	assert_true(v->x == x && v->y == y && v->z == z && v->w == w);
}

static void assert_bounds(const struct bounds *b, float x0, float y0, float z0, float w0, float x1, float y1, float z1,
			  float w1)
{
	assert_vec(&b->min, x0, y0, z0, w0);
	assert_vec(&b->max, x1, y1, z1, w1);
}

/* vec3_add clears w. An in-place move still reads the old corner. */
static void test_bounds_move(void **state)
{
	struct bounds b, dst;
	struct vec3 v;

	UNUSED_PARAMETER(state);
	set_bounds(&b, 1.0f, 2.0f, 3.0f, 7.0f, 4.0f, 5.0f, 6.0f, 8.0f);
	set_vec(&v, 10.0f, -1.0f, 0.5f, 9.0f);
	bounds_move(&dst, &b, &v);
	assert_bounds(&dst, 11.0f, 1.0f, 3.5f, 0.0f, 14.0f, 4.0f, 6.5f, 0.0f);

	set_vec(&v, 10.0f, 0.0f, 0.0f, 9.0f);
	bounds_move(&b, &b, &v);
	assert_bounds(&b, 11.0f, 2.0f, 3.0f, 0.0f, 14.0f, 5.0f, 6.0f, 0.0f);
}

/* vec3_mul keeps w, including a negative scale that inverts the box. */
static void test_bounds_scale_keeps_w(void **state)
{
	struct bounds b, dst;
	struct vec3 v;

	UNUSED_PARAMETER(state);
	set_bounds(&b, 1.0f, 2.0f, 3.0f, 7.0f, 4.0f, 5.0f, 6.0f, 8.0f);
	set_vec(&v, 2.0f, 3.0f, -1.0f, 5.0f);
	bounds_scale(&dst, &b, &v);
	assert_bounds(&dst, 2.0f, 6.0f, -3.0f, 35.0f, 8.0f, 15.0f, -6.0f, 40.0f);
}

/* vec3_min / vec3_max return the second lane unless the comparison holds,
 * so a NaN in either lane yields the second operand. w is then cleared.
 * Characterized, not endorsed. */
static void test_bounds_merge_nan_lane(void **state)
{
	struct bounds a, b, dst;

	UNUSED_PARAMETER(state);
	set_bounds(&a, NAN, 0.0f, 0.0f, 1.0f, NAN, 0.0f, 0.0f, 1.0f);
	set_bounds(&b, 1.0f, NAN, 2.0f, 3.0f, 1.0f, 5.0f, NAN, 3.0f);
	bounds_merge(&dst, &a, &b);
	assert_true(dst.min.x == 1.0f);
	assert_true(isnan(dst.min.y));
	assert_true(dst.min.z == 0.0f);
	assert_true(dst.min.w == 0.0f);
	assert_true(dst.max.x == 1.0f);
	assert_true(dst.max.y == 5.0f);
	assert_true(isnan(dst.max.z));
	assert_true(dst.max.w == 0.0f);
}

static void test_bounds_merge_point(void **state)
{
	struct bounds b, dst;
	struct vec3 p;

	UNUSED_PARAMETER(state);
	set_bounds(&b, 0.0f, 0.0f, 0.0f, 7.0f, 2.0f, 2.0f, 2.0f, 8.0f);
	set_vec(&p, -1.0f, 5.0f, 1.0f, 9.0f);
	bounds_merge_point(&dst, &b, &p);
	assert_bounds(&dst, -1.0f, 0.0f, 0.0f, 0.0f, 2.0f, 5.0f, 2.0f, 0.0f);
}

/* Corners 0..7 follow the comment in bounds.c. Index 8 falls through the
 * `i > 8` check and writes the same xyz as corner 6. w is never written.
 * Characterized, not endorsed. */
static void test_bounds_get_point(void **state)
{
	struct bounds b;
	struct vec3 p;
	const float corners[8][3] = {
		{1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 6.0f}, {1.0f, 5.0f, 3.0f}, {1.0f, 5.0f, 6.0f},
		{4.0f, 2.0f, 3.0f}, {4.0f, 2.0f, 6.0f}, {4.0f, 5.0f, 3.0f}, {4.0f, 5.0f, 6.0f},
	};
	unsigned int i;

	UNUSED_PARAMETER(state);
	set_bounds(&b, 1.0f, 2.0f, 3.0f, 7.0f, 4.0f, 5.0f, 6.0f, 8.0f);
	for (i = 0; i < 8; i++) {
		set_vec(&p, 9.0f, 9.0f, 9.0f, 42.0f);
		bounds_get_point(&p, &b, i);
		assert_vec(&p, corners[i][0], corners[i][1], corners[i][2], 42.0f);
	}

	set_vec(&p, 9.0f, 9.0f, 9.0f, 42.0f);
	bounds_get_point(&p, &b, 8);
	assert_vec(&p, 4.0f, 5.0f, 3.0f, 42.0f);

	set_vec(&p, 9.0f, 9.0f, 9.0f, 42.0f);
	bounds_get_point(&p, &b, 9);
	assert_vec(&p, 9.0f, 9.0f, 9.0f, 42.0f);
}

/* (max - min) * 0.5 + min. sub and add clear w, so the source w lanes drop. */
static void test_bounds_get_center(void **state)
{
	struct bounds b;
	struct vec3 c;

	UNUSED_PARAMETER(state);
	set_bounds(&b, 0.0f, 2.0f, 4.0f, 7.0f, 2.0f, 6.0f, 10.0f, 8.0f);
	set_vec(&c, 9.0f, 9.0f, 9.0f, 9.0f);
	bounds_get_center(&c, &b);
	assert_vec(&c, 1.0f, 4.0f, 7.0f, 0.0f);
}

/* Translation is the fourth row. The grown box copies the transformed
 * first corner, whose w is 0. */
static void test_bounds_transform_translation(void **state)
{
	struct bounds b, dst;
	struct matrix4 m;

	UNUSED_PARAMETER(state);
	set_bounds(&b, 0.0f, 0.0f, 0.0f, 7.0f, 2.0f, 4.0f, 6.0f, 8.0f);
	matrix4_identity(&m);
	vec4_set(&m.t, 10.0f, -1.0f, 0.5f, 1.0f);
	bounds_transform(&dst, &b, &m);
	assert_bounds(&dst, 10.0f, -1.0f, 0.5f, 0.0f, 12.0f, 3.0f, 6.5f, 0.0f);
}

/* Row-vector multiply by a 90-degree Z rotation. The AABB of the rotated
 * corners swaps the x and y sizes and shifts y below zero. */
static void test_bounds_transform_z_rotation(void **state)
{
	struct bounds b, dst;
	struct matrix4 m;

	UNUSED_PARAMETER(state);
	set_bounds(&b, 0.0f, 0.0f, 0.0f, 7.0f, 2.0f, 4.0f, 6.0f, 8.0f);
	matrix4_identity(&m);
	vec4_set(&m.x, 0.0f, -1.0f, 0.0f, 0.0f);
	vec4_set(&m.y, 1.0f, 0.0f, 0.0f, 0.0f);
	bounds_transform(&dst, &b, &m);
	assert_bounds(&dst, 0.0f, -2.0f, 0.0f, 0.0f, 4.0f, 0.0f, 6.0f, 0.0f);
}

/* transform3x4 subtracts m.t (clearing w) before the rotate, so t.w is
 * ignored and a positive translation moves the box the other way. */
static void test_bounds_transform3x4_translation(void **state)
{
	struct bounds b, dst;
	struct matrix3 m;

	UNUSED_PARAMETER(state);
	set_bounds(&b, 0.0f, 0.0f, 0.0f, 7.0f, 2.0f, 4.0f, 6.0f, 8.0f);
	matrix3_identity(&m);
	set_vec(&m.t, 10.0f, 0.0f, 0.0f, 9.0f);
	bounds_transform3x4(&dst, &b, &m);
	assert_bounds(&dst, -10.0f, 0.0f, 0.0f, 0.0f, -8.0f, 4.0f, 6.0f, 0.0f);
}

/* A hit from outside stores the entry t. A hit from inside stores the
 * exit t (t_min is negative, so *t is t_max). */
static void test_bounds_intersection_ray_hit(void **state)
{
	struct bounds b;
	struct vec3 orig, dir;
	float t;

	UNUSED_PARAMETER(state);
	set_bounds(&b, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f, 0.0f);

	set_vec(&orig, -1.0f, 1.0f, 1.0f, 0.0f);
	set_vec(&dir, 1.0f, 0.0f, 0.0f, 0.0f);
	t = 42.0f;
	assert_true(bounds_intersection_ray(&b, &orig, &dir, &t));
	assert_true(t == 1.0f);

	set_vec(&orig, 1.0f, 1.0f, 1.0f, 0.0f);
	t = 42.0f;
	assert_true(bounds_intersection_ray(&b, &orig, &dir, &t));
	assert_true(t == 1.0f);
}

/* A miss, including a direction parallel to a slab the origin is outside,
 * leaves *t unchanged. */
static void test_bounds_intersection_ray_miss_leaves_t(void **state)
{
	struct bounds b;
	struct vec3 orig, dir;
	float t;

	UNUSED_PARAMETER(state);
	set_bounds(&b, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f, 0.0f);

	set_vec(&orig, 3.0f, 1.0f, 1.0f, 0.0f);
	set_vec(&dir, 1.0f, 0.0f, 0.0f, 0.0f);
	t = 42.0f;
	assert_false(bounds_intersection_ray(&b, &orig, &dir, &t));
	assert_true(t == 42.0f);

	set_vec(&dir, 0.0f, 1.0f, 0.0f, 0.0f);
	t = 42.0f;
	assert_false(bounds_intersection_ray(&b, &orig, &dir, &t));
	assert_true(t == 42.0f);
}

/* The ray t is divided by the segment length. A zero-length segment and a
 * miss both leave *t unchanged. */
static void test_bounds_intersection_line(void **state)
{
	struct bounds b;
	struct vec3 p1, p2;
	float t;

	UNUSED_PARAMETER(state);
	set_bounds(&b, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f, 0.0f);

	set_vec(&p1, -1.0f, 1.0f, 1.0f, 4.0f);
	set_vec(&p2, 3.0f, 1.0f, 1.0f, 0.0f);
	t = 42.0f;
	assert_true(bounds_intersection_line(&b, &p1, &p2, &t));
	assert_true(t == 0.25f);

	set_vec(&p2, -1.0f, 1.0f, 1.0f, 0.0f);
	t = 42.0f;
	assert_false(bounds_intersection_line(&b, &p1, &p2, &t));
	assert_true(t == 42.0f);

	set_vec(&p1, 3.0f, 1.0f, 1.0f, 0.0f);
	set_vec(&p2, 5.0f, 1.0f, 1.0f, 0.0f);
	t = 42.0f;
	assert_false(bounds_intersection_line(&b, &p1, &p2, &t));
	assert_true(t == 42.0f);
}

/* Touching faces count (`<= epsilon`), including equality with epsilon. */
static void test_bounds_intersects_epsilon(void **state)
{
	struct bounds a, touch, gap;

	UNUSED_PARAMETER(state);
	set_bounds(&a, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	set_bounds(&touch, 1.0f, 0.0f, 0.0f, 0.0f, 2.0f, 1.0f, 1.0f, 0.0f);
	set_bounds(&gap, 1.5f, 0.0f, 0.0f, 0.0f, 2.5f, 1.0f, 1.0f, 0.0f);

	assert_true(bounds_intersects(&a, &touch, 0.0f));
	assert_false(bounds_intersects(&a, &gap, 0.0f));
	assert_true(bounds_intersects(&a, &gap, 0.5f));
	assert_false(bounds_intersects(&a, &gap, 0.25f));
}

static void test_bounds_intersects_obb_identity(void **state)
{
	struct bounds a, b;
	struct matrix4 m;

	UNUSED_PARAMETER(state);
	set_bounds(&a, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	set_bounds(&b, 0.5f, 0.5f, 0.5f, 0.0f, 1.5f, 1.5f, 1.5f, 0.0f);
	matrix4_identity(&m);
	assert_true(bounds_intersects_obb(&a, &b, &m, 0.0f));

	set_bounds(&b, 3.0f, 0.0f, 0.0f, 0.0f, 4.0f, 1.0f, 1.0f, 0.0f);
	assert_false(bounds_intersects_obb(&a, &b, &m, 0.0f));
}

/* obb3x4 transposes, which is the inverse of a rotation. Identity stays. */
static void test_bounds_intersects_obb3x4_identity(void **state)
{
	struct bounds a, b;
	struct matrix3 m;

	UNUSED_PARAMETER(state);
	set_bounds(&a, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	set_bounds(&b, 0.5f, 0.5f, 0.5f, 0.0f, 1.5f, 1.5f, 1.5f, 0.0f);
	matrix3_identity(&m);
	assert_true(bounds_intersects_obb3x4(&a, &b, &m, 0.0f));

	set_bounds(&b, 3.0f, 0.0f, 0.0f, 0.0f, 4.0f, 1.0f, 1.0f, 0.0f);
	assert_false(bounds_intersects_obb3x4(&a, &b, &m, 0.0f));
}

/* p->dist cancels: the result is dot(center, dir) - half the projected
 * extent. dir.w does not contribute because the center's w is 0 and
 * vec3_abs clears the direction's w. */
static void test_bounds_min_dist(void **state)
{
	struct bounds b;
	struct plane p;

	UNUSED_PARAMETER(state);
	set_bounds(&b, 0.0f, 0.0f, 3.0f, 7.0f, 0.0f, 0.0f, 5.0f, 8.0f);
	plane_setf(&p, 0.0f, 0.0f, 1.0f, 0.0f);
	p.dir.w = 5.0f;
	assert_true(bounds_min_dist(&b, &p) == 3.0f);

	p.dist = 9.0f;
	assert_true(bounds_min_dist(&b, &p) == 3.0f);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_bounds_move),
		cmocka_unit_test(test_bounds_scale_keeps_w),
		cmocka_unit_test(test_bounds_merge_nan_lane),
		cmocka_unit_test(test_bounds_merge_point),
		cmocka_unit_test(test_bounds_get_point),
		cmocka_unit_test(test_bounds_get_center),
		cmocka_unit_test(test_bounds_transform_translation),
		cmocka_unit_test(test_bounds_transform_z_rotation),
		cmocka_unit_test(test_bounds_transform3x4_translation),
		cmocka_unit_test(test_bounds_intersection_ray_hit),
		cmocka_unit_test(test_bounds_intersection_ray_miss_leaves_t),
		cmocka_unit_test(test_bounds_intersection_line),
		cmocka_unit_test(test_bounds_intersects_epsilon),
		cmocka_unit_test(test_bounds_intersects_obb_identity),
		cmocka_unit_test(test_bounds_intersects_obb3x4_identity),
		cmocka_unit_test(test_bounds_min_dist),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
