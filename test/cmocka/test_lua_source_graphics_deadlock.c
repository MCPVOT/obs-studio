/* Regression test for upstream obsproject/obs-studio#6674 (reported by
 * kalimag): a Lua script that implements a video source and calls
 * obs_enter_graphics() could deadlock OBS.
 *
 * The graphics thread renders a Lua source with the graphics context held
 * (output_frame()) and then needs the source type's definition_mutex and the
 * script's mutex to call into Lua (obs_lua_source_get_width(),
 * obs_lua_source_video_render() in obs-scripting-lua-source.c). Any other
 * thread that runs the same script holds those locks while the script runs,
 * so a script that calls obs_enter_graphics() there waits for the graphics
 * context while the graphics thread waits for the script: neither ever
 * proceeds. The paths covered here are the ones reported upstream: creating
 * a source (create callback), reloading the script (destroy callback), and a
 * script callback on another thread (script_update, which takes the same
 * script lock as property button callbacks).
 *
 * Each scenario loads a real Lua script through obs-scripting and shows one
 * of its sources on output channel 1, so the graphics thread renders it every
 * frame. A C "gate" source on channel 0 holds the graphics thread inside a
 * frame (graphics context held, Lua source not rendered yet) until the
 * script, run on another thread, reports through the LuaJIT FFI that it holds
 * its locks and is about to call obs_enter_graphics(). Each scenario runs in a
 * child process with a watchdog, so a deadlock is a deterministic failure. A
 * control scenario checks that an uncontended Lua source renders once per
 * output frame.
 *
 * Core video needs a graphics module and libobs needs an X display on Linux,
 * so the tests skip without one (run under xvfb-run). They need fork(), so
 * they skip on Windows and macOS as well.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#define SCENARIO_UNSUPPORTED 1
#endif

#include <obs.h>
#include <obs-scripting.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <util/threading.h>

/* How long a scripted call may take in total. */
#define CALL_TIMEOUT_MS 5000
/* Watchdog for the whole child. */
#define CHILD_TIMEOUT_MS 30000
/* Frames that may skip the Lua source while a script waits for graphics:
 * the one held by the gate, plus slack for the frame in flight. */
#define MAX_SKIPPED_FRAMES 2
/* Length of the uncontended rendering run. */
#define RENDER_RUN_MS 3000

#define SOURCE_ID "test_lua_graphics_source"
#define GATE_ID "test_lua_graphics_gate"

/* The script reports through the FFI, while it holds its locks, that it is
 * about to enter graphics (test_lua_script_locked() below), and counts its
 * video_render calls (test_lua_rendered()). */
static const char *lua_script = "\
obs = obslua\n\
local ffi = require(\"ffi\")\n\
ffi.cdef[[void test_lua_script_locked(void); void test_lua_rendered(void);]]\n\
enter_on_destroy = false\n\
\n\
local function enter_graphics()\n\
	ffi.C.test_lua_script_locked()\n\
	obs.obs_enter_graphics()\n\
	obs.obs_leave_graphics()\n\
end\n\
\n\
obs.obs_register_source({\n\
	id = \"" SOURCE_ID "\",\n\
	type = obs.OBS_SOURCE_TYPE_INPUT,\n\
	output_flags = obs.OBS_SOURCE_VIDEO,\n\
	get_name = function() return \"Lua graphics test\" end,\n\
	create = function(settings, source)\n\
		if obs.obs_data_get_bool(settings, \"enter_on_create\") then\n\
			enter_graphics()\n\
		end\n\
		return {}\n\
	end,\n\
	destroy = function(data)\n\
		if enter_on_destroy then\n\
			enter_graphics()\n\
		end\n\
	end,\n\
	get_width = function(data) return 16 end,\n\
	get_height = function(data) return 16 end,\n\
	video_render = function(data, effect) ffi.C.test_lua_rendered() end,\n\
})\n\
\n\
function script_update(settings)\n\
	enter_on_destroy = obs.obs_data_get_bool(settings, \"enter_on_destroy\")\n\
	if obs.obs_data_get_bool(settings, \"enter_on_update\") then\n\
		enter_graphics()\n\
	end\n\
end\n";

enum result {
	RESULT_OK = 0,
	RESULT_SETUP_FAILED = 1,
	RESULT_SCRIPT_FAILED = 2,
	RESULT_DEADLOCK = 3,
	RESULT_NOT_RENDERING = 4,
	RESULT_FRAMES_MISSED = 5,
};

#ifndef SCENARIO_UNSUPPORTED
enum scenario {
	SCENARIO_CREATE_ENTERS_GRAPHICS,
	SCENARIO_RELOAD_DESTROY_ENTERS_GRAPHICS,
	SCENARIO_SCRIPT_UPDATE_ENTERS_GRAPHICS,
	SCENARIO_RENDER_EVERY_FRAME,
};

static char *script_path;
static obs_script_t *script;
static obs_source_t *shown;

/* ------------------------------------------------------------------------- */
/* A C source rendered on channel 0, before the Lua source on channel 1. When
 * armed, it holds the graphics thread inside the frame (graphics context
 * held) until the script reports that it holds its locks, so the graphics
 * thread then needs the script while the script needs graphics. */

static volatile bool gate_armed;
static os_event_t *gate_entered;
static os_event_t *script_locked;

EXPORT void test_lua_script_locked(void);
void test_lua_script_locked(void)
{
	os_event_signal(script_locked);
}

static const char *gate_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return "gate";
}

static void *gate_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	return source;
}

static void gate_destroy(void *data)
{
	UNUSED_PARAMETER(data);
}

static uint32_t gate_size(void *data)
{
	UNUSED_PARAMETER(data);
	return 16;
}

static void gate_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(effect);

	if (os_atomic_exchange_bool(&gate_armed, false)) {
		os_event_signal(gate_entered);
		os_event_timedwait(script_locked, CALL_TIMEOUT_MS);
	}
}

static void register_gate(void)
{
	struct obs_source_info info = {
		.id = GATE_ID,
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_VIDEO,
		.get_name = gate_name,
		.create = gate_create,
		.destroy = gate_destroy,
		.get_width = gate_size,
		.get_height = gate_size,
		.video_render = gate_render,
	};
	obs_register_source(&info);
}

/* ------------------------------------------------------------------------- */

static void script_log(void *param, obs_script_t *s, int lvl, const char *msg)
{
	UNUSED_PARAMETER(param);
	UNUSED_PARAMETER(s);
	if (lvl <= LOG_WARNING)
		fprintf(stderr, "script: %s\n", msg);
}

static bool init_core(void)
{
	if (!obs_startup("en-US", NULL, NULL))
		return false;

	struct obs_video_info ovi = {
		.graphics_module = GRAPHICS_MODULE_PATH,
		.fps_num = 30,
		.fps_den = 1,
		.base_width = 64,
		.base_height = 64,
		.output_width = 64,
		.output_height = 64,
		.output_format = VIDEO_FORMAT_NV12,
		.colorspace = VIDEO_CS_709,
		.range = VIDEO_RANGE_PARTIAL,
		.adapter = 0,
		.gpu_conversion = false,
		.scale_type = OBS_SCALE_BICUBIC,
	};
	int ret = obs_reset_video(&ovi);
	if (ret != OBS_VIDEO_SUCCESS) {
		fprintf(stderr, "obs_reset_video failed: %d\n", ret);
		return false;
	}

	register_gate();
	return true;
}

static bool load_script(void)
{
	obs_scripting_set_log_callback(script_log, NULL);
	if (!obs_scripting_load()) {
		fprintf(stderr, "obs_scripting_load failed\n");
		return false;
	}
	script = obs_script_create(script_path, NULL);
	if (!script || !obs_script_loaded(script)) {
		fprintf(stderr, "the Lua script did not load\n");
		return false;
	}
	return true;
}

static volatile long lua_renders;

EXPORT void test_lua_rendered(void);
void test_lua_rendered(void)
{
	os_atomic_inc_long(&lua_renders);
}

/* video_render calls of the Lua source */
static long long renders(void)
{
	return os_atomic_load_long(&lua_renders);
}

static bool wait_renders(long long count, int timeout_ms)
{
	long long target = renders() + count;
	for (int i = 0; i < timeout_ms / 10; i++) {
		if (renders() >= target)
			return true;
		os_sleep_ms(10);
	}
	return false;
}

struct call_job {
	enum scenario scenario;
	os_event_t *done;
};

static void *call_thread(void *param)
{
	struct call_job *job = param;

	if (job->scenario == SCENARIO_CREATE_ENTERS_GRAPHICS) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_bool(settings, "enter_on_create", true);
		obs_source_t *source = obs_source_create(SOURCE_ID, "second", settings, NULL);
		obs_data_release(settings);
		obs_source_release(source);
	} else if (job->scenario == SCENARIO_RELOAD_DESTROY_ENTERS_GRAPHICS) {
		obs_script_reload(script);
	} else {
		obs_data_t *settings = obs_data_create();
		obs_data_set_bool(settings, "enter_on_update", true);
		obs_script_update(script, settings);
		obs_data_set_bool(settings, "enter_on_update", false);
		obs_script_update(script, settings);
		obs_data_release(settings);
	}

	os_event_signal(job->done);
	return NULL;
}

static enum result run_scenario(enum scenario scenario)
{
	if (os_event_init(&gate_entered, OS_EVENT_TYPE_MANUAL) != 0 ||
	    os_event_init(&script_locked, OS_EVENT_TYPE_MANUAL) != 0)
		return RESULT_SETUP_FAILED;
	if (!init_core())
		return RESULT_SETUP_FAILED;
	if (!load_script())
		return RESULT_SCRIPT_FAILED;

	obs_source_t *gate = obs_source_create(GATE_ID, "gate", NULL, NULL);
	shown = obs_source_create(SOURCE_ID, "shown", NULL, NULL);
	if (!gate || !shown)
		return RESULT_SCRIPT_FAILED;
	obs_set_output_source(0, gate);
	obs_set_output_source(1, shown);

	if (!wait_renders(10, CALL_TIMEOUT_MS)) {
		fprintf(stderr, "the Lua source is not being rendered\n");
		return RESULT_NOT_RENDERING;
	}

	if (scenario == SCENARIO_RENDER_EVERY_FRAME) {
		uint32_t frames_before = obs_get_total_frames();
		uint32_t lagged_before = obs_get_lagged_frames();
		long long renders_before = renders();
		os_sleep_ms(RENDER_RUN_MS);
		long long rendered = renders() - renders_before;
		long long frames = (long long)(obs_get_total_frames() - frames_before);
		long long lagged = (long long)(obs_get_lagged_frames() - lagged_before);
		fprintf(stderr, "uncontended: %lld renders, %lld frames (%lld lagged), %d expected at 30 fps\n",
			rendered, frames, lagged, RENDER_RUN_MS * 30 / 1000);
		/* one render per output frame, give or take the frame in flight
		 * at either end of the measurement */
		if (rendered < frames - 1 || rendered > frames + 1 || frames < RENDER_RUN_MS * 30 / 1000 * 9 / 10)
			return RESULT_FRAMES_MISSED;
		return RESULT_OK;
	}

	if (scenario == SCENARIO_RELOAD_DESTROY_ENTERS_GRAPHICS) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_bool(settings, "enter_on_destroy", true);
		obs_script_update(script, settings);
		obs_data_release(settings);
	}

	uint32_t frames_before = obs_get_total_frames();
	long long renders_before = renders();

	/* hold the graphics thread inside a frame, then run the script */
	os_atomic_set_bool(&gate_armed, true);
	if (os_event_timedwait(gate_entered, CALL_TIMEOUT_MS) != 0) {
		fprintf(stderr, "the gate source is not being rendered\n");
		return RESULT_NOT_RENDERING;
	}

	struct call_job job = {.scenario = scenario};
	pthread_t thread;
	if (os_event_init(&job.done, OS_EVENT_TYPE_MANUAL) != 0 ||
	    pthread_create(&thread, NULL, call_thread, &job) != 0)
		return RESULT_SETUP_FAILED;
	if (os_event_timedwait(job.done, CALL_TIMEOUT_MS) != 0) {
		fprintf(stderr, "the script call did not return within %d ms\n", CALL_TIMEOUT_MS);
		return RESULT_DEADLOCK;
	}
	pthread_join(thread, NULL);
	os_event_destroy(job.done);

	if (os_event_try(script_locked) != 0) {
		fprintf(stderr, "the script did not run the graphics call\n");
		return RESULT_SCRIPT_FAILED;
	}

	/* the source keeps rendering afterwards */
	if (!wait_renders(10, CALL_TIMEOUT_MS)) {
		fprintf(stderr, "the Lua source stopped rendering\n");
		return RESULT_NOT_RENDERING;
	}

	/* only the frames that met the script in obs_enter_graphics() skip it */
	long long frames = (long long)(obs_get_total_frames() - frames_before);
	long long skipped = frames - (renders() - renders_before);
	fprintf(stderr, "contended: the Lua source skipped %lld of %lld frames\n", skipped, frames);
	if (skipped > MAX_SKIPPED_FRAMES) {
		fprintf(stderr, "the Lua source missed %lld of %lld frames\n", skipped, frames);
		return RESULT_FRAMES_MISSED;
	}

	/* the child exits without tearing down */
	return RESULT_OK;
}

static int run_in_child(enum scenario scenario)
{
	fflush(stderr);
	pid_t pid = fork();
	if (pid == 0) {
		signal(SIGSEGV, SIG_DFL);
		signal(SIGBUS, SIG_DFL);
		signal(SIGILL, SIG_DFL);
		signal(SIGFPE, SIG_DFL);
		signal(SIGABRT, SIG_DFL);
		int r = (int)run_scenario(scenario);
		fflush(stderr);
		_exit(r);
	}
	if (pid < 0) {
		fprintf(stderr, "fork failed\n");
		fail();
	}

	int status = 0;
	pid_t r = 0;
	for (int i = 0; i < CHILD_TIMEOUT_MS / 10; i++) {
		r = waitpid(pid, &status, WNOHANG);
		if (r != 0)
			break;
		os_sleep_ms(10);
	}
	if (r == 0) {
		kill(pid, SIGKILL);
		waitpid(pid, &status, 0);
		fprintf(stderr, "scenario %d did not finish within %d ms\n", (int)scenario, CHILD_TIMEOUT_MS);
		fail();
	}
	if (WIFSIGNALED(status)) {
		fprintf(stderr, "scenario %d crashed with signal %d\n", (int)scenario, WTERMSIG(status));
		fail();
	}
	return WEXITSTATUS(status);
}

static bool have_display(void)
{
	const char *display = getenv("DISPLAY");
	return display && *display;
}

static void expect_ok(enum scenario scenario, const char *what)
{
	if (!have_display())
		skip();

	struct dstr path = {0};
	const char *tmp = getenv("TMPDIR");
	dstr_printf(&path, "%s/test_lua_graphics_%d.lua", tmp && *tmp ? tmp : "/tmp", (int)getpid());
	if (!os_quick_write_utf8_file(path.array, lua_script, strlen(lua_script), false)) {
		fprintf(stderr, "could not write %s\n", path.array);
		dstr_free(&path);
		fail();
	}
	script_path = path.array;

	int r = run_in_child(scenario);
	os_unlink(path.array);
	dstr_free(&path);
	script_path = NULL;

	if (r == RESULT_SETUP_FAILED)
		skip();
	if (r != RESULT_OK) {
		fprintf(stderr, "%s: expected result %d, got %d\n", what, RESULT_OK, r);
		fail();
	}
}
#endif

/* Control: an uncontended Lua source renders once per output frame. */
static void lua_source_renders_every_frame(void **state)
{
	UNUSED_PARAMETER(state);
#ifdef SCENARIO_UNSUPPORTED
	skip();
#else
	expect_ok(SCENARIO_RENDER_EVERY_FRAME, "uncontended Lua source");
#endif
}

/* Adding a source whose Lua create callback enters graphics while another
 * source of the same type is being rendered. */
static void create_entering_graphics_does_not_deadlock(void **state)
{
	UNUSED_PARAMETER(state);
#ifdef SCENARIO_UNSUPPORTED
	skip();
#else
	expect_ok(SCENARIO_CREATE_ENTERS_GRAPHICS, "create callback entering graphics");
#endif
}

/* Reloading the script while its source is rendered, with a destroy callback
 * that enters graphics (the upstream reproduction). */
static void reload_destroy_entering_graphics_does_not_deadlock(void **state)
{
	UNUSED_PARAMETER(state);
#ifdef SCENARIO_UNSUPPORTED
	skip();
#else
	expect_ok(SCENARIO_RELOAD_DESTROY_ENTERS_GRAPHICS, "script reload, destroy callback entering graphics");
#endif
}

/* A script callback on another thread (script lock only, as for property
 * button callbacks) entering graphics while the script's source renders. */
static void script_callback_entering_graphics_does_not_deadlock(void **state)
{
	UNUSED_PARAMETER(state);
#ifdef SCENARIO_UNSUPPORTED
	skip();
#else
	expect_ok(SCENARIO_SCRIPT_UPDATE_ENTERS_GRAPHICS, "script_update entering graphics");
#endif
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(lua_source_renders_every_frame),
		cmocka_unit_test(create_entering_graphics_does_not_deadlock),
		cmocka_unit_test(reload_destroy_entering_graphics_does_not_deadlock),
		cmocka_unit_test(script_callback_entering_graphics_does_not_deadlock),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
