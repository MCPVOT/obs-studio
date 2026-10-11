#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <wchar.h>
#include <string.h>

#include <util/c99defs.h>
#include <util/utf8.h>

/* The Windows branch of libobs/util/utf8.c: MultiByteToWideChar and
 * WideCharToMultiByte with CP_UTF8 and no flags. Unlike the other branch,
 * invalid input is replaced with U+FFFD instead of failing, the flags are
 * ignored, and a leading UTF-8 BOM is always dropped. */

#define SENTINEL 0x5a5a

static void test_round_trip(void **state)
{
	UNUSED_PARAMETER(state);

	wchar_t wide[8];
	char narrow[16];

	/* "aé€😀": 1 + 2 + 3 + 4 bytes; the emoji is a surrogate pair */
	const char *utf8 = "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80";
	assert_int_equal(utf8_to_wchar(utf8, 0, NULL, 0, 0), 5);
	assert_int_equal(utf8_to_wchar(utf8, 0, wide, 8, 0), 5);
	assert_int_equal(wide[0], L'a');
	assert_int_equal(wide[1], 0xe9);
	assert_int_equal(wide[2], 0x20ac);
	assert_int_equal(wide[3], 0xd83d);
	assert_int_equal(wide[4], 0xde00);

	assert_int_equal(wchar_to_utf8(wide, 5, NULL, 0, 0), 10);
	assert_int_equal(wchar_to_utf8(wide, 5, narrow, sizeof(narrow), 0), 10);
	assert_memory_equal(narrow, utf8, 10);
}

/* No terminator is written, and insize 0 means "up to the NUL". */
static void test_lengths_and_terminators(void **state)
{
	UNUSED_PARAMETER(state);

	wchar_t wide[8];
	wmemset(wide, SENTINEL, 8);
	assert_int_equal(utf8_to_wchar("ab", 0, wide, 8, 0), 2);
	assert_int_equal(wide[2], SENTINEL);

	/* a NUL inside insize is converted like any other character */
	assert_int_equal(utf8_to_wchar("a\0b", 3, wide, 8, 0), 3);
	assert_int_equal(wide[1], 0);
	assert_int_equal(wide[2], L'b');
	assert_int_equal(utf8_to_wchar("a\0b", 0, NULL, 0, 0), 1);

	assert_int_equal(wchar_to_utf8(L"x\0y", 0, NULL, 0, 0), 1);
	assert_int_equal(wchar_to_utf8(L"x\0y", 3, NULL, 0, 0), 3);
}

static void test_bom_dropped_and_flags_ignored(void **state)
{
	UNUSED_PARAMETER(state);

	wchar_t wide[8];

	/* dropped with or without UTF8_SKIP_BOM */
	assert_int_equal(utf8_to_wchar("\xef\xbb\xbfx", 0, wide, 8, 0), 1);
	assert_int_equal(wide[0], L'x');
	assert_int_equal(utf8_to_wchar("\xef\xbb\xbfx", 4, wide, 8, UTF8_SKIP_BOM), 1);
	assert_int_equal(wide[0], L'x');
	/* only a leading one, and only from 3 bytes or more */
	assert_int_equal(utf8_to_wchar("x\xef\xbb\xbf", 4, wide, 8, 0), 2);
	assert_int_equal(wide[1], 0xfeff);
	assert_int_equal(utf8_to_wchar("\xef\xbb", 2, wide, 8, 0), 1);
	/* a BOM and nothing else leaves no input: 0 */
	assert_int_equal(utf8_to_wchar("\xef\xbb\xbf", 0, wide, 8, 0), 0);

	/* wide BOMs are kept */
	char narrow[8];
	assert_int_equal(wchar_to_utf8(L"\xfeffx", 0, narrow, 8, UTF8_SKIP_BOM), 4);
	assert_memory_equal(narrow, "\xef\xbb\xbfx", 4);
}

static void test_invalid_input_is_replaced(void **state)
{
	UNUSED_PARAMETER(state);

	wchar_t wide[8];
	char narrow[8];

	/* UTF8_IGNORE_ERROR changes nothing */
	for (int flags = 0; flags <= UTF8_IGNORE_ERROR; flags++) {
		assert_int_equal(utf8_to_wchar("a\xff"
					       "b",
					       3, wide, 8, flags),
				 3);
		assert_int_equal(wide[1], 0xfffd);
	}

	/* a lone surrogate becomes U+FFFD's three bytes */
	const wchar_t lone[] = {L'x', 0xd800, L'y', 0};
	assert_int_equal(wchar_to_utf8(lone, 0, narrow, 8, 0), 5);
	assert_memory_equal(narrow, "x\xef\xbf\xbdy", 5);
}

static void test_buffer_sizes(void **state)
{
	UNUSED_PARAMETER(state);

	wchar_t wide[8];
	char narrow[8];

	/* too small: 0 */
	assert_int_equal(utf8_to_wchar("abc", 3, wide, 2, 0), 0);
	assert_int_equal(wchar_to_utf8(L"abc", 3, narrow, 2, 0), 0);

	/* a buffer with size 0 asks for the size, like NULL */
	assert_int_equal(utf8_to_wchar("abc", 3, wide, 0, 0), 3);
	assert_int_equal(wchar_to_utf8(L"abc", 3, narrow, 0, 0), 3);

	/* NULL with a size is an invalid argument: 0 */
	assert_int_equal(utf8_to_wchar("abc", 3, NULL, 8, 0), 0);
	assert_int_equal(wchar_to_utf8(L"abc", 3, NULL, 8, 0), 0);

	/* empty input: 0 */
	assert_int_equal(utf8_to_wchar("", 0, wide, 8, 0), 0);
	assert_int_equal(wchar_to_utf8(L"", 0, narrow, 8, 0), 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_round_trip),
		cmocka_unit_test(test_lengths_and_terminators),
		cmocka_unit_test(test_bom_dropped_and_flags_ignored),
		cmocka_unit_test(test_invalid_input_is_replaced),
		cmocka_unit_test(test_buffer_sizes),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
