#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include <util/platform.h>

/*
 * os_dtostr exponent normalization (obs-rust/obs-studio#754). Removing the
 * '+' and leading zeros from a "%.17g" exponent must shorten the string,
 * return its real length, and read back to the same double.
 */

struct case_ {
	double value;
	const char *expected;
};

static const struct case_ cases[] = {
	{1.5e-5, "1.5e-5"},
	{1e-5, "1.0000000000000001e-5"},
	{1e17, "1e17"},
	{1.5e17, "1.5e17"},
	{1e-300, "1e-300"},
	{1e300, "1.0000000000000001e300"},
	{123456789012345678.0, "1.2345678901234568e17"},
	{0.1, "0.10000000000000001"},
	{100.0, "100.0"},
	{1e16, "10000000000000000.0"},
};

static void test_dtostr_exponent(void **state)
{
	UNUSED_PARAMETER(state);

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		char buf[64];
		int len = os_dtostr(cases[i].value, buf, sizeof(buf));

		assert_string_equal(buf, cases[i].expected);
		assert_int_equal(len, (int)strlen(buf));
		assert_true(os_strtod(buf) == cases[i].value);
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_dtostr_exponent),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
