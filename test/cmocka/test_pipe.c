#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#ifndef _WIN32
#include <signal.h>
#endif

#include <util/bmem.h>
#include <util/pipe.h>

static void test_args_create(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_args_t *args = os_process_args_create("prog");
	assert_non_null(args);
	assert_int_equal(os_process_args_get_argc(args), 1);

	char **argv = os_process_args_get_argv(args);
	assert_non_null(argv);
	assert_string_equal(argv[0], "prog");
	/* argv is NULL-terminated */
	assert_null(argv[1]);

	os_process_args_destroy(args);
}

static void test_args_add_arg_order(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_args_t *args = os_process_args_create("prog");
	os_process_args_add_arg(args, "one");
	os_process_args_add_arg(args, "two");
	os_process_args_add_arg(args, "three");

	assert_int_equal(os_process_args_get_argc(args), 4);

	char **argv = os_process_args_get_argv(args);
	assert_string_equal(argv[0], "prog");
	assert_string_equal(argv[1], "one");
	assert_string_equal(argv[2], "two");
	assert_string_equal(argv[3], "three");
	assert_null(argv[4]);

	os_process_args_destroy(args);
}

static void test_args_add_argf(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_args_t *args = os_process_args_create("prog");
	os_process_args_add_argf(args, "%d-%s", 42, "abc");
	os_process_args_add_arg(args, "after");

	assert_int_equal(os_process_args_get_argc(args), 3);

	char **argv = os_process_args_get_argv(args);
	assert_string_equal(argv[1], "42-abc");
	assert_string_equal(argv[2], "after");
	assert_null(argv[3]);

	os_process_args_destroy(args);
}

static void test_args_verbatim(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_args_t *args = os_process_args_create("prog");
	os_process_args_add_arg(args, "has space");
	os_process_args_add_arg(args, "say \"hi\" 'there'");
	os_process_args_add_arg(args, "");

	assert_int_equal(os_process_args_get_argc(args), 4);

	char **argv = os_process_args_get_argv(args);
	assert_string_equal(argv[1], "has space");
	assert_string_equal(argv[2], "say \"hi\" 'there'");
	assert_string_equal(argv[3], "");
	assert_null(argv[4]);

	os_process_args_destroy(args);
}

static void test_args_destroy_null(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_args_destroy(NULL);
}

/* read until EOF (read returns 0); returns total bytes, buf is NUL-terminated */
static size_t read_all(os_process_pipe_t *pp, size_t (*reader)(os_process_pipe_t *, uint8_t *, size_t), char *buf,
		       size_t cap)
{
	size_t total = 0;

	for (;;) {
		if (total + 1 >= cap)
			break;
		size_t n = reader(pp, (uint8_t *)buf + total, cap - 1 - total);
		if (n == 0)
			break;
		total += n;
	}
	buf[total] = 0;
	return total;
}

#ifndef _WIN32

static void test_pipe_create_read(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_pipe_t *pp = os_process_pipe_create("printf 'hello'", "r");
	assert_non_null(pp);

	char buf[64];
	size_t n = read_all(pp, os_process_pipe_read, buf, sizeof(buf));
	assert_int_equal(n, 5);
	assert_string_equal(buf, "hello");

	assert_int_equal(os_process_pipe_destroy(pp), 0);
}

static void test_pipe_exit_code(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_pipe_t *pp = os_process_pipe_create("exit 3", "r");
	assert_non_null(pp);

	char buf[16];
	assert_int_equal(read_all(pp, os_process_pipe_read, buf, sizeof(buf)), 0);
	assert_int_equal(os_process_pipe_destroy(pp), 3);
}

static void test_pipe_exit_code_signed_char(void **state)
{
	UNUSED_PARAMETER(state);

	/*
	 * Characterized, not endorsed: the exit status is cast through (char), so
	 * 255 becomes -1 where char is signed (x86, Apple arm64) and stays 255
	 * where it is unsigned (Linux aarch64).
	 */
	os_process_pipe_t *pp = os_process_pipe_create("exit 255", "r");
	assert_non_null(pp);

	char buf[16];
	read_all(pp, os_process_pipe_read, buf, sizeof(buf));
	assert_int_equal(os_process_pipe_destroy(pp), (int)(char)255);
}

static void test_pipe_create2_stdout_stderr(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_args_t *args = os_process_args_create("/bin/sh");
	os_process_args_add_arg(args, "-c");
	os_process_args_add_arg(args, "printf abc; printf err 1>&2");

	os_process_pipe_t *pp = os_process_pipe_create2(args, "r");
	assert_non_null(pp);

	char buf[64];
	size_t n = read_all(pp, os_process_pipe_read, buf, sizeof(buf));
	assert_int_equal(n, 3);
	assert_string_equal(buf, "abc");

	n = read_all(pp, os_process_pipe_read_err, buf, sizeof(buf));
	assert_int_equal(n, 3);
	assert_string_equal(buf, "err");

	assert_int_equal(os_process_pipe_destroy(pp), 0);
	os_process_args_destroy(args);
}

static void test_pipe_create2_missing_binary(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_args_t *args = os_process_args_create("/nonexistent/obs-test-binary");
	assert_null(os_process_pipe_create2(args, "r"));
	os_process_args_destroy(args);
}

static void test_pipe_write_mode(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_pipe_t *pp = os_process_pipe_create("cat > /dev/null", "w");
	assert_non_null(pp);

	const uint8_t data[] = "some data";
	assert_int_equal(os_process_pipe_write(pp, data, sizeof(data) - 1), sizeof(data) - 1);

	/* reading from a write pipe yields nothing */
	uint8_t rb[4];
	assert_int_equal(os_process_pipe_read(pp, rb, sizeof(rb)), 0);

	assert_int_equal(os_process_pipe_destroy(pp), 0);
}

#ifdef __linux__
static void test_pipe_write_stops_after_partial_error(void **state)
{
	UNUSED_PARAMETER(state);

	/* More than Linux's default pipe capacity, including on 64 KiB-page hosts. */
	const size_t len = 16 * 1024 * 1024;
	uint8_t *data = bzalloc(len);
	assert_non_null(data);

	struct sigaction ignore = {0};
	struct sigaction previous;
	ignore.sa_handler = SIG_IGN;
	sigemptyset(&ignore.sa_mask);
	assert_int_equal(sigaction(SIGPIPE, &ignore, &previous), 0);

	/*
	 * dd consumes exactly one byte and exits. The large write must therefore
	 * transfer a prefix and encounter EPIPE. The EOF handshake ensures stderr
	 * cannot keep the parent waiting while dd waits for that first byte.
	 */
	os_process_pipe_t *pp = os_process_pipe_create("exec 2>&-; dd bs=1 count=1 of=/dev/null 2>/dev/null", "w");
	bool created = pp != NULL;
	size_t err_bytes = 0;
	size_t written = 0;
	size_t next_written = 0;
	int exit_status = -1;
	if (pp) {
		uint8_t byte;
		err_bytes = os_process_pipe_read_err(pp, &byte, 1);
		written = os_process_pipe_write(pp, data, len);
		next_written = os_process_pipe_write(pp, data, len);
		exit_status = os_process_pipe_destroy(pp);
	}

	int restore_status = sigaction(SIGPIPE, &previous, NULL);
	bfree(data);
	assert_int_equal(restore_status, 0);
	assert_true(created);
	assert_int_equal(err_bytes, 0);
	assert_int_equal(exit_status, 0);
	assert_true(written > 0);
	assert_true(written < len);
	assert_int_equal(next_written, 0);
}
#endif

static void test_pipe_write_on_read_pipe(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_pipe_t *pp = os_process_pipe_create("exit 0", "r");
	assert_non_null(pp);

	const uint8_t data[] = "x";
	assert_int_equal(os_process_pipe_write(pp, data, 1), 0);

	assert_int_equal(os_process_pipe_destroy(pp), 0);
}

static void test_pipe_type_handling(void **state)
{
	UNUSED_PARAMETER(state);

	/* only the first character 'r' selects read mode; anything else is write mode */
	os_process_pipe_t *pp = os_process_pipe_create("cat > /dev/null", "x");
	assert_non_null(pp);

	const uint8_t data[] = "ab";
	assert_int_equal(os_process_pipe_write(pp, data, 2), 2);
	assert_int_equal(os_process_pipe_destroy(pp), 0);
}

static void test_pipe_null_arguments(void **state)
{
	UNUSED_PARAMETER(state);

	assert_null(os_process_pipe_create(NULL, "r"));
	assert_null(os_process_pipe_create("exit 0", NULL));

	uint8_t b[4];
	assert_int_equal(os_process_pipe_read(NULL, b, sizeof(b)), 0);
	assert_int_equal(os_process_pipe_read_err(NULL, b, sizeof(b)), 0);
	assert_int_equal(os_process_pipe_write(NULL, b, sizeof(b)), 0);
	assert_int_equal(os_process_pipe_destroy(NULL), 0);
}

#else /* _WIN32 */

/* pipe-windows.c runs the command line itself (CreateProcessW with no
 * application name), so the children here are cmd.exe built-ins. cmd's echo
 * prints the rest of its command line as given, which shows how create2
 * quotes arguments. */

static void test_pipe_create_read(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_pipe_t *pp = os_process_pipe_create("cmd /c echo hello", "r");
	assert_non_null(pp);

	char buf[64];
	size_t n = read_all(pp, os_process_pipe_read, buf, sizeof(buf));
	assert_int_equal(n, 7);
	assert_string_equal(buf, "hello\r\n");

	assert_int_equal(os_process_pipe_destroy(pp), 0);
}

static void test_pipe_exit_code(void **state)
{
	UNUSED_PARAMETER(state);

	/* the exit code is returned as (int)DWORD, with no char cast */
	const struct {
		const char *cmd;
		int code;
	} cases[] = {
		{"cmd /c exit 3", 3},
		{"cmd /c exit 255", 255},
		{"cmd /c exit 256", 256},
		{"cmd /c exit -1", -1},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		os_process_pipe_t *pp = os_process_pipe_create(cases[i].cmd, "r");
		assert_non_null(pp);

		char buf[16];
		assert_int_equal(read_all(pp, os_process_pipe_read, buf, sizeof(buf)), 0);
		assert_int_equal(os_process_pipe_destroy(pp), cases[i].code);
	}
}

static void test_pipe_stderr(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_pipe_t *pp = os_process_pipe_create("cmd /c (echo out) & (1>&2 echo err) & exit 5", "r");
	assert_non_null(pp);

	char buf[64];
	assert_int_equal(read_all(pp, os_process_pipe_read, buf, sizeof(buf)), 5);
	assert_string_equal(buf, "out\r\n");
	assert_int_equal(read_all(pp, os_process_pipe_read_err, buf, sizeof(buf)), 5);
	assert_string_equal(buf, "err\r\n");

	assert_int_equal(os_process_pipe_destroy(pp), 5);
}

static void test_pipe_create2_quoting(void **state)
{
	UNUSED_PARAMETER(state);

	/* Quoted when empty or containing a space or tab. A quote is escaped
	 * with a backslash, and backslashes before a quote, or before the
	 * closing quote, are doubled; other backslashes are kept as is. */
	os_process_args_t *args = os_process_args_create("cmd");
	os_process_args_add_arg(args, "/c");
	os_process_args_add_arg(args, "echo");
	os_process_args_add_arg(args, "plain");
	os_process_args_add_arg(args, "a b");
	os_process_args_add_arg(args, "");
	os_process_args_add_arg(args, "t\tb");
	os_process_args_add_arg(args, "x\"y");
	os_process_args_add_arg(args, "q \"z");
	os_process_args_add_arg(args, "b\\\"c");
	os_process_args_add_arg(args, "dir\\");
	os_process_args_add_arg(args, "s p\\");
	os_process_args_add_arg(args, "C:\\a\\b");

	os_process_pipe_t *pp = os_process_pipe_create2(args, "r");
	assert_non_null(pp);

	char buf[256];
	read_all(pp, os_process_pipe_read, buf, sizeof(buf));
	assert_string_equal(buf,
			    "plain \"a b\" \"\" \"t\tb\" x\\\"y \"q \\\"z\" b\\\\\\\"c dir\\ \"s p\\\\\" C:\\a\\b\r\n");

	assert_int_equal(os_process_pipe_destroy(pp), 0);
	os_process_args_destroy(args);
}

static void test_pipe_create2_missing_binary(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_args_t *args = os_process_args_create("obs-test-binary-that-does-not-exist");
	assert_null(os_process_pipe_create2(args, "r"));
	os_process_args_destroy(args);

	assert_null(os_process_pipe_create("", "r"));
}

static void test_pipe_write_mode(void **state)
{
	UNUSED_PARAMETER(state);

	/* the child's stdout is not captured in write mode */
	os_process_pipe_t *pp = os_process_pipe_create("cmd /c more > NUL", "w");
	assert_non_null(pp);

	const uint8_t data[] = "some data\r\n";
	assert_int_equal(os_process_pipe_write(pp, data, sizeof(data) - 1), sizeof(data) - 1);

	/* reading from a write pipe yields nothing */
	uint8_t rb[4];
	assert_int_equal(os_process_pipe_read(pp, rb, sizeof(rb)), 0);

	assert_int_equal(os_process_pipe_destroy(pp), 0);
}

static void test_pipe_write_on_read_pipe(void **state)
{
	UNUSED_PARAMETER(state);

	os_process_pipe_t *pp = os_process_pipe_create("cmd /c exit 0", "r");
	assert_non_null(pp);

	const uint8_t data[] = "x";
	assert_int_equal(os_process_pipe_write(pp, data, 1), 0);

	assert_int_equal(os_process_pipe_destroy(pp), 0);
}

static void test_pipe_type_handling(void **state)
{
	UNUSED_PARAMETER(state);

	/* unlike pipe-posix.c, only a leading 'r' or 'w' is accepted */
	assert_null(os_process_pipe_create("cmd /c exit 0", "x"));
	assert_null(os_process_pipe_create("cmd /c exit 0", ""));

	os_process_pipe_t *pp = os_process_pipe_create("cmd /c more > NUL", "wx");
	assert_non_null(pp);
	const uint8_t data[] = "ab";
	assert_int_equal(os_process_pipe_write(pp, data, 2), 2);
	assert_int_equal(os_process_pipe_destroy(pp), 0);

	pp = os_process_pipe_create("cmd /c echo r", "rw");
	assert_non_null(pp);
	char buf[16];
	read_all(pp, os_process_pipe_read, buf, sizeof(buf));
	assert_string_equal(buf, "r\r\n");
	assert_int_equal(os_process_pipe_destroy(pp), 0);
}

static void test_pipe_null_arguments(void **state)
{
	UNUSED_PARAMETER(state);

	assert_null(os_process_pipe_create(NULL, "r"));
	assert_null(os_process_pipe_create("cmd /c exit 0", NULL));

	uint8_t b[4];
	assert_int_equal(os_process_pipe_read(NULL, b, sizeof(b)), 0);
	assert_int_equal(os_process_pipe_read_err(NULL, b, sizeof(b)), 0);
	assert_int_equal(os_process_pipe_write(NULL, b, sizeof(b)), 0);
	assert_int_equal(os_process_pipe_destroy(NULL), 0);
}

#endif /* _WIN32 */

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_args_create),
		cmocka_unit_test(test_args_add_arg_order),
		cmocka_unit_test(test_args_add_argf),
		cmocka_unit_test(test_args_verbatim),
		cmocka_unit_test(test_args_destroy_null),
		cmocka_unit_test(test_pipe_create_read),
		cmocka_unit_test(test_pipe_exit_code),
#ifndef _WIN32
		cmocka_unit_test(test_pipe_exit_code_signed_char),
		cmocka_unit_test(test_pipe_create2_stdout_stderr),
#else
		cmocka_unit_test(test_pipe_stderr),
		cmocka_unit_test(test_pipe_create2_quoting),
#endif
		cmocka_unit_test(test_pipe_create2_missing_binary),
		cmocka_unit_test(test_pipe_write_mode),
#ifdef __linux__
		cmocka_unit_test(test_pipe_write_stops_after_partial_error),
#endif
		cmocka_unit_test(test_pipe_write_on_read_pipe),
		cmocka_unit_test(test_pipe_type_handling),
		cmocka_unit_test(test_pipe_null_arguments),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
