#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include <graphics/shader-parser.h>

/* Characterization of the exported shader_sampler_convert (#1716).
 * Link the real libobs function, without a parser or graphics device. Inputs
 * have equally sized states/values arrays and non-NULL strings. Empty border
 * strings, mismatched arrays and NULL pointers are outside the C contract.
 * Numeric cases avoid strtol overflow and out-of-range long-to-int casts.
 * No public layout, header, existing test or behavior is changed.
 */
static struct gs_sampler_info convert(const char **names, const char **values, size_t count)
{
	struct shader_sampler sampler = {0};
	/* Borrowed stack arrays: conversion must neither own nor modify them. */
	sampler.states.array = (char **)names;
	sampler.states.num = sampler.states.capacity = count;
	sampler.values.array = (char **)values;
	sampler.values.num = sampler.values.capacity = count;
	struct gs_sampler_info info;
	memset(&info, 0xa5, sizeof(info));
	shader_sampler_convert(&sampler, &info);
	assert_ptr_equal(sampler.states.array, names);
	assert_ptr_equal(sampler.values.array, values);
	assert_int_equal(sampler.states.num, count);
	assert_int_equal(sampler.values.num, count);
	assert_int_equal(sampler.states.capacity, count);
	assert_int_equal(sampler.values.capacity, count);
	assert_null(sampler.name);
	return info;
}

static void assert_info(struct gs_sampler_info actual, struct gs_sampler_info expected)
{
	assert_int_equal(actual.filter, expected.filter);
	assert_int_equal(actual.address_u, expected.address_u);
	assert_int_equal(actual.address_v, expected.address_v);
	assert_int_equal(actual.address_w, expected.address_w);
	assert_int_equal(actual.max_anisotropy, expected.max_anisotropy);
	assert_int_equal(actual.border_color, expected.border_color);
}

static struct gs_sampler_info defaults(void)
{
	/* Characterized, not endorsed: absent Filter means zero/POINT, while an
	 * explicitly unknown Filter falls back to LINEAR. */
	return (struct gs_sampler_info){.filter = GS_FILTER_POINT,
					.address_u = GS_ADDRESS_CLAMP,
					.address_v = GS_ADDRESS_CLAMP,
					.address_w = GS_ADDRESS_CLAMP,
					.max_anisotropy = 1,
					.border_color = 0};
}

static void empty_and_unknown(void **state)
{
	(void)state;
	assert_info(convert(NULL, NULL, 0), defaults());
	const char *names[] = {"", "Unknown", "Filter ", " AddressU", "MaxAnisotropyX", "BorderColour"};
	const char *values[] = {"Point", "Mirror", "Anisotropy", "Wrap", "12", "#123456"};
	assert_info(convert(names, values, 6), defaults());
}

static void filters(void **state)
{
	(void)state;
	const struct {
		const char *text;
		enum gs_sample_filter expected;
	} cases[] = {
		{"Point", GS_FILTER_POINT},
		{"pOiNt", GS_FILTER_POINT},
		{"Linear", GS_FILTER_LINEAR},
		{"lInEaR", GS_FILTER_LINEAR},
		{"Anisotropy", GS_FILTER_ANISOTROPIC},
		{"ANISOTROPY", GS_FILTER_ANISOTROPIC},
		{"MIN_MAG_MIP_POINT", GS_FILTER_POINT},
		{"MIN_MAG_MIP_LINEAR", GS_FILTER_LINEAR},
		{"MIN_MAG_POINT_MIP_LINEAR", GS_FILTER_MIN_MAG_POINT_MIP_LINEAR},
		{"MIN_POINT_MAG_LINEAR_MIP_POINT", GS_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT},
		{"MIN_POINT_MAG_MIP_LINEAR", GS_FILTER_MIN_POINT_MAG_MIP_LINEAR},
		{"MIN_LINEAR_MAG_MIP_POINT", GS_FILTER_MIN_LINEAR_MAG_MIP_POINT},
		{"MIN_LINEAR_MAG_POINT_MIP_LINEAR", GS_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR},
		{"MIN_MAG_LINEAR_MIP_POINT", GS_FILTER_MIN_MAG_LINEAR_MIP_POINT},
		{"min_mag_mip_point", GS_FILTER_LINEAR},
		{"", GS_FILTER_LINEAR},
		{"unknown", GS_FILTER_LINEAR},
		{"Point ", GS_FILTER_LINEAR},
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		const char *names[] = {"fIlTeR"};
		const char *values[] = {cases[i].text};
		struct gs_sampler_info expected = defaults();
		expected.filter = cases[i].expected;
		assert_info(convert(names, values, 1), expected);
	}
}

static void addresses(void **state)
{
	(void)state;
	const struct {
		const char *text;
		enum gs_address_mode expected;
	} cases[] = {
		{"Wrap", GS_ADDRESS_WRAP},
		{"REPEAT", GS_ADDRESS_WRAP},
		{"Clamp", GS_ADDRESS_CLAMP},
		{"None", GS_ADDRESS_CLAMP},
		{"mIrRoR", GS_ADDRESS_MIRROR},
		{"BORDER", GS_ADDRESS_BORDER},
		{"MirrorOnce", GS_ADDRESS_MIRRORONCE},
		{"", GS_ADDRESS_CLAMP},
		{"unknown", GS_ADDRESS_CLAMP},
		{"Wrap ", GS_ADDRESS_CLAMP},
	};
	const char *keys[] = {"aDdReSsU", "ADDRESSV", "addressw"};
	for (size_t axis = 0; axis < 3; axis++) {
		for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
			const char *names[] = {keys[axis]};
			const char *values[] = {cases[i].text};
			struct gs_sampler_info expected = defaults();
			if (axis == 0)
				expected.address_u = cases[i].expected;
			if (axis == 1)
				expected.address_v = cases[i].expected;
			if (axis == 2)
				expected.address_w = cases[i].expected;
			assert_info(convert(names, values, 1), expected);
		}
	}
}

static void anisotropy(void **state)
{
	(void)state;
	const struct {
		const char *text;
		int expected;
	} cases[] = {
		{"0", 0},
		{"1", 1},
		{"16", 16},
		{"-7", -7},
		{" +12tail", 12},
		{"010", 10},
		{"0x10", 0},
		{"", 0},
		{"junk", 0},
		{"2147483647", INT_MAX},
		{"-2147483648", INT_MIN},
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		const char *names[] = {"mAxAnIsOtRoPy"};
		const char *values[] = {cases[i].text};
		struct gs_sampler_info expected = defaults();
		expected.max_anisotropy = cases[i].expected;
		assert_info(convert(names, values, 1), expected);
	}
}

static void border_colors(void **state)
{
	(void)state;
	const struct {
		const char *text;
		uint32_t expected;
	} cases[] = {
		{"#0", 0},
		{"#123abc", 0x123abc},
		{"#7fffffff", 0x7fffffff},
		{"#aBcDeF", 0xabcdef},
		{"#0x1234", 0x1234},
		{"# +12tail", 0x12},
		{"#-1", UINT32_MAX},
		{"#", 0},
		{"#junk", 0},
		/* Characterized, not endorsed: skip ANY first byte, not just '#'. */
		{"x1234", 0x1234},
		{"1234", 0x234},
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		const char *names[] = {"bOrDeRcOlOr"};
		const char *values[] = {cases[i].text};
		struct gs_sampler_info expected = defaults();
		expected.border_color = cases[i].expected;
		assert_info(convert(names, values, 1), expected);
	}
}

static void combined_and_duplicates(void **state)
{
	(void)state;
	const char *names[] = {"Filter",        "AddressU",    "AddressV", "AddressW", "MaxAnisotropy",
			       "BorderColor",   "FILTER",      "addressu", "addressv", "addressw",
			       "MAXANISOTROPY", "BORDERCOLOR", "ignored"};
	const char *values[] = {"Point",      "Wrap",   "Mirror", "Border", "4",   "#123456", "unknown",
				"MirrorOnce", "Repeat", "None",   "-2",     "#-1", "0"};
	struct gs_sampler_info expected = {.filter = GS_FILTER_LINEAR,
					   .address_u = GS_ADDRESS_MIRRORONCE,
					   .address_v = GS_ADDRESS_WRAP,
					   .address_w = GS_ADDRESS_CLAMP,
					   .max_anisotropy = -2,
					   .border_color = UINT32_MAX};
	assert_info(convert(names, values, 13), expected);
	/* Each call clears a previous result rather than inheriting any fields. */
	struct shader_sampler empty = {0};
	shader_sampler_convert(&empty, &expected);
	assert_info(expected, defaults());
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(empty_and_unknown), cmocka_unit_test(filters),
		cmocka_unit_test(addresses),         cmocka_unit_test(anisotropy),
		cmocka_unit_test(border_colors),     cmocka_unit_test(combined_and_duplicates),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
