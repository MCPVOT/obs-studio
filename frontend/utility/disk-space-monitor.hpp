#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

/* Free disk space, queried off the UI thread (obs-rust/obs-studio#1282).
 *
 * os_get_free_disk_space() (statvfs / GetDiskFreeSpaceEx) can block for
 * seconds on a hung network or NAS path. The recording timer and the stats
 * window call request() on each tick and read latest(); both return at once.
 * One worker thread runs at most one query at a time: a request made while a
 * query is in flight is coalesced (a request for the same path is dropped, one
 * for another path replaces the single pending slot).
 *
 * Shutdown never waits on a hung query for long: the destructor tells the
 * worker to stop and waits a bounded time for it to exit. A worker still
 * stuck in the free-space function after that is detached; it shares its
 * state through a shared_ptr, so it touches no freed memory when the query
 * finally returns, and it exits without starting another. Not Qt, so it is
 * unit tested without a GUI. */

struct DiskSpaceSample {
	uint64_t bytes = 0;
	/* os_gettime_ns() when the query finished */
	uint64_t timestamp_ns = 0;
	/* false until a query for this path has finished */
	bool valid = false;
};

class DiskSpaceMonitor {
public:
	using FreeSpaceFn = uint64_t (*)(const char *path);

	/* free_space: NULL means os_get_free_disk_space(). */
	explicit DiskSpaceMonitor(FreeSpaceFn free_space = nullptr);
	~DiskSpaceMonitor();

	DiskSpaceMonitor(const DiskSpaceMonitor &) = delete;
	DiskSpaceMonitor &operator=(const DiskSpaceMonitor &) = delete;

	/* Non-blocking: ask for a fresh value for path. */
	void request(const std::string &path);
	/* Non-blocking: the last value queried for path (valid false if none). */
	DiskSpaceSample latest(const std::string &path) const;
	/* Queries started so far (for tests). */
	uint64_t queries_started() const;

	struct State;

private:
	std::shared_ptr<State> state;
	std::thread worker;
};
