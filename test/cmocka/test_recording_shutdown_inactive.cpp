/* #1391: deactivate is emitted before the output's inactive store. The
 * signal must wake the wait, but cannot itself certify completion. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
extern "C" {
#include <cmocka.h>
}

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>

#include "RecordingShutdownWait.hpp"

struct Result {
	bool stopped;
	bool active;
	bool ui_processed;
	bool checked_after_deactivate;
	qint64 elapsed_ms;
};

/* No sleeps or scheduler-dependent gap: the inactive store is a later UI
 * event, scheduled only after the active predicate observes deactivate.
 * The real libobs signal handler still invokes callbacks on a worker. */
static Result run_wait(bool become_inactive)
{
	auto *handler = signal_handler_create();
	signal_handler_add(handler, "void stop(ptr output, int code)");
	signal_handler_add(handler, "void deactivate(ptr output)");
	std::atomic<bool> deactivated{false};
	bool active = true;
	bool ui_processed = false;
	bool checked_after_deactivate = false;
	QObject receiver;
	std::mutex mutex;
	std::condition_variable condition;
	bool start = false;
	std::thread worker([&]() {
		{
			std::unique_lock<std::mutex> lock(mutex);
			condition.wait(lock, [&]() { return start; });
		}
		uint8_t stack[128];
		calldata_t data;
		calldata_init_fixed(&data, stack, sizeof(stack));
		calldata_set_ptr(&data, "output", nullptr);
		calldata_set_int(&data, "code", 0);
		deactivated = true;
		signal_handler_signal(handler, "deactivate", &data);
	});
	QTimer::singleShot(0, &receiver, [&]() {
		ui_processed = true;
		{
			std::lock_guard<std::mutex> lock(mutex);
			start = true;
		}
		condition.notify_one();
	});
	QElapsedTimer elapsed;
	elapsed.start();
	bool stopped = WaitForOutputToStop(handler, 100, [&]() {
		if (deactivated.load() && !checked_after_deactivate) {
			checked_after_deactivate = true;
			if (become_inactive) {
				QTimer::singleShot(0, &receiver, [&]() { active = false; });
			}
		}
		return active;
	});
	Result result{stopped, active, ui_processed, checked_after_deactivate, elapsed.elapsed()};
	worker.join();
	signal_handler_destroy(handler);
	return result;
}

static void test_deactivate_before_inactive(void **)
{
	const auto result = run_wait(true);
	assert_true(result.stopped);
	assert_false(result.active);
	assert_true(result.ui_processed);
	assert_true(result.checked_after_deactivate);
}

static void test_deactivate_without_inactive_times_out(void **)
{
	const auto result = run_wait(false);
	assert_false(result.stopped);
	assert_true(result.active);
	assert_true(result.ui_processed);
	assert_true(result.checked_after_deactivate);
	assert_true(result.elapsed_ms >= 80);
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	const CMUnitTest tests[] = {cmocka_unit_test(test_deactivate_before_inactive),
				    cmocka_unit_test(test_deactivate_without_inactive_times_out)};
	return cmocka_run_group_tests(tests, nullptr, nullptr);
}
