// The C++ headers go first: cmocka.h defines macros (fail, ...) that clash
// with libstdc++.
#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
// The Windows cmocka.h has no extern "C" guard of its own.
extern "C" {
#include <cmocka.h>
}

#include "disk-space-monitor.hpp"

/* obs-rust/obs-studio#1282: the recording timer (every second) and the stats
 * window (every two seconds) queried free disk space on the UI thread; on a
 * hung network path each query blocked for seconds and froze the UI. */

using Clock = std::chrono::steady_clock;

static std::atomic<int> calls{0};

/* A slow volume: 2 s per query. "/a" has 111 bytes free, anything else 222. */
static uint64_t slow_free_space(const char *path)
{
	calls++;
	std::this_thread::sleep_for(std::chrono::seconds(2));
	return std::string(path) == "/a" ? 111 : 222;
}

static long long ms_since(Clock::time_point t)
{
	return (long long)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count();
}

/* Wait (bounded) until latest(path) is valid. */
static DiskSpaceSample wait_for(DiskSpaceMonitor &m, const std::string &path)
{
	auto start = Clock::now();
	DiskSpaceSample s = m.latest(path);
	while (!s.valid && ms_since(start) < 10000) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		s = m.latest(path);
	}
	return s;
}

static void request_and_latest_do_not_block(void **state)
{
	(void)state;
	calls = 0;
	DiskSpaceMonitor m(slow_free_space);

	auto t = Clock::now();
	m.request("/a");
	DiskSpaceSample s = m.latest("/a");
	long long elapsed = ms_since(t);

	assert_true(elapsed < 10);
	assert_false(s.valid);

	s = wait_for(m, "/a");
	assert_true(s.valid);
	assert_int_equal(s.bytes, 111);
	assert_true(s.timestamp_ns != 0);
}

static void requests_in_flight_are_coalesced(void **state)
{
	(void)state;
	calls = 0;
	DiskSpaceMonitor m(slow_free_space);

	auto t = Clock::now();
	for (int i = 0; i < 50; i++) {
		m.request("/a");
	}
	assert_true(ms_since(t) < 10);

	assert_true(wait_for(m, "/a").valid);
	/* give a second query a chance to start if one was queued */
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	assert_int_equal(m.queries_started(), 1);
}

static void path_change_returns_new_value(void **state)
{
	(void)state;
	calls = 0;
	DiskSpaceMonitor m(slow_free_space);

	m.request("/a");
	assert_int_equal(wait_for(m, "/a").bytes, 111);

	auto t = Clock::now();
	m.request("/b");
	/* the old path's value is not reported for the new path */
	assert_false(m.latest("/b").valid);
	assert_true(ms_since(t) < 10);

	DiskSpaceSample s = wait_for(m, "/b");
	assert_true(s.valid);
	assert_int_equal(s.bytes, 222);
}

static void destroy_does_not_wait_for_hung_query(void **state)
{
	(void)state;
	auto t = Clock::now();
	{
		DiskSpaceMonitor m(slow_free_space);
		m.request("/a");
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	assert_true(ms_since(t) < 1500);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(request_and_latest_do_not_block),
		cmocka_unit_test(requests_in_flight_are_coalesced),
		cmocka_unit_test(path_change_returns_new_value),
		cmocka_unit_test(destroy_does_not_wait_for_hung_query),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
