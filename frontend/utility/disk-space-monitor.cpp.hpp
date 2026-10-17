#include "frontend/build/frontend-utility-disk-space-monitor.pch.hpp"

/* How long the destructor waits for the worker to exit before it detaches it
 * (a query on a hung network path can block for much longer). */
#define DISK_SPACE_MONITOR_JOIN_MS 250

struct DiskSpaceMonitor::State {
	DiskSpaceMonitor::FreeSpaceFn free_space = nullptr;

	mutable std::mutex mutex;
	std::condition_variable cv;

	/* the path of the last finished query and its value */
	std::string path;
	DiskSpaceSample sample;

	/* the query in flight, and the one pending after it */
	bool in_flight = false;
	std::string in_flight_path;
	bool pending = false;
	std::string pending_path;

	uint64_t started = 0;
	bool stop = false;
	bool exited = false;
};

/* The worker holds its own reference to the state, so a worker detached by
 * the destructor while a query hangs never touches freed memory. */
static void disk_space_worker(std::shared_ptr<DiskSpaceMonitor::State> state)
{
	std::unique_lock<std::mutex> lock(state->mutex);

	for (;;) {
		state->cv.wait(lock, [&] { return state->stop || state->pending; });
		if (state->stop) {
			break;
		}

		std::string path = std::move(state->pending_path);
		state->pending = false;
		state->in_flight = true;
		state->in_flight_path = path;
		state->started++;

		lock.unlock();
		uint64_t bytes = state->free_space(path.c_str());
		uint64_t now = os_gettime_ns();
		lock.lock();

		state->in_flight = false;
		state->path = path;
		state->sample.bytes = bytes;
		state->sample.timestamp_ns = now;
		state->sample.valid = true;
	}

	state->exited = true;
	state->cv.notify_all();
}

DiskSpaceMonitor::DiskSpaceMonitor(FreeSpaceFn free_space) : state(std::make_shared<State>())
{
	state->free_space = free_space ? free_space : os_get_free_disk_space;
	worker = std::thread(disk_space_worker, state);
}

DiskSpaceMonitor::~DiskSpaceMonitor()
{
	std::unique_lock<std::mutex> lock(state->mutex);
	state->stop = true;
	state->cv.notify_all();

	bool exited = state->cv.wait_for(lock, std::chrono::milliseconds(DISK_SPACE_MONITOR_JOIN_MS),
					 [&] { return state->exited; });
	lock.unlock();

	if (exited) {
		worker.join();
	} else {
		/* stuck in a hung query: it exits on its own when that returns */
		worker.detach();
	}
}

void DiskSpaceMonitor::request(const std::string &path)
{
	std::lock_guard<std::mutex> lock(state->mutex);

	/* coalesce: one query in flight, one pending slot */
	if (state->in_flight && state->in_flight_path == path) {
		state->pending = false;
		return;
	}
	if (state->pending && state->pending_path == path) {
		return;
	}

	state->pending = true;
	state->pending_path = path;
	state->cv.notify_all();
}

DiskSpaceSample DiskSpaceMonitor::latest(const std::string &path) const
{
	std::lock_guard<std::mutex> lock(state->mutex);
	return path == state->path ? state->sample : DiskSpaceSample{};
}

uint64_t DiskSpaceMonitor::queries_started() const
{
	std::lock_guard<std::mutex> lock(state->mutex);
	return state->started;
}

/* File-local macros (BUILD.md): out of the next file in the unity group. */
#undef DISK_SPACE_MONITOR_JOIN_MS
