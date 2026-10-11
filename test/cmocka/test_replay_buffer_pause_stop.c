/* Regression test for upstream obsproject/obs-studio#7249 (reported by
 * TianQiBuTian): with the recording and the replay buffer running on shared
 * encoders, pausing the recording made the replay buffer impossible to stop,
 * and OBS hung on exit.
 *
 * Pausing an encoded output pauses its encoders, so every output on those
 * encoders stops receiving packets. The replay buffer only finished stopping
 * when it received a packet at or past its stop timestamp
 * (replay_buffer_data() in obs-ffmpeg-mux.c), which never arrives while the
 * shared encoders are paused. obs_output_stop() never completed and
 * obs_output_release() (the exit path) waited on the output's stopping event
 * forever.
 *
 * The test drives the real replay_buffer output (obs-ffmpeg-mux.c is built
 * into the executable, so the obs-ffmpeg module and its hardware encoder
 * probes are not loaded) next to a pausable "recording" output, both on the
 * same trivial video and audio encoders, with real core video and audio.
 * Each scenario runs in a child process with a watchdog, so a hang is a
 * deterministic failure.
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
#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>

extern struct obs_output_info replay_buffer;

/* obs-ffmpeg-mux.c looks up its UI strings through the module locale. */
const char *obs_module_text(const char *lookup)
{
	return lookup;
}

#define VENC_ID "test_rb_video_encoder"
#define AENC_ID "test_rb_audio_encoder"
#define RECORDING_ID "test_rb_pausable_recording"

/* How long a stop may take. A healthy stop finishes within a few frames. */
#define STOP_TIMEOUT_MS 5000
/* Watchdog for the whole child, in case a stop blocks the calling thread. */
#define CHILD_TIMEOUT_MS 30000

enum result {
	RESULT_OK = 0,
	RESULT_SETUP_FAILED = 1,
	RESULT_STOP_HUNG = 2,
	RESULT_RELEASE_HUNG = 3,
	RESULT_NO_PACKETS = 4,
	RESULT_RESTART_FAILED = 5,
};

/* ------------------------------------------------------------------------- */
/* trivial encoders */

static volatile long video_packets;
static uint8_t packet_byte;

static const char *enc_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return "test encoder";
}

static void *enc_create(obs_data_t *settings, obs_encoder_t *encoder)
{
	UNUSED_PARAMETER(settings);
	return encoder;
}

static void enc_destroy(void *data)
{
	UNUSED_PARAMETER(data);
}

static bool venc_encode(void *data, struct encoder_frame *frame, struct encoder_packet *packet, bool *received)
{
	UNUSED_PARAMETER(data);
	packet->data = &packet_byte;
	packet->size = 1;
	packet->pts = frame->pts;
	packet->dts = frame->pts;
	packet->type = OBS_ENCODER_VIDEO;
	packet->keyframe = true;
	*received = true;
	os_atomic_inc_long(&video_packets);
	return true;
}

static bool aenc_encode(void *data, struct encoder_frame *frame, struct encoder_packet *packet, bool *received)
{
	UNUSED_PARAMETER(data);
	packet->data = &packet_byte;
	packet->size = 1;
	packet->pts = frame->pts;
	packet->dts = frame->pts;
	packet->type = OBS_ENCODER_AUDIO;
	*received = true;
	return true;
}

static size_t aenc_frame_size(void *data)
{
	UNUSED_PARAMETER(data);
	return 1024;
}

static void register_encoders(void)
{
	struct obs_encoder_info venc = {
		.id = VENC_ID,
		.type = OBS_ENCODER_VIDEO,
		.codec = "h264",
		.get_name = enc_name,
		.create = enc_create,
		.destroy = enc_destroy,
		.encode = venc_encode,
	};
	obs_register_encoder(&venc);

	struct obs_encoder_info aenc = {
		.id = AENC_ID,
		.type = OBS_ENCODER_AUDIO,
		.codec = "aac",
		.get_name = enc_name,
		.create = enc_create,
		.destroy = enc_destroy,
		.encode = aenc_encode,
		.get_frame_size = aenc_frame_size,
	};
	obs_register_encoder(&aenc);
}

/* ------------------------------------------------------------------------- */
/* a pausable recording output that shares the replay buffer's encoders */

static const char *rec_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return "test recording";
}

static void *rec_create(obs_data_t *settings, obs_output_t *output)
{
	UNUSED_PARAMETER(settings);
	return output;
}

static void rec_destroy(void *data)
{
	UNUSED_PARAMETER(data);
}

static bool rec_start(void *data)
{
	obs_output_t *output = data;
	if (!obs_output_can_begin_data_capture(output, 0))
		return false;
	if (!obs_output_initialize_encoders(output, 0))
		return false;
	return obs_output_begin_data_capture(output, 0);
}

static void rec_stop(void *data, uint64_t ts)
{
	UNUSED_PARAMETER(ts);
	obs_output_end_data_capture(data);
}

static void rec_packet(void *data, struct encoder_packet *packet)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(packet);
}

static void register_outputs(void)
{
	struct obs_output_info rec = {
		.id = RECORDING_ID,
		.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_CAN_PAUSE,
		.get_name = rec_name,
		.create = rec_create,
		.destroy = rec_destroy,
		.start = rec_start,
		.stop = rec_stop,
		.encoded_packet = rec_packet,
	};
	obs_register_output(&rec);
	obs_register_output(&replay_buffer);
}

/* ------------------------------------------------------------------------- */

#ifndef SCENARIO_UNSUPPORTED

enum scenario {
	SCENARIO_STOP_UNPAUSED,
	SCENARIO_STOP_WHILE_PAUSED,
	SCENARIO_RELEASE_WHILE_PAUSED,
	SCENARIO_RESTART_AFTER_PAUSED_STOP,
};

static bool wait_inactive(obs_output_t *output, int timeout_ms)
{
	for (int i = 0; i < timeout_ms / 10; i++) {
		if (!obs_output_active(output))
			return true;
		os_sleep_ms(10);
	}
	return !obs_output_active(output);
}

static bool wait_video_packets(long count, int timeout_ms)
{
	long target = os_atomic_load_long(&video_packets) + count;
	for (int i = 0; i < timeout_ms / 10; i++) {
		if (os_atomic_load_long(&video_packets) >= target)
			return true;
		os_sleep_ms(10);
	}
	return false;
}

struct release_job {
	obs_output_t *output;
	os_event_t *done;
};

static void *release_thread(void *param)
{
	struct release_job *job = param;
	obs_output_release(job->output);
	os_event_signal(job->done);
	return NULL;
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

	struct obs_audio_info oai = {.samples_per_sec = 48000, .speakers = SPEAKERS_STEREO};
	if (!obs_reset_audio(&oai)) {
		fprintf(stderr, "obs_reset_audio failed\n");
		return false;
	}

	register_encoders();
	register_outputs();
	return true;
}

static enum result run_scenario(enum scenario scenario)
{
	if (!init_core())
		return RESULT_SETUP_FAILED;

	obs_encoder_t *venc = obs_video_encoder_create(VENC_ID, "venc", NULL, NULL);
	obs_encoder_t *aenc = obs_audio_encoder_create(AENC_ID, "aenc", NULL, 0, NULL);
	obs_output_t *rec = obs_output_create(RECORDING_ID, "recording", NULL, NULL);
	obs_output_t *rb = obs_output_create("replay_buffer", "replay buffer", NULL, NULL);
	if (!venc || !aenc || !rec || !rb)
		return RESULT_SETUP_FAILED;

	obs_encoder_set_video(venc, obs_get_video());
	obs_encoder_set_audio(aenc, obs_get_audio());
	obs_output_set_video_encoder(rec, venc);
	obs_output_set_audio_encoder(rec, aenc, 0);
	obs_output_set_video_encoder(rb, venc);
	obs_output_set_audio_encoder(rb, aenc, 0);

	if (!obs_output_start(rec) || !obs_output_start(rb)) {
		fprintf(stderr, "outputs failed to start\n");
		return RESULT_SETUP_FAILED;
	}
	if (!wait_video_packets(30, STOP_TIMEOUT_MS) || !obs_output_get_total_frames(rb))
		return RESULT_NO_PACKETS;

	if (scenario != SCENARIO_STOP_UNPAUSED) {
		if (!obs_output_pause(rec, true)) {
			fprintf(stderr, "recording failed to pause\n");
			return RESULT_SETUP_FAILED;
		}
		/* let the frames queued before the pause drain */
		os_sleep_ms(300);
	}

	if (scenario == SCENARIO_RELEASE_WHILE_PAUSED) {
		/* the exit path: the replay buffer is released while active */
		struct release_job job = {.output = rb};
		pthread_t thread;
		if (os_event_init(&job.done, OS_EVENT_TYPE_MANUAL) != 0 ||
		    pthread_create(&thread, NULL, release_thread, &job) != 0)
			return RESULT_SETUP_FAILED;
		if (os_event_timedwait(job.done, STOP_TIMEOUT_MS) != 0)
			return RESULT_RELEASE_HUNG;
		pthread_join(thread, NULL);
		os_event_destroy(job.done);
		rb = NULL;
	} else {
		obs_output_stop(rb);
		if (!wait_inactive(rb, STOP_TIMEOUT_MS))
			return RESULT_STOP_HUNG;
	}

	if (scenario == SCENARIO_RESTART_AFTER_PAUSED_STOP) {
		/* the replay buffer must come back fully usable */
		obs_output_pause(rec, false);
		if (!obs_output_start(rb))
			return RESULT_RESTART_FAILED;
		if (!wait_video_packets(30, STOP_TIMEOUT_MS))
			return RESULT_NO_PACKETS;
		/* still running and buffering, not finishing a stale stop */
		if (!obs_output_active(rb) || !obs_output_get_total_frames(rb))
			return RESULT_RESTART_FAILED;
		obs_output_stop(rb);
		if (!wait_inactive(rb, STOP_TIMEOUT_MS))
			return RESULT_STOP_HUNG;
	}

	obs_output_stop(rec);
	if (!wait_inactive(rec, STOP_TIMEOUT_MS))
		return RESULT_STOP_HUNG;

	obs_output_release(rb);
	obs_output_release(rec);
	obs_encoder_release(venc);
	obs_encoder_release(aenc);
	obs_shutdown();
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
		/* a hung output cannot be torn down: leave without cleanup */
		_exit((int)run_scenario(scenario));
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
	int r = run_in_child(scenario);
	if (r == RESULT_SETUP_FAILED)
		skip();
	if (r != RESULT_OK) {
		fprintf(stderr, "%s: expected result %d, got %d\n", what, RESULT_OK, r);
		fail();
	}
}
#endif

/* Control: stopping the replay buffer with nothing paused. */
static void stop_unpaused_completes(void **state)
{
	UNUSED_PARAMETER(state);
#ifdef SCENARIO_UNSUPPORTED
	skip();
#else
	expect_ok(SCENARIO_STOP_UNPAUSED, "replay buffer stop, recording running");
#endif
}

static void stop_while_recording_paused_completes(void **state)
{
	UNUSED_PARAMETER(state);
#ifdef SCENARIO_UNSUPPORTED
	skip();
#else
	expect_ok(SCENARIO_STOP_WHILE_PAUSED, "replay buffer stop, recording paused");
#endif
}

static void release_while_recording_paused_completes(void **state)
{
	UNUSED_PARAMETER(state);
#ifdef SCENARIO_UNSUPPORTED
	skip();
#else
	expect_ok(SCENARIO_RELEASE_WHILE_PAUSED, "replay buffer release, recording paused");
#endif
}

static void restart_after_paused_stop_works(void **state)
{
	UNUSED_PARAMETER(state);
#ifdef SCENARIO_UNSUPPORTED
	skip();
#else
	expect_ok(SCENARIO_RESTART_AFTER_PAUSED_STOP, "replay buffer restart after a stop while paused");
#endif
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(stop_unpaused_completes),
		cmocka_unit_test(stop_while_recording_paused_completes),
		cmocka_unit_test(release_while_recording_paused_completes),
		cmocka_unit_test(restart_after_paused_stop_works),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
