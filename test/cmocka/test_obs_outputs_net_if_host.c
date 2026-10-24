#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "plugins/obs-outputs/net-if.c.h"
#include "plugins/obs-outputs/net-if-host-posix.c.h"

/*
 * obs-outputs' network interface list with its POSIX host
 * (obs-rust/obs-studio#851): addresses parse and print back unchanged for
 * IPv4 and IPv6, and the bind-address list leaves out loopback interfaces.
 * The Windows host (WSAStringToAddressA, GetAdaptersAddresses) is compiled
 * by the cross check.
 */

static void round_trip(const char *text, int family, int len)
{
	struct sockaddr_storage addr;
	char back[INET6_ADDRSTRLEN];
	int addr_len;

	assert_true(netif_str_to_addr(&addr, &addr_len, text));
	assert_int_equal(addr.ss_family, family);
	assert_int_equal(addr_len, len);
	assert_true(netif_addr_to_str(&addr, back, sizeof(back)));
	assert_string_equal(back, text);
}

static void test_ipv4_round_trip(void **state)
{
	(void)state;
	round_trip("192.168.1.2", AF_INET, (int)sizeof(struct sockaddr_in));
}

static void test_ipv6_round_trip(void **state)
{
	(void)state;
	round_trip("fe80::1", AF_INET6, (int)sizeof(struct sockaddr_in6));
}

static void test_rejects_garbage(void **state)
{
	(void)state;
	struct sockaddr_storage addr;
	int addr_len;

	assert_false(netif_str_to_addr(&addr, &addr_len, "not-an-address"));
	assert_false(netif_str_to_addr(&addr, &addr_len, NULL));
}

static void test_list_skips_loopback(void **state)
{
	(void)state;
	struct netif_saddr_data data = {0};

	netif_get_addrs(&data);
	for (size_t i = 0; i < data.addrs.num; i++) {
		assert_null(strstr(data.addrs.array[i].name, "[lo]"));
		assert_null(strstr(data.addrs.array[i].name, "[lo0]"));
	}
	netif_saddr_data_free(&data);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_ipv4_round_trip),
		cmocka_unit_test(test_ipv6_round_trip),
		cmocka_unit_test(test_rejects_garbage),
		cmocka_unit_test(test_list_skips_loopback),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
