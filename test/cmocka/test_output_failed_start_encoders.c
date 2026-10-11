/* Regression test for upstream obsproject/obs-studio#10651 (reported by
 * tt2468): when a streaming output fails to start, its encoders stay
 * initialized.
 *
 * Network outputs (rtmp_output, whip_output, ...) initialize their encoders in
 * start(), return true, and connect on a separate thread. If the connection
 * fails, that thread calls obs_output_signal_stop() with an error code. The
 * output never reached obs_output_begin_data_capture(), so libobs takes the
 * "not active" early return in obs_output_end_data_capture() and nothing
 * shuts the encoders down. A hardware encoder keeps its session (an NVENC
 * session, for example) until the next successful start or until OBS exits.
 *
 * The test registers a counting encoder and an output that fails the way
 * rtmp_output fails on a refused connection, and checks that no encoder
 * instance is left after the failed start. Other cases check that an
 * encoder shared with an output that is running, or with one that is still
 * connecting, is left alone; that it is shut down once every output sharing it
 * has failed; and that the encoder still starts normally after a failed start.
 *
 * obs_startup needs a display on Linux, so the tests skip without one (run
 * under xvfb-run). Starting an encoded output reads the core video info
 * through obs_get_video_info(); this executable supplies it, which only works
 * on ELF platforms (see test_texture_encoder_raw_format.c), so the tests skip
 * on Windows and macOS.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>

#include <obs.h>
#include <media-io/video-io.h>
#include <media-io/video-frame.h>
#include <util/platform.h>
#include <util/threading.h>

#if defined(_WIN32) || defined(__APPLE__)
#define SCENARIO_UNSUPPORTED 1
#endif

#define FPS_NUM 30
#define FPS_DEN 1
#define WIDTH 64
#define HEIGHT 64

#define ENCODER_ID "test_counting_encoder"
#define FAILING_OUTPUT_ID "test_connect_failing_output"
#define GOOD_OUTPUT_ID "test_running_output"
#define CONNECTING_OUTPUT_ID "test_connecting_output"

static int obs_up;
static volatile long encoder_creates;
static volatile long encoder_destroys;
static volatile long encode_calls;

static long live_encoders(void)
{
	return os_atomic_load_long(&encoder_creates) - os_atomic_load_long(&encoder_destroys);
}

/* libobs shuts the encoders of a failed start down on a separate thread, so
 * give it a moment */
static long wait_for_live_encoders(long expected)
{
	for (int i = 0; i < 200 && live_encoders() != expected; i++)
		os_sleep_ms(10);
	return live_encoders();
}

/* ------------------------------------------------------------------------- */
/* core video info, see the comment at the top */

bool obs_get_video_info(struct obs_video_info *ovi)
{
	*ovi = (struct obs_video_info){
		.graphics_module = "libobs-opengl",
		.fps_num = FPS_NUM,
		.fps_den = FPS_DEN,
		.base_width = WIDTH,
		.base_height = HEIGHT,
		.output_width = WIDTH,
		.output_height = HEIGHT,
		.output_format = VIDEO_FORMAT_NV12,
		.colorspace = VIDEO_CS_709,
		.range = VIDEO_RANGE_PARTIAL,
		.scale_type = OBS_SCALE_BICUBIC,
	};
	return true;
}

/* ------------------------------------------------------------------------- */
/* counting encoder: every create() is one hardware session */

static const char *enc_name(void *type_data)
{
	(void)type_data;
	return "counting encoder";
}

static void *enc_create(obs_data_t *settings, obs_encoder_t *encoder)
{
	(void)settings;
	os_atomic_inc_long(&encoder_creates);
	return encoder;
}

static void enc_destroy(void *data)
{
	(void)data;
	os_atomic_inc_long(&encoder_destroys);
}

static bool enc_encode(void *data, struct encoder_frame *frame, struct encoder_packet *packet, bool *received_packet)
{
	(void)data;
	(void)frame;
	(void)packet;
	os_atomic_inc_long(&encode_calls);
	*received_packet = false;
	return true;
}

static void register_encoder(void)
{
	struct obs_encoder_info info = {
		.id = ENCODER_ID,
		.type = OBS_ENCODER_VIDEO,
		.codec = "h264",
		.get_name = enc_name,
		.create = enc_create,
		.destroy = enc_destroy,
		.encode = enc_encode,
	};
	obs_register_encoder(&info);
}

/* ------------------------------------------------------------------------- */
/* output that fails like rtmp_output on a refused connection: start()
 * initializes the encoders and returns true, the connect thread then reports
 * the failure with obs_output_signal_stop() */

struct failing_output {
	obs_output_t *output;
	pthread_t thread;
	bool thread_active;
};

static const char *failing_name(void *type_data)
{
	(void)type_data;
	return "connect failing output";
}

static void *failing_create(obs_data_t *settings, obs_output_t *output)
{
	(void)settings;
	struct failing_output *f = bzalloc(sizeof(*f));
	f->output = output;
	return f;
}

static void failing_join(struct failing_output *f)
{
	if (f->thread_active) {
		pthread_join(f->thread, NULL);
		f->thread_active = false;
	}
}

static void failing_destroy(void *data)
{
	struct failing_output *f = data;
	failing_join(f);
	bfree(f);
}

static void *connect_thread(void *data)
{
	struct failing_output *f = data;
	os_sleep_ms(20);
	obs_output_signal_stop(f->output, OBS_OUTPUT_CONNECT_FAILED);
	return NULL;
}

static bool failing_start(void *data)
{
	struct failing_output *f = data;
	failing_join(f);
	if (!obs_output_can_begin_data_capture(f->output, 0))
		return false;
	if (!obs_output_initialize_encoders(f->output, 0))
		return false;
	f->thread_active = pthread_create(&f->thread, NULL, connect_thread, f) == 0;
	return f->thread_active;
}

static void failing_stop(void *data, uint64_t ts)
{
	(void)ts;
	struct failing_output *f = data;
	failing_join(f);
}

static void output_packet(void *data, struct encoder_packet *packet)
{
	(void)data;
	(void)packet;
}

/* ------------------------------------------------------------------------- */
/* output that starts normally */

static const char *good_name(void *type_data)
{
	(void)type_data;
	return "running output";
}

static void *good_create(obs_data_t *settings, obs_output_t *output)
{
	(void)settings;
	return output;
}

static void good_destroy(void *data)
{
	(void)data;
}

static bool good_start(void *data)
{
	obs_output_t *output = data;
	if (!obs_output_can_begin_data_capture(output, 0))
		return false;
	if (!obs_output_initialize_encoders(output, 0))
		return false;
	return obs_output_begin_data_capture(output, 0);
}

static void good_stop(void *data, uint64_t ts)
{
	(void)ts;
	obs_output_end_data_capture(data);
}

/* ------------------------------------------------------------------------- */
/* output that is still connecting: start() initializes the encoders and
 * returns true, and the test decides when the connection succeeds
 * (obs_output_begin_data_capture()) or fails (obs_output_signal_stop()) */

static bool connecting_start(void *data)
{
	obs_output_t *output = data;
	if (!obs_output_can_begin_data_capture(output, 0))
		return false;
	return obs_output_initialize_encoders(output, 0);
}

static void connecting_stop(void *data, uint64_t ts)
{
	(void)ts;
	obs_output_t *output = data;
	if (obs_output_active(output))
		obs_output_end_data_capture(output);
	else
		obs_output_signal_stop(output, OBS_OUTPUT_SUCCESS);
}

static void register_outputs(void)
{
	struct obs_output_info failing = {
		.id = FAILING_OUTPUT_ID,
		.flags = OBS_OUTPUT_VIDEO | OBS_OUTPUT_ENCODED,
		.get_name = failing_name,
		.create = failing_create,
		.destroy = failing_destroy,
		.start = failing_start,
		.stop = failing_stop,
		.encoded_packet = output_packet,
	};
	obs_register_output(&failing);

	struct obs_output_info good = {
		.id = GOOD_OUTPUT_ID,
		.flags = OBS_OUTPUT_VIDEO | OBS_OUTPUT_ENCODED,
		.get_name = good_name,
		.create = good_create,
		.destroy = good_destroy,
		.start = good_start,
		.stop = good_stop,
		.encoded_packet = output_packet,
	};
	obs_register_output(&good);

	struct obs_output_info connecting = {
		.id = CONNECTING_OUTPUT_ID,
		.flags = OBS_OUTPUT_VIDEO | OBS_OUTPUT_ENCODED,
		.get_name = good_name,
		.create = good_create,
		.destroy = good_destroy,
		.start = connecting_start,
		.stop = connecting_stop,
		.encoded_packet = output_packet,
	};
	obs_register_output(&connecting);
}

/* ------------------------------------------------------------------------- */

struct fixture {
	video_t *video;
	obs_encoder_t *encoder;
	os_event_t *stopped;
	int stop_code;
};

static void on_stop(void *data, calldata_t *cd)
{
	struct fixture *fx = data;
	fx->stop_code = (int)calldata_int(cd, "code");
	os_event_signal(fx->stopped);
}

static void push_frames(struct fixture *fx, int count)
{
	uint64_t ts = os_gettime_ns();
	for (int i = 0; i < count; i++) {
		struct video_frame frame;
		ts += 33333333;
		if (video_output_lock_frame(fx->video, &frame, 1, ts))
			video_output_unlock_frame(fx->video);
		os_sleep_ms(40);
	}
}

static int fixture_setup(void **state)
{
#ifdef SCENARIO_UNSUPPORTED
	*state = NULL;
	return 0;
#else
	if (!obs_up) {
		*state = NULL;
		return 0;
	}

	struct fixture *fx = bzalloc(sizeof(*fx));
	struct video_output_info voi = {
		.name = "test video",
		.format = VIDEO_FORMAT_I420,
		.fps_num = FPS_NUM,
		.fps_den = FPS_DEN,
		.width = WIDTH,
		.height = HEIGHT,
		.range = VIDEO_RANGE_PARTIAL,
		.colorspace = VIDEO_CS_709,
		.cache_size = 4,
	};
	if (video_output_open(&fx->video, &voi) != VIDEO_OUTPUT_SUCCESS)
		return -1;
	if (os_event_init(&fx->stopped, OS_EVENT_TYPE_AUTO) != 0)
		return -1;
	fx->encoder = obs_video_encoder_create(ENCODER_ID, "venc", NULL, NULL);
	if (!fx->encoder)
		return -1;
	obs_encoder_set_video(fx->encoder, fx->video);
	*state = fx;
	return 0;
#endif
}

static int fixture_teardown(void **state)
{
	struct fixture *fx = *state;
	if (!fx)
		return 0;
	obs_encoder_release(fx->encoder);
	video_output_close(fx->video);
	os_event_destroy(fx->stopped);
	bfree(fx);
	return 0;
}

static obs_output_t *create_output(struct fixture *fx, const char *id, const char *name)
{
	obs_output_t *out = obs_output_create(id, name, NULL, NULL);
	if (!out) {
		fprintf(stderr, "could not create output %s\n", id);
		fail();
	}
	obs_output_set_video_encoder(out, fx->encoder);
	signal_handler_connect(obs_output_get_signal_handler(out), "stop", on_stop, fx);
	return out;
}

static void release_output(struct fixture *fx, obs_output_t *out)
{
	signal_handler_disconnect(obs_output_get_signal_handler(out), "stop", on_stop, fx);
	obs_output_release(out);
}

/* Start the failing output and wait for its stop signal. */
static void start_and_fail(struct fixture *fx, obs_output_t *out)
{
	if (!obs_output_start(out)) {
		fprintf(stderr, "failing output did not get past start()\n");
		fail();
	}
	if (os_event_timedwait(fx->stopped, 5000) != 0) {
		fprintf(stderr, "failing output never signalled stop\n");
		fail();
	}
	if (fx->stop_code != OBS_OUTPUT_CONNECT_FAILED) {
		fprintf(stderr, "unexpected stop code %d\n", fx->stop_code);
		fail();
	}
	/* the stop signal is emitted before the connect thread returns */
	obs_output_stop(out);
}

static bool skip_scenario(void **state)
{
#ifdef SCENARIO_UNSUPPORTED
	(void)state;
	return true;
#else
	return !obs_up || !*state;
#endif
}

/* The bug: the encoder stays initialized after the failed start. */
static void failed_start_shuts_down_encoders(void **state)
{
	if (skip_scenario(state))
		skip();
	struct fixture *fx = *state;
	long before = live_encoders();

	obs_output_t *out = create_output(fx, FAILING_OUTPUT_ID, "failing");
	start_and_fail(fx, out);

	long after = wait_for_live_encoders(before);
	release_output(fx, out);

	if (after != before) {
		fprintf(stderr,
			"after a failed start the output's encoder was left initialized: "
			"%ld encoder instance(s) before, %ld after\n",
			before, after);
		fail();
	}
}

/* Repeated failed starts (an automatic retry, a user pressing Start Streaming
 * again) must not keep a session open between attempts either. */
static void repeated_failed_starts_leave_nothing(void **state)
{
	if (skip_scenario(state))
		skip();
	struct fixture *fx = *state;
	long before = live_encoders();
	long creates_before = os_atomic_load_long(&encoder_creates);

	obs_output_t *out = create_output(fx, FAILING_OUTPUT_ID, "failing");
	for (int i = 0; i < 5; i++) {
		start_and_fail(fx, out);
		if (wait_for_live_encoders(before) != before) {
			fprintf(stderr, "attempt %d left %ld encoder instance(s) (expected %ld)\n", i + 1,
				live_encoders(), before);
			release_output(fx, out);
			fail();
		}
	}
	release_output(fx, out);

	if (os_atomic_load_long(&encoder_creates) - creates_before != 5) {
		fprintf(stderr, "expected the encoder to be created once per attempt, got %ld\n",
			os_atomic_load_long(&encoder_creates) - creates_before);
		fail();
	}
}

/* Control: after a failed start, the same encoder starts and encodes on an
 * output that works, and a normal stop shuts it down. */
static void encoder_still_starts_after_failed_start(void **state)
{
	if (skip_scenario(state))
		skip();
	struct fixture *fx = *state;
	long before = live_encoders();

	obs_output_t *bad = create_output(fx, FAILING_OUTPUT_ID, "failing");
	start_and_fail(fx, bad);
	release_output(fx, bad);

	obs_output_t *good = create_output(fx, GOOD_OUTPUT_ID, "good");
	long encodes_before = os_atomic_load_long(&encode_calls);
	if (!obs_output_start(good)) {
		fprintf(stderr, "output did not start after a failed start: %s\n", obs_output_get_last_error(good));
		release_output(fx, good);
		fail();
	}
	push_frames(fx, 8);
	long encodes = os_atomic_load_long(&encode_calls) - encodes_before;
	obs_output_stop(good);
	os_event_timedwait(fx->stopped, 5000);
	for (int i = 0; i < 100 && obs_output_active(good); i++)
		os_sleep_ms(10);
	release_output(fx, good);

	if (encodes <= 0) {
		fprintf(stderr, "the encoder did not encode after a failed start\n");
		fail();
	}
	if (live_encoders() != before) {
		fprintf(stderr, "a normal stop left %ld encoder instance(s) (expected %ld)\n", live_encoders(), before);
		fail();
	}
}

/* Control: an encoder shared with an output that is running (a recording
 * that shares the stream encoder) must not be shut down when the stream
 * fails to connect. */
static void shared_running_encoder_is_kept(void **state)
{
	if (skip_scenario(state))
		skip();
	struct fixture *fx = *state;
	long before = live_encoders();

	obs_output_t *good = create_output(fx, GOOD_OUTPUT_ID, "good");
	if (!obs_output_start(good)) {
		fprintf(stderr, "running output did not start: %s\n", obs_output_get_last_error(good));
		release_output(fx, good);
		fail();
	}
	push_frames(fx, 3);

	obs_output_t *bad = create_output(fx, FAILING_OUTPUT_ID, "failing");
	start_and_fail(fx, bad);
	release_output(fx, bad);

	bool still_active = obs_encoder_active(fx->encoder);
	long live_during = live_encoders();
	long encodes_before = os_atomic_load_long(&encode_calls);
	push_frames(fx, 5);
	long encodes = os_atomic_load_long(&encode_calls) - encodes_before;

	obs_output_stop(good);
	os_event_timedwait(fx->stopped, 5000);
	for (int i = 0; i < 100 && obs_output_active(good); i++)
		os_sleep_ms(10);
	release_output(fx, good);

	if (!still_active || live_during != before + 1 || encodes <= 0) {
		fprintf(stderr,
			"the failed start disturbed an encoder in use by another output: "
			"active=%d instances=%ld (expected %ld) encodes=%ld\n",
			still_active, live_during, before + 1, encodes);
		fail();
	}
	if (live_encoders() != before) {
		fprintf(stderr, "stopping the running output left %ld encoder instance(s) (expected %ld)\n",
			live_encoders(), before);
		fail();
	}
}

/* Two outputs share the encoder (multistreaming to two services, for
 * example). The second is still connecting when the first fails to connect:
 * the encoder must stay initialized for the second, which then starts and
 * encodes normally. */
static void connecting_shared_encoder_is_kept(void **state)
{
	if (skip_scenario(state))
		skip();
	struct fixture *fx = *state;
	long before = live_encoders();

	obs_output_t *connecting = create_output(fx, CONNECTING_OUTPUT_ID, "connecting");
	if (!obs_output_start(connecting)) {
		fprintf(stderr, "connecting output did not get past start(): %s\n",
			obs_output_get_last_error(connecting));
		release_output(fx, connecting);
		fail();
	}

	obs_output_t *bad = create_output(fx, FAILING_OUTPUT_ID, "failing");
	start_and_fail(fx, bad);
	release_output(fx, bad);

	long live_after_failure = live_encoders();

	/* the second output connects */
	bool began = obs_output_begin_data_capture(connecting, 0);
	bool encoder_active = obs_encoder_active(fx->encoder);
	long encodes_before = os_atomic_load_long(&encode_calls);
	push_frames(fx, 5);
	long encodes = os_atomic_load_long(&encode_calls) - encodes_before;

	obs_output_stop(connecting);
	os_event_timedwait(fx->stopped, 5000);
	for (int i = 0; i < 100 && obs_output_active(connecting); i++)
		os_sleep_ms(10);
	release_output(fx, connecting);

	if (live_after_failure != before + 1) {
		fprintf(stderr,
			"the failed start shut down an encoder that another output was still starting with: "
			"%ld encoder instance(s), expected %ld\n",
			live_after_failure, before + 1);
		fail();
	}
	if (!began || !encoder_active || encodes <= 0) {
		fprintf(stderr,
			"the output that was still connecting did not get a working encoder: "
			"began=%d active=%d encodes=%ld\n",
			began, encoder_active, encodes);
		fail();
	}
	if (live_encoders() != before) {
		fprintf(stderr, "stopping the second output left %ld encoder instance(s) (expected %ld)\n",
			live_encoders(), before);
		fail();
	}
}

/* Both outputs sharing the encoder fail to connect, one after the other:
 * once the last one has failed, the encoder must be shut down. */
static void shared_encoder_shut_down_after_all_fail(void **state)
{
	if (skip_scenario(state))
		skip();
	struct fixture *fx = *state;
	long before = live_encoders();

	obs_output_t *connecting = create_output(fx, CONNECTING_OUTPUT_ID, "connecting");
	if (!obs_output_start(connecting)) {
		fprintf(stderr, "connecting output did not get past start(): %s\n",
			obs_output_get_last_error(connecting));
		release_output(fx, connecting);
		fail();
	}

	obs_output_t *bad = create_output(fx, FAILING_OUTPUT_ID, "failing");
	start_and_fail(fx, bad);
	release_output(fx, bad);

	/* the second output fails to connect too */
	obs_output_signal_stop(connecting, OBS_OUTPUT_CONNECT_FAILED);
	bool stopped = os_event_timedwait(fx->stopped, 5000) == 0;
	long after = wait_for_live_encoders(before);
	release_output(fx, connecting);

	if (!stopped) {
		fprintf(stderr, "the second output never signalled stop\n");
		fail();
	}
	if (after != before) {
		fprintf(stderr,
			"after both outputs failed to start the shared encoder was left initialized: "
			"%ld encoder instance(s) before, %ld after\n",
			before, after);
		fail();
	}
}

static int group_setup(void **state)
{
	(void)state;
	obs_up = obs_startup("en-US", NULL, NULL);
	if (obs_up) {
		register_encoder();
		register_outputs();
	}
	return 0;
}

static int group_teardown(void **state)
{
	(void)state;
	if (obs_up)
		obs_shutdown();
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(encoder_still_starts_after_failed_start, fixture_setup,
						fixture_teardown),
		cmocka_unit_test_setup_teardown(shared_running_encoder_is_kept, fixture_setup, fixture_teardown),
		cmocka_unit_test_setup_teardown(failed_start_shuts_down_encoders, fixture_setup, fixture_teardown),
		cmocka_unit_test_setup_teardown(repeated_failed_starts_leave_nothing, fixture_setup, fixture_teardown),
		cmocka_unit_test_setup_teardown(connecting_shared_encoder_is_kept, fixture_setup, fixture_teardown),
		cmocka_unit_test_setup_teardown(shared_encoder_shut_down_after_all_fail, fixture_setup,
						fixture_teardown),
	};
	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
