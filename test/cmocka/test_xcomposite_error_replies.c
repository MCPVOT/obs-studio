/* linux-capture: a failed X request must not leak its error reply (#2053).
 *
 * xcb hands every failed request's error back as a malloc'd
 * xcb_generic_error_t the caller frees. xcomp_window_exists() (the capture's
 * safety-net round trip) and xcb_xcursor_offset_win() never freed it, so a
 * capture whose window had closed leaked one error per check (ASan:
 * xcomposite-input.c.h and xcursor-xcb.c.h). This asks about a window that
 * does not exist many times and checks the heap does not grow by an error
 * per call. Builds the linux-capture unity file directly, as
 * test_xcomposite_window_events does. Skips without a display. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <malloc.h>
#include <stdio.h>

#include <xcb/xcb.h>

#include "plugins/linux-capture/xcursor-xcb.h"

bool xcomp_window_exists(xcb_connection_t *xconn, xcb_window_t win);

#define CALLS 1000

static xcb_connection_t *conn;
static xcb_window_t missing;

static int setup(void **state)
{
	(void)state;
	conn = xcb_connect(NULL, NULL);
	if (xcb_connection_has_error(conn)) {
		xcb_disconnect(conn);
		conn = NULL;
		return 0;
	}
	/* An id this client owns but never created: every request fails. */
	missing = xcb_generate_id(conn);
	return 0;
}

static int teardown(void **state)
{
	(void)state;
	if (conn)
		xcb_disconnect(conn);
	return 0;
}

static size_t heap_in_use(void)
{
	return mallinfo2().uordblks;
}

/* Less than one 32-byte error per call: allocator noise, not a leak. */
static void assert_no_growth(size_t before)
{
	size_t after = heap_in_use();
	size_t grown = after > before ? after - before : 0;
	fprintf(stderr, "heap grew by %zu bytes over %d calls\n", grown, CALLS);
	assert_true(grown < (size_t)CALLS * 8);
}

static void window_exists_frees_its_error(void **state)
{
	(void)state;
	if (!conn)
		skip();
	assert_false(xcomp_window_exists(conn, missing));

	size_t before = heap_in_use();
	for (int i = 0; i < CALLS; i++)
		assert_false(xcomp_window_exists(conn, missing));
	assert_no_growth(before);
}

static void cursor_offset_frees_its_error(void **state)
{
	(void)state;
	if (!conn)
		skip();
	xcb_xcursor_t *cursor = xcb_xcursor_init(conn);
	assert_non_null(cursor);
	xcb_xcursor_offset_win(conn, cursor, missing);

	size_t before = heap_in_use();
	for (int i = 0; i < CALLS; i++)
		xcb_xcursor_offset_win(conn, cursor, missing);
	assert_no_growth(before);
	xcb_xcursor_destroy(cursor);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(window_exists_frees_its_error),
		cmocka_unit_test(cursor_offset_frees_its_error),
	};
	return cmocka_run_group_tests(tests, setup, teardown);
}
