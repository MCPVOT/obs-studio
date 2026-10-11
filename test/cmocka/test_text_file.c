#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <cmocka.h>

#include <util/bmem.h>
#include <util/platform.h>

#include "text-file.h"

/* The text files of the FreeType 2 text source: UTF-8, or UTF-16LE with a
 * BOM, carriage returns removed, optionally only the last lines.
 *
 * Regressions: UTF-16 files were copied into the wchar_t buffer as raw
 * 2-byte units, which is wrong where wchar_t is 4 bytes and left the text
 * without a terminator; an odd-sized UTF-16 file also over-read on Windows
 * and made the backwards scan of ft2_read_text_file_end wrap around. */

#define TEXT_PATH "test_text_file.txt"

static void write_bytes(const void *data, size_t size)
{
	FILE *file = fopen(TEXT_PATH, "wb");
	assert_non_null(file);
	assert_int_equal(fwrite(data, 1, size, file), size);
	fclose(file);
}

static void write_utf8(const char *text)
{
	write_bytes(text, strlen(text));
}

/* BOM, then each UTF-16 unit little-endian, then `extra` stray bytes */
static void write_utf16(const uint16_t *units, size_t count, size_t extra)
{
	uint8_t data[256];
	size_t size = 0;
	data[size++] = 0xff;
	data[size++] = 0xfe;
	for (size_t i = 0; i < count; i++) {
		data[size++] = (uint8_t)units[i];
		data[size++] = (uint8_t)(units[i] >> 8);
	}
	for (size_t i = 0; i < extra; i++)
		data[size++] = 'x';
	assert_true(size <= sizeof(data));
	write_bytes(data, size);
}

/* an ASCII string as UTF-16 units */
static size_t ascii_units(const char *text, uint16_t *units)
{
	size_t n = 0;
	while (text[n]) {
		units[n] = (uint8_t)text[n];
		n++;
	}
	return n;
}

static void write_utf16_ascii(const char *text, size_t extra)
{
	uint16_t units[100];
	write_utf16(units, ascii_units(text, units), extra);
}

/* `got` is the wide string of the UTF-8 `want`; frees `got` */
static void expect_text(wchar_t *got, const char *want)
{
	wchar_t *want_wcs;
	os_utf8_to_wcs_ptr(want, 0, &want_wcs);
	assert_non_null(got);
	assert_int_equal(wcslen(got), wcslen(want_wcs));
	assert_memory_equal(got, want_wcs, (wcslen(want_wcs) + 1) * sizeof(wchar_t));
	bfree(want_wcs);
	bfree(got);
}

static int teardown(void **state)
{
	(void)state;
	remove(TEXT_PATH);
	return 0;
}

static void test_utf8_file(void **state)
{
	(void)state;

	write_utf8("one\r\ntwo \xc3\xa9\n");
	expect_text(ft2_read_text_file(TEXT_PATH), "one\ntwo \xc3\xa9\n");

	write_utf8("");
	expect_text(ft2_read_text_file(TEXT_PATH), "");

	remove(TEXT_PATH);
	assert_null(ft2_read_text_file(TEXT_PATH));
	assert_null(ft2_read_text_file_end(TEXT_PATH, 1));
}

static void test_utf16_file(void **state)
{
	(void)state;

	write_utf16_ascii("one\r\ntwo", 0);
	expect_text(ft2_read_text_file(TEXT_PATH), "one\ntwo");

	/* U+00E9 and U+1F600, a surrogate pair */
	const uint16_t units[] = {'a', 0xe9, 0xd83d, 0xde00, 'b'};
	write_utf16(units, 5, 0);
	expect_text(ft2_read_text_file(TEXT_PATH), "a\xc3\xa9\xf0\x9f\x98\x80"
						   "b");

	write_utf16(NULL, 0, 0);
	expect_text(ft2_read_text_file(TEXT_PATH), "");
}

/* A stray last byte is not half of a character. */
static void test_utf16_odd_size(void **state)
{
	(void)state;

	write_utf16_ascii("h", 1);
	expect_text(ft2_read_text_file(TEXT_PATH), "h");
	expect_text(ft2_read_text_file_end(TEXT_PATH, 10), "h");

	write_utf16(NULL, 0, 1);
	expect_text(ft2_read_text_file(TEXT_PATH), "");
	expect_text(ft2_read_text_file_end(TEXT_PATH, 10), "");
}

/* log_lines counts line breaks from the end, so the text keeps one more
 * line than log_lines when the file does not end with one. */
static void test_utf8_end(void **state)
{
	(void)state;

	write_utf8("a\nb\nc");
	expect_text(ft2_read_text_file_end(TEXT_PATH, 0), "c");
	expect_text(ft2_read_text_file_end(TEXT_PATH, 1), "b\nc");
	expect_text(ft2_read_text_file_end(TEXT_PATH, 5), "a\nb\nc");

	write_utf8("a\r\nb\r\nc\r\n");
	expect_text(ft2_read_text_file_end(TEXT_PATH, 1), "c\n");
}

/* The same lines, as UTF-16, give the same text (without the BOM). */
static void test_utf16_end_matches_utf8(void **state)
{
	(void)state;

	const char *texts[] = {"a\nb\nc", "a\r\nbb\nccc\n\ndd\n", "\nx", "single", ""};
	for (size_t t = 0; t < sizeof(texts) / sizeof(texts[0]); t++) {
		for (uint32_t lines = 0; lines < 7; lines++) {
			write_utf8(texts[t]);
			wchar_t *want = ft2_read_text_file_end(TEXT_PATH, lines);
			char *want_utf8;
			os_wcs_to_utf8_ptr(want, 0, &want_utf8);
			bfree(want);

			write_utf16_ascii(texts[t], 0);
			expect_text(ft2_read_text_file_end(TEXT_PATH, lines), want_utf8);
			bfree(want_utf8);
		}
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_teardown(test_utf8_file, teardown),
		cmocka_unit_test_teardown(test_utf16_file, teardown),
		cmocka_unit_test_teardown(test_utf16_odd_size, teardown),
		cmocka_unit_test_teardown(test_utf8_end, teardown),
		cmocka_unit_test_teardown(test_utf16_end_matches_utf8, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
