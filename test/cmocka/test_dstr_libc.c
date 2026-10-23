#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <wchar.h>
#include <cmocka.h>

#include <util/bmem.h>
#include <util/dstr.h>

/* Characterization tests for libobs/util/dstr-libc.c: the wide-string
 * helpers and the dstr <-> multibyte/wide conversions, which test_dstr does
 * not call. Every assertion pins what the C does today, including the null
 * handling. Case mapping only uses ASCII, the part every C locale maps the
 * same way. Each test is bracketed by an allocation-count check. */

static long allocs_at_start;

static int leak_setup(void **state)
{
	UNUSED_PARAMETER(state);
	allocs_at_start = bnum_allocs();
	return 0;
}

static int leak_teardown(void **state)
{
	UNUSED_PARAMETER(state);
	assert_int_equal(bnum_allocs(), allocs_at_start);
	return 0;
}

static void test_wstrcmpi(void **state)
{
	UNUSED_PARAMETER(state);

	assert_int_equal(wstrcmpi(L"Hello", L"hELLO"), 0);
	assert_int_equal(wstrcmpi(L"abc", L"ABD"), -1);
	assert_int_equal(wstrcmpi(L"abd", L"ABC"), 1);
	/* a prefix is smaller */
	assert_int_equal(wstrcmpi(L"ab", L"abc"), -1);
	assert_int_equal(wstrcmpi(L"abc", L"ab"), 1);
	/* NULL is the empty string */
	assert_int_equal(wstrcmpi(NULL, NULL), 0);
	assert_int_equal(wstrcmpi(NULL, L""), 0);
	assert_int_equal(wstrcmpi(L"", NULL), 0);
	assert_int_equal(wstrcmpi(NULL, L"a"), -1);
	assert_int_equal(wstrcmpi(L"a", NULL), 1);
	/* case folds to upper: '_' (0x5F) sorts after 'A'..'Z' */
	assert_int_equal(wstrcmpi(L"a", L"_"), -1);
}

static void test_wstrcmp_n(void **state)
{
	UNUSED_PARAMETER(state);

	assert_int_equal(wstrcmp_n(L"abc", L"abd", 2), 0);
	assert_int_equal(wstrcmp_n(L"abc", L"abd", 3), -1);
	assert_int_equal(wstrcmp_n(L"abd", L"abc", 3), 1);
	assert_int_equal(wstrcmp_n(L"abc", L"ABC", 3), 1);
	assert_int_equal(wstrcmp_n(L"abc", L"abc", 100), 0);
	assert_int_equal(wstrcmp_n(L"ab", L"abc", 100), -1);
	/* n == 0 compares nothing, even against NULL */
	assert_int_equal(wstrcmp_n(L"a", L"b", 0), 0);
	assert_int_equal(wstrcmp_n(NULL, L"b", 0), 0);
	assert_int_equal(wstrcmp_n(NULL, NULL, 5), 0);
	assert_int_equal(wstrcmp_n(NULL, L"b", 5), -1);
	assert_int_equal(wstrcmp_n(L"b", NULL, 5), 1);
}

static void test_wstrcmpi_n(void **state)
{
	UNUSED_PARAMETER(state);

	assert_int_equal(wstrcmpi_n(L"ABc", L"abD", 2), 0);
	assert_int_equal(wstrcmpi_n(L"ABc", L"abD", 3), -1);
	assert_int_equal(wstrcmpi_n(L"abD", L"ABc", 3), 1);
	assert_int_equal(wstrcmpi_n(L"Hello", L"hello", 100), 0);
	assert_int_equal(wstrcmpi_n(L"a", L"b", 0), 0);
	assert_int_equal(wstrcmpi_n(NULL, NULL, 3), 0);
	assert_int_equal(wstrcmpi_n(NULL, L"x", 3), -1);
	assert_int_equal(wstrcmpi_n(L"x", NULL, 3), 1);
}

static void test_wstrstri(void **state)
{
	UNUSED_PARAMETER(state);

	const wchar_t *hay = L"Hello World";

	assert_ptr_equal(wstrstri(hay, L"WORLD"), hay + 6);
	assert_ptr_equal(wstrstri(hay, L"lo w"), hay + 3);
	assert_ptr_equal(wstrstri(hay, L"hello"), hay);
	assert_null(wstrstri(hay, L"worlds"));
	assert_null(wstrstri(hay, L"xyz"));
	/* an empty needle matches at the start */
	assert_ptr_equal(wstrstri(hay, L""), hay);
	const wchar_t *empty = L"";
	assert_ptr_equal(wstrstri(empty, L""), empty);
	/* NULL on either side finds nothing */
	assert_null(wstrstri(NULL, L"a"));
	assert_null(wstrstri(hay, NULL));
}

static void test_wcsdepad(void **state)
{
	UNUSED_PARAMETER(state);

	wchar_t buf[32];

	wcscpy(buf, L" \t\r\nab c \n\t\r ");
	assert_ptr_equal(wcsdepad(buf), buf);
	assert_true(wcscmp(buf, L"ab c") == 0);

	wcscpy(buf, L"abc");
	assert_ptr_equal(wcsdepad(buf), buf);
	assert_true(wcscmp(buf, L"abc") == 0);

	/* only the four padding characters: a vertical tab stays */
	wcscpy(buf, L"\vab\v");
	wcsdepad(buf);
	assert_true(wcscmp(buf, L"\vab\v") == 0);

	/* all padding becomes empty */
	wcscpy(buf, L" \t \n");
	assert_ptr_equal(wcsdepad(buf), buf);
	assert_int_equal(buf[0], 0);

	buf[0] = 0;
	assert_ptr_equal(wcsdepad(buf), buf);
	assert_int_equal(buf[0], 0);

	assert_null(wcsdepad(NULL));
}

#ifndef _WIN32
static void test_dstr_wcs(void **state)
{
	UNUSED_PARAMETER(state);

	/* UTF-8 with 2-, 3- and 4-byte sequences */
	const char *utf8 = "h\xc3\xa9llo \xe2\x82\xac \xf0\x9f\x98\x80";
	const wchar_t wide[] = {L'h', 0xe9, L'l', L'l', L'o', L' ', 0x20ac, L' ', 0x1f600, 0};

	struct dstr d = {0};
	dstr_copy(&d, utf8);
	wchar_t *w = dstr_to_wcs(&d);
	assert_non_null(w);
	assert_true(wcscmp(w, wide) == 0);
	bfree(w);
	dstr_free(&d);

	dstr_from_wcs(&d, wide);
	assert_string_equal(d.array, utf8);
	assert_int_equal(d.len, strlen(utf8));
	dstr_free(&d);

	/* an empty wide string frees the destination */
	dstr_copy(&d, "old");
	dstr_from_wcs(&d, L"");
	assert_null(d.array);
	assert_int_equal(d.len, 0);
	assert_int_equal(d.capacity, 0);

	/* an empty dstr (NULL array) converts to NULL */
	assert_null(dstr_to_wcs(&d));
	assert_null(dstr_to_mbs(&d));
}

static void test_dstr_mbs(void **state)
{
	UNUSED_PARAMETER(state);

	struct dstr d = {0};
	dstr_copy(&d, "old");
	dstr_from_mbs(&d, "plain ascii");
	assert_string_equal(d.array, "plain ascii");
	assert_int_equal(d.len, 11);
	/* dstr_to_mbs converts with os_mbs_to_utf8_ptr, as the C does */
	char *m = dstr_to_mbs(&d);
	assert_string_equal(m, "plain ascii");
	bfree(m);
	dstr_free(&d);

	/* a NULL source frees and leaves NULL */
	dstr_copy(&d, "old");
	dstr_from_mbs(&d, NULL);
	assert_null(d.array);
	assert_int_equal(d.len, 0);
}

static void test_dstr_case_ascii(void **state)
{
	UNUSED_PARAMETER(state);

	struct dstr d = {0};

	/* a dstr whose first byte is NUL counts as empty and is not touched */
	dstr_copy(&d, "x");
	d.array[0] = 0;
	dstr_to_upper(&d);
	assert_int_equal(d.len, 1);
	assert_int_equal(d.array[0], 0);
	dstr_free(&d);

	dstr_copy(&d, "MiXeD 09_az{}");
	dstr_to_upper(&d);
	assert_string_equal(d.array, "MIXED 09_AZ{}");
	dstr_to_lower(&d);
	assert_string_equal(d.array, "mixed 09_az{}");
	assert_int_equal(d.len, 13);
	dstr_free(&d);
}
#endif

int main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_wstrcmpi, leak_setup, leak_teardown),
		cmocka_unit_test_setup_teardown(test_wstrcmp_n, leak_setup, leak_teardown),
		cmocka_unit_test_setup_teardown(test_wstrcmpi_n, leak_setup, leak_teardown),
		cmocka_unit_test_setup_teardown(test_wstrstri, leak_setup, leak_teardown),
		cmocka_unit_test_setup_teardown(test_wcsdepad, leak_setup, leak_teardown),
#ifndef _WIN32
		cmocka_unit_test_setup_teardown(test_dstr_wcs, leak_setup, leak_teardown),
		cmocka_unit_test_setup_teardown(test_dstr_mbs, leak_setup, leak_teardown),
		cmocka_unit_test_setup_teardown(test_dstr_case_ascii, leak_setup, leak_teardown),
#endif
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
