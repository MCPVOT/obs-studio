#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <cmocka.h>
#include <X11/Xlib.h>

#include <obs.h>
#include <obs-nix-platform.h>
#include <util/base.h>

/* ELF interposition injects a deterministic platform-init failure into the
 * real shared libobs, including the Rust-substituted implementation. */
static bool injected_display_failure;

/* The test build uses hidden visibility; this hook must reach the loader. */
EXPORT Display *XOpenDisplay(const char *name)
{
	(void)name;
	injected_display_failure = true;
	errno = ECONNREFUSED;
	return NULL;
}

static bool saw_hotkeys_stage;
static bool saw_display_failure;

static void record_failure(int level, const char *format, va_list args, void *param)
{
	(void)param;
	if (level != LOG_ERROR)
		return;
	char message[512];
	vsnprintf(message, sizeof(message), format, args);
	if (strstr(message, "obs_init_hotkeys"))
		saw_hotkeys_stage = true;
	if (strstr(message, "XOpenDisplay"))
		saw_display_failure = true;
}

static void failed_startup_reports_initializer(void **state)
{
	(void)state;
	log_handler_t previous;
	void *previous_param;
	base_get_log_handler(&previous, &previous_param);
	base_set_log_handler(record_failure, NULL);
	obs_set_nix_platform(OBS_NIX_PLATFORM_X11_EGL);
	bool started = obs_startup("en-US", NULL, NULL);
	base_set_log_handler(previous, previous_param);
	assert_true(injected_display_failure);
	assert_false(started);
	assert_true(saw_hotkeys_stage);
	assert_true(saw_display_failure);
}

int main(void)
{
	const struct CMUnitTest tests[] = {cmocka_unit_test(failed_startup_reports_initializer)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
