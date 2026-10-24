#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

#include "plugins/obs-outputs/rtmp-stream-host-posix.c.h"

/*
 * The RTMP output's POSIX host (obs-rust/obs-studio#851): socket errors map
 * to the same locale keys, sockets go non-blocking and report pending bytes
 * through ioctl, the last error is errno, and the Windows-only socket loop
 * is refused. The Windows host (WSA errors, ioctlsocket, send thread) is
 * compiled by the cross check.
 */

const char *obs_module_text(const char *key)
{
	return key;
}

static void test_error_text(void **state)
{
	(void)state;
	assert_string_equal(rtmp_host_socket_error_text(ETIMEDOUT), "ConnectionTimedOut");
	assert_string_equal(rtmp_host_socket_error_text(ECONNRESET), "ConnectionReset");
	assert_string_equal(rtmp_host_socket_error_text(EHOSTUNREACH), "NoRoute");
	assert_null(rtmp_host_socket_error_text(0));
}

static void test_root_cert_error(void **state)
{
	(void)state;
	const char *msg = rtmp_host_root_cert_error();
	assert_non_null(strstr(msg, "Failed to load root certificates for a secure TLS connection."));
}

static void test_socket_calls(void **state)
{
	(void)state;
	int fds[2];
	int size = -1;

	assert_int_equal(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
	assert_int_equal(rtmp_host_set_nonblocking((uintptr_t)fds[0]), 0);
	assert_true(fcntl(fds[0], F_GETFL) & O_NONBLOCK);

	assert_int_equal(write(fds[1], "abc", 3), 3);
	assert_true(rtmp_host_bytes_available((uintptr_t)fds[0], &size) >= 0);
	assert_int_equal(size, 3);

	close(fds[0]);
	close(fds[1]);

	assert_true(rtmp_host_bytes_available((uintptr_t)fds[0], &size) < 0);
	assert_int_equal(rtmp_host_last_socket_error(), EBADF);
	assert_int_equal(rtmp_host_set_nonblocking((uintptr_t)fds[0]), EBADF);
}

static void test_no_socket_loop(void **state)
{
	(void)state;
	assert_false(rtmp_host_has_socket_loop());
	assert_false(rtmp_host_start_socket_loop(NULL));
	rtmp_host_log_sndbuf_size(NULL);
	rtmp_host_log_interface_type(NULL);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_error_text),
		cmocka_unit_test(test_root_cert_error),
		cmocka_unit_test(test_socket_calls),
		cmocka_unit_test(test_no_socket_loop),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
