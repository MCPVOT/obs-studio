#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <util/c99defs.h>
#include <graphics/axisang.h>
#include <graphics/matrix3.h>
#include <graphics/matrix4.h>
#include <graphics/quat.h>

/* Characterization of the exported functions in graphics/matrix4.c.
 * Odd failure and lane behavior is characterized, not endorsed.
 * matrix4_transpose moves lanes with SSE unpacks. Those values are exact.
 * The post-multiply helpers and the *_i helpers disagree on translation;
 * both results below are what the C code produces. */

static void assert_row(const struct vec4 *v, float x, float y, float z, float w)
{
	assert_true(v->x == x && v->y == y && v->z == z && v->w == w);
}

static void assert_identity(const struct matrix4 *m)
{
	assert_row(&m->x, 1.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&m->y, 0.0f, 1.0f, 0.0f, 0.0f);
	assert_row(&m->z, 0.0f, 0.0f, 1.0f, 0.0f);
	assert_row(&m->t, 0.0f, 0.0f, 0.0f, 1.0f);
}

static void set_row3(struct vec3 *v, float x, float y, float z, float w)
{
	vec3_set(v, x, y, z);
	v->w = w;
}

static void x180(struct matrix4 *m)
{
	struct quat q;

	quat_set(&q, 1.0f, 0.0f, 0.0f, 0.0f);
	matrix4_from_quat(m, &q);
}

static void translation(struct matrix4 *m, float x, float y, float z)
{
	matrix4_identity(m);
	vec4_set(&m->t, x, y, z, 1.0f);
}

static void fill_matrix(struct matrix4 *m, float v)
{
	float *p = (float *)m;
	int i;

	for (i = 0; i < 16; i++)
		p[i] = v;
}

/* A zero quaternion has norm 0, so the scale is 0. Axes are identity with
 * w = 0, and the translation lane is 1. */
static void test_matrix4_from_quat_zero_is_identity(void **state)
{
	struct quat q;
	struct matrix4 m;

	UNUSED_PARAMETER(state);
	quat_set(&q, 0.0f, 0.0f, 0.0f, 0.0f);
	matrix4_from_quat(&m, &q);
	assert_identity(&m);
}

static void test_matrix4_from_quat_identity(void **state)
{
	struct quat q;
	struct matrix4 m;

	UNUSED_PARAMETER(state);
	quat_identity(&q);
	matrix4_from_quat(&m, &q);
	assert_identity(&m);

	/* A non-unit scalar quaternion normalizes to the same matrix. */
	quat_set(&q, 0.0f, 0.0f, 0.0f, 2.0f);
	matrix4_from_quat(&m, &q);
	assert_identity(&m);
}

/* 180 degrees about x: quat (1, 0, 0, 0). */
static void test_matrix4_from_quat_x180(void **state)
{
	struct matrix4 m;

	UNUSED_PARAMETER(state);
	x180(&m);
	assert_row(&m.x, 1.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&m.y, 0.0f, -1.0f, 0.0f, 0.0f);
	assert_row(&m.z, 0.0f, 0.0f, -1.0f, 0.0f);
	assert_row(&m.t, 0.0f, 0.0f, 0.0f, 1.0f);
}

/* Angle 0 yields the identity quaternion, then the identity matrix. */
static void test_matrix4_from_axisang_zero(void **state)
{
	struct axisang aa;
	struct matrix4 m;

	UNUSED_PARAMETER(state);
	axisang_set(&aa, 0.0f, 0.0f, 1.0f, 0.0f);
	matrix4_from_axisang(&m, &aa);
	assert_identity(&m);
}

/* Copies every lane, then forces t.w to 1. Axis w lanes survive. */
static void test_matrix4_from_matrix3_sets_tw(void **state)
{
	struct matrix3 src;
	struct matrix4 m;

	UNUSED_PARAMETER(state);
	set_row3(&src.x, 1.0f, 2.0f, 3.0f, 4.0f);
	set_row3(&src.y, 5.0f, 6.0f, 7.0f, 8.0f);
	set_row3(&src.z, 9.0f, 10.0f, 11.0f, 12.0f);
	set_row3(&src.t, 13.0f, 14.0f, 15.0f, 16.0f);
	matrix4_from_matrix3(&m, &src);
	assert_row(&m.x, 1.0f, 2.0f, 3.0f, 4.0f);
	assert_row(&m.y, 5.0f, 6.0f, 7.0f, 8.0f);
	assert_row(&m.z, 9.0f, 10.0f, 11.0f, 12.0f);
	assert_row(&m.t, 13.0f, 14.0f, 15.0f, 1.0f);
}

static void test_matrix4_mul_translation_scale_and_alias(void **state)
{
	struct matrix4 m1, m2, dst, aliased;

	UNUSED_PARAMETER(state);
	translation(&m1, 1.0f, 0.0f, 0.0f);
	matrix4_identity(&m2);
	vec4_set(&m2.x, 2.0f, 0.0f, 0.0f, 0.0f);
	vec4_set(&m2.y, 0.0f, 3.0f, 0.0f, 0.0f);
	vec4_set(&m2.z, 0.0f, 0.0f, 4.0f, 0.0f);

	matrix4_mul(&dst, &m1, &m2);
	assert_row(&dst.x, 2.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&dst.y, 0.0f, 3.0f, 0.0f, 0.0f);
	assert_row(&dst.z, 0.0f, 0.0f, 4.0f, 0.0f);
	assert_row(&dst.t, 2.0f, 0.0f, 0.0f, 1.0f);

	/* dst == m2 is computed through a temporary, so m2 is not half-updated. */
	aliased = m2;
	matrix4_mul(&aliased, &m1, &aliased);
	assert_row(&aliased.x, 2.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&aliased.y, 0.0f, 3.0f, 0.0f, 0.0f);
	assert_row(&aliased.z, 0.0f, 0.0f, 4.0f, 0.0f);
	assert_row(&aliased.t, 2.0f, 0.0f, 0.0f, 1.0f);
}

static void test_matrix4_determinant(void **state)
{
	struct matrix4 m;

	UNUSED_PARAMETER(state);
	matrix4_identity(&m);
	assert_true(matrix4_determinant(&m) == 1.0f);

	/* diag(2, 4, 8, 1) = 64, all powers of two. */
	vec4_set(&m.x, 2.0f, 0.0f, 0.0f, 0.0f);
	vec4_set(&m.y, 0.0f, 4.0f, 0.0f, 0.0f);
	vec4_set(&m.z, 0.0f, 0.0f, 8.0f, 0.0f);
	assert_true(matrix4_determinant(&m) == 64.0f);

	/* Shear in x: still determinant 1. */
	matrix4_identity(&m);
	m.x.y = 2.0f;
	assert_true(matrix4_determinant(&m) == 1.0f);

	fill_matrix(&m, 0.0f);
	assert_true(matrix4_determinant(&m) == 0.0f);

	/* 0.125^3 = 1/512, above the 0.0005 inverse cutoff.
	 * 0.0625^3 = 1/4096, below it. */
	matrix4_identity(&m);
	m.x.x = m.y.y = m.z.z = 0.125f;
	assert_true(matrix4_determinant(&m) == 0.001953125f);
	m.x.x = m.y.y = m.z.z = 0.0625f;
	assert_true(matrix4_determinant(&m) == 0.000244140625f);
}

static void test_matrix4_translate_post_and_pre(void **state)
{
	struct matrix4 m, post, pre;
	struct vec3 v3;
	struct vec4 v4;

	UNUSED_PARAMETER(state);
	x180(&m);
	vec3_set(&v3, 0.0f, 1.0f, 0.0f);
	matrix4_translate3v(&post, &m, &v3);
	assert_row(&post.x, 1.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&post.y, 0.0f, -1.0f, 0.0f, 0.0f);
	assert_row(&post.z, 0.0f, 0.0f, -1.0f, 0.0f);
	assert_row(&post.t, 0.0f, 1.0f, 0.0f, 1.0f);

	/* Pre-multiply transforms the vector by m and keeps m's axes. */
	matrix4_translate3v_i(&pre, &v3, &m);
	assert_row(&pre.x, 1.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&pre.y, 0.0f, -1.0f, 0.0f, 0.0f);
	assert_row(&pre.z, 0.0f, 0.0f, -1.0f, 0.0f);
	assert_row(&pre.t, 0.0f, -1.0f, 0.0f, 1.0f);

	/* vec3.w is ignored. vec4.w is stored as the translation w. */
	matrix4_identity(&m);
	v3.w = 9.0f;
	matrix4_translate3v(&post, &m, &v3);
	assert_row(&post.t, 0.0f, 1.0f, 0.0f, 1.0f);

	vec4_set(&v4, 4.0f, 5.0f, 6.0f, 7.0f);
	matrix4_translate4v(&post, &m, &v4);
	assert_identity(&m);
	assert_row(&post.x, 1.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&post.t, 4.0f, 5.0f, 6.0f, 7.0f);

	x180(&m);
	matrix4_translate4v_i(&pre, &v4, &m);
	assert_row(&pre.y, 0.0f, -1.0f, 0.0f, 0.0f);
	assert_row(&pre.t, 4.0f, -5.0f, -6.0f, 7.0f);
}

static void test_matrix4_rotate_post_and_pre(void **state)
{
	struct matrix4 m, dst;
	struct quat q;
	struct axisang aa;

	UNUSED_PARAMETER(state);
	translation(&m, 1.0f, 2.0f, 3.0f);
	quat_set(&q, 1.0f, 0.0f, 0.0f, 0.0f);
	matrix4_rotate(&dst, &m, &q);
	assert_row(&dst.x, 1.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&dst.y, 0.0f, -1.0f, 0.0f, 0.0f);
	assert_row(&dst.z, 0.0f, 0.0f, -1.0f, 0.0f);
	assert_row(&dst.t, 1.0f, -2.0f, -3.0f, 1.0f);

	/* rotate_i multiplies on the left through matrix4_mul_4x3_only, which
	 * copies the right-hand translation row unchanged. */
	matrix4_rotate_i(&dst, &q, &m);
	assert_row(&dst.x, 1.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&dst.y, 0.0f, -1.0f, 0.0f, 0.0f);
	assert_row(&dst.z, 0.0f, 0.0f, -1.0f, 0.0f);
	assert_row(&dst.t, 1.0f, 2.0f, 3.0f, 1.0f);

	axisang_set(&aa, 0.0f, 1.0f, 0.0f, 0.0f);
	matrix4_rotate_aa(&dst, &m, &aa);
	assert_row(&dst.x, 1.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&dst.y, 0.0f, 1.0f, 0.0f, 0.0f);
	assert_row(&dst.z, 0.0f, 0.0f, 1.0f, 0.0f);
	assert_row(&dst.t, 1.0f, 2.0f, 3.0f, 1.0f);

	matrix4_rotate_aa_i(&dst, &aa, &m);
	assert_row(&dst.t, 1.0f, 2.0f, 3.0f, 1.0f);
	assert_row(&dst.x, 1.0f, 0.0f, 0.0f, 0.0f);
}

static void test_matrix4_scale_post_and_pre(void **state)
{
	struct matrix4 m, dst;
	struct vec3 v;

	UNUSED_PARAMETER(state);
	translation(&m, 1.0f, 2.0f, 3.0f);
	vec3_set(&v, 2.0f, 3.0f, 4.0f);
	/* vec3.w is not read. Characterized, not endorsed. */
	v.w = 5.0f;

	matrix4_scale(&dst, &m, &v);
	assert_row(&dst.x, 2.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&dst.y, 0.0f, 3.0f, 0.0f, 0.0f);
	assert_row(&dst.z, 0.0f, 0.0f, 4.0f, 0.0f);
	assert_row(&dst.t, 2.0f, 6.0f, 12.0f, 1.0f);

	/* scale_i keeps the right-hand translation row. */
	matrix4_scale_i(&dst, &v, &m);
	assert_row(&dst.x, 2.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&dst.y, 0.0f, 3.0f, 0.0f, 0.0f);
	assert_row(&dst.z, 0.0f, 0.0f, 4.0f, 0.0f);
	assert_row(&dst.t, 1.0f, 2.0f, 3.0f, 1.0f);
}

static void test_matrix4_inv_scale_translation_and_singular(void **state)
{
	struct matrix4 m, dst;
	const float *p;
	int i;

	UNUSED_PARAMETER(state);
	matrix4_identity(&m);
	m.x.x = 2.0f;
	m.y.y = 4.0f;
	m.z.z = 8.0f;
	assert_true(matrix4_inv(&dst, &m));
	assert_row(&dst.x, 0.5f, 0.0f, 0.0f, 0.0f);
	assert_row(&dst.y, 0.0f, 0.25f, 0.0f, 0.0f);
	assert_row(&dst.z, 0.0f, 0.0f, 0.125f, 0.0f);
	assert_row(&dst.t, 0.0f, 0.0f, 0.0f, 1.0f);

	translation(&m, 4.0f, -2.0f, 8.0f);
	assert_true(matrix4_inv(&dst, &m));
	assert_row(&dst.x, 1.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&dst.y, 0.0f, 1.0f, 0.0f, 0.0f);
	assert_row(&dst.z, 0.0f, 0.0f, 1.0f, 0.0f);
	assert_row(&dst.t, -4.0f, 2.0f, -8.0f, 1.0f);

	/* 0.125^3 is above the 0.0005 cutoff. */
	matrix4_identity(&m);
	m.x.x = m.y.y = m.z.z = 0.125f;
	assert_true(matrix4_inv(&dst, &m));
	assert_row(&dst.x, 8.0f, 0.0f, 0.0f, 0.0f);
	assert_row(&dst.y, 0.0f, 8.0f, 0.0f, 0.0f);
	assert_row(&dst.z, 0.0f, 0.0f, 8.0f, 0.0f);
	assert_row(&dst.t, 0.0f, 0.0f, 0.0f, 1.0f);

	/* Below the cutoff, matrix4_inv writes nothing. The previous dst stays,
	 * including t.w. Characterized, not endorsed. */
	matrix4_identity(&m);
	m.x.x = m.y.y = m.z.z = 0.0625f;
	fill_matrix(&dst, 9.0f);
	assert_false(matrix4_inv(&dst, &m));
	p = (const float *)&dst;
	for (i = 0; i < 16; i++)
		assert_true(p[i] == 9.0f);

	/* Aliased singular input stays put. Aliased invertible input is inverted. */
	fill_matrix(&m, 0.0f);
	assert_false(matrix4_inv(&m, &m));
	p = (const float *)&m;
	for (i = 0; i < 16; i++)
		assert_true(p[i] == 0.0f);

	matrix4_identity(&m);
	m.x.x = 2.0f;
	assert_true(matrix4_inv(&m, &m));
	assert_row(&m.x, 0.5f, 0.0f, 0.0f, 0.0f);
	assert_row(&m.y, 0.0f, 1.0f, 0.0f, 0.0f);
	assert_row(&m.z, 0.0f, 0.0f, 1.0f, 0.0f);
	assert_row(&m.t, 0.0f, 0.0f, 0.0f, 1.0f);
}

static void test_matrix4_transpose(void **state)
{
	struct matrix4 m, dst;

	UNUSED_PARAMETER(state);
	vec4_set(&m.x, 1.0f, 2.0f, 3.0f, 4.0f);
	vec4_set(&m.y, 5.0f, 6.0f, 7.0f, 8.0f);
	vec4_set(&m.z, 9.0f, 10.0f, 11.0f, 12.0f);
	vec4_set(&m.t, 13.0f, 14.0f, 15.0f, 16.0f);
	matrix4_transpose(&dst, &m);
	assert_row(&dst.x, 1.0f, 5.0f, 9.0f, 13.0f);
	assert_row(&dst.y, 2.0f, 6.0f, 10.0f, 14.0f);
	assert_row(&dst.z, 3.0f, 7.0f, 11.0f, 15.0f);
	assert_row(&dst.t, 4.0f, 8.0f, 12.0f, 16.0f);

	matrix4_transpose(&m, &m);
	assert_row(&m.x, 1.0f, 5.0f, 9.0f, 13.0f);
	assert_row(&m.y, 2.0f, 6.0f, 10.0f, 14.0f);
	assert_row(&m.z, 3.0f, 7.0f, 11.0f, 15.0f);
	assert_row(&m.t, 4.0f, 8.0f, 12.0f, 16.0f);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_matrix4_from_quat_zero_is_identity),
		cmocka_unit_test(test_matrix4_from_quat_identity),
		cmocka_unit_test(test_matrix4_from_quat_x180),
		cmocka_unit_test(test_matrix4_from_axisang_zero),
		cmocka_unit_test(test_matrix4_from_matrix3_sets_tw),
		cmocka_unit_test(test_matrix4_mul_translation_scale_and_alias),
		cmocka_unit_test(test_matrix4_determinant),
		cmocka_unit_test(test_matrix4_translate_post_and_pre),
		cmocka_unit_test(test_matrix4_rotate_post_and_pre),
		cmocka_unit_test(test_matrix4_scale_post_and_pre),
		cmocka_unit_test(test_matrix4_inv_scale_translation_and_singular),
		cmocka_unit_test(test_matrix4_transpose),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
