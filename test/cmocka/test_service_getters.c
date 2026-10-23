/* Characterization of the service capability and connect-info getters in
 * libobs/obs-service.c.h (#1899, #1846 unit 8): obs_service_get_protocol(),
 * _get_connect_info(), _can_try_to_connect(), _get_preferred_output_type(),
 * _get_max_fps(), _get_max_bitrate(), _get_supported_resolutions() and the
 * supported codec lists. It pins the CURRENT behaviour before the Rust port,
 * through the public API only, so the same test runs on libobs with the C
 * backend, the hybrid libobs and libobs-c-reference. Cases marked
 * "characterized, not endorsed" are odd but real.
 *
 * Two service types are registered: "full" fills every optional callback,
 * "bare" only the required ones (get_name, create, destroy, get_protocol).
 * Each callback records the data pointer it got, so the cases check that the
 * getter passes the service's own data.
 *
 * obs_startup() needs a display on Linux, so the test runs under xvfb-run
 * there. It skips only when no display is set at all; with one set, a failed
 * obs_startup() fails the test. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <obs.h>
#include <util/base.h>
#include <util/bmem.h>

#define FULL_ID "test_service_getters_full"
#define BARE_ID "test_service_getters_bare"

/* What every create() returns; a getter must hand it back to the plugin. */
struct svc {
	int tag;
};

static const void *last_data;
static uint32_t last_type;
static int calls;

static const char *video_codecs[] = {"h264", "hevc", NULL};
static const char *audio_codecs[] = {"aac", NULL};
static struct obs_service_resolution resolutions[] = {{1920, 1080}, {1280, 720}};

static void seen(void *data)
{
	last_data = data;
	calls++;
}

static const char *svc_name(void *type_data)
{
	(void)type_data;
	return "Service getters test";
}

static void *svc_create(obs_data_t *settings, obs_service_t *service)
{
	(void)settings;
	(void)service;
	struct svc *s = bzalloc(sizeof(*s));
	s->tag = 0x5e4c;
	return s;
}

static void svc_destroy(void *data)
{
	bfree(data);
}

static const char *full_protocol(void *data)
{
	seen(data);
	return "SRT";
}

static const char *bare_protocol(void *data)
{
	seen(data);
	return "RTMP";
}

static const char *full_connect_info(void *data, uint32_t type)
{
	seen(data);
	last_type = type;
	return type == OBS_SERVICE_CONNECT_INFO_SERVER_URL ? "srt://example.invalid:9000" : NULL;
}

static bool can_connect_result;
static bool full_can_try(void *data)
{
	seen(data);
	return can_connect_result;
}

static const char *full_output_type(void *data)
{
	seen(data);
	return "ffmpeg_mpegts_muxer";
}

static void full_max_fps(void *data, int *fps)
{
	seen(data);
	*fps = 60;
}

/* Writes only the video bitrate: the getter's zeroing decides the audio. */
static void full_max_bitrate(void *data, int *video, int *audio)
{
	(void)audio;
	seen(data);
	if (video)
		*video = 6000;
}

static void full_resolutions(void *data, struct obs_service_resolution **res, size_t *count)
{
	seen(data);
	*res = bmemdup(resolutions, sizeof(resolutions));
	*count = 2;
}

static const char **full_video_codecs(void *data)
{
	seen(data);
	return video_codecs;
}

static const char **full_audio_codecs(void *data)
{
	seen(data);
	return audio_codecs;
}

static bool obs_up;
static obs_service_t *full;
static obs_service_t *bare;

/* One libobs for the whole group: each case only resets the counters. A
 * display that is set but refuses obs_startup() fails the group instead of
 * skipping it, so a CI run under xvfb-run cannot pass with every case
 * skipped. */
static int group_setup(void **state)
{
	(void)state;
	obs_up = obs_startup("en-US", NULL, NULL);
	if (!obs_up) {
		if (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY")) {
			fprintf(stderr, "obs_startup failed with a display set\n");
			return -1;
		}
		fprintf(stderr, "skipped: obs_startup failed (no display?)\n");
		return 0;
	}

	struct obs_service_info info = {
		.id = FULL_ID,
		.get_name = svc_name,
		.create = svc_create,
		.destroy = svc_destroy,
		.get_protocol = full_protocol,
		.get_connect_info = full_connect_info,
		.can_try_to_connect = full_can_try,
		.get_output_type = full_output_type,
		.get_max_fps = full_max_fps,
		.get_max_bitrate = full_max_bitrate,
		.get_supported_resolutions = full_resolutions,
		.get_supported_video_codecs = full_video_codecs,
		.get_supported_audio_codecs = full_audio_codecs,
	};
	obs_register_service(&info);

	struct obs_service_info bare_info = {
		.id = BARE_ID,
		.get_name = svc_name,
		.create = svc_create,
		.destroy = svc_destroy,
		.get_protocol = bare_protocol,
	};
	obs_register_service(&bare_info);

	full = obs_service_create_private(FULL_ID, "full", NULL);
	bare = obs_service_create_private(BARE_ID, "bare", NULL);
	if (!full || !bare) {
		fprintf(stderr, "obs_service_create_private failed\n");
		return -1;
	}
	return 0;
}

static int group_teardown(void **state)
{
	(void)state;
	if (obs_up) {
		obs_service_release(full);
		obs_service_release(bare);
		obs_shutdown();
	}
	full = bare = NULL;
	obs_up = false;
	return 0;
}

static int setup(void **state)
{
	(void)state;
	last_data = NULL;
	last_type = 0;
	calls = 0;
	can_connect_result = true;
	return 0;
}

#define SKIP_WITHOUT_OBS()          \
	do {                        \
		if (!obs_up)        \
			skip();     \
	} while (0)

static void test_protocol(void **state)
{
	(void)state;
	SKIP_WITHOUT_OBS();
	assert_string_equal(obs_service_get_protocol(full), "SRT");
	assert_ptr_equal(last_data, obs_obj_get_data(full));
	assert_string_equal(obs_service_get_protocol(bare), "RTMP");
	assert_ptr_equal(last_data, obs_obj_get_data(bare));
	assert_null(obs_service_get_protocol(NULL));
	assert_int_equal(calls, 2);
}

static void test_connect_info(void **state)
{
	(void)state;
	SKIP_WITHOUT_OBS();
	assert_string_equal(obs_service_get_connect_info(full, OBS_SERVICE_CONNECT_INFO_SERVER_URL),
			    "srt://example.invalid:9000");
	assert_int_equal(last_type, OBS_SERVICE_CONNECT_INFO_SERVER_URL);
	assert_ptr_equal(last_data, obs_obj_get_data(full));
	/* Any type reaches the plugin unchanged, a value outside the enum too. */
	assert_null(obs_service_get_connect_info(full, 0xfffffff0u));
	assert_int_equal(last_type, 0xfffffff0u);
	assert_null(obs_service_get_connect_info(bare, OBS_SERVICE_CONNECT_INFO_SERVER_URL));
	assert_null(obs_service_get_connect_info(NULL, OBS_SERVICE_CONNECT_INFO_SERVER_URL));
	assert_int_equal(calls, 2);
}

static void test_can_try_to_connect(void **state)
{
	(void)state;
	SKIP_WITHOUT_OBS();
	assert_true(obs_service_can_try_to_connect(full));
	can_connect_result = false;
	assert_false(obs_service_can_try_to_connect(full));
	assert_ptr_equal(last_data, obs_obj_get_data(full));
	/* No callback: the service can always try. */
	assert_true(obs_service_can_try_to_connect(bare));
	assert_false(obs_service_can_try_to_connect(NULL));
	assert_int_equal(calls, 2);
}

static void test_preferred_output_type(void **state)
{
	(void)state;
	SKIP_WITHOUT_OBS();
	assert_string_equal(obs_service_get_preferred_output_type(full), "ffmpeg_mpegts_muxer");
	assert_ptr_equal(last_data, obs_obj_get_data(full));
	assert_null(obs_service_get_preferred_output_type(bare));
	assert_null(obs_service_get_preferred_output_type(NULL));
	assert_int_equal(calls, 1);
}

static void test_max_fps(void **state)
{
	(void)state;
	SKIP_WITHOUT_OBS();
	int fps = 7;
	obs_service_get_max_fps(full, &fps);
	assert_int_equal(fps, 60);
	assert_ptr_equal(last_data, obs_obj_get_data(full));

	fps = 7;
	obs_service_get_max_fps(bare, &fps);
	assert_int_equal(fps, 0);

	/* A null service returns before zeroing: fps is left as it was. */
	fps = 7;
	obs_service_get_max_fps(NULL, &fps);
	assert_int_equal(fps, 7);

	obs_service_get_max_fps(full, NULL);
	assert_int_equal(calls, 1);
}

static void test_max_bitrate(void **state)
{
	(void)state;
	SKIP_WITHOUT_OBS();
	int video = 7, audio = 7;
	obs_service_get_max_bitrate(full, &video, &audio);
	assert_int_equal(video, 6000);
	/* Zeroed by the getter; the plugin left it alone. */
	assert_int_equal(audio, 0);
	assert_ptr_equal(last_data, obs_obj_get_data(full));

	video = audio = 7;
	obs_service_get_max_bitrate(bare, &video, &audio);
	assert_int_equal(video, 0);
	assert_int_equal(audio, 0);

	/* Characterized, not endorsed: unlike max_fps, both are zeroed before
	 * the service is validated, so a null service still writes them. */
	video = audio = 7;
	obs_service_get_max_bitrate(NULL, &video, &audio);
	assert_int_equal(video, 0);
	assert_int_equal(audio, 0);

	/* Either out-parameter may be NULL; the plugin gets the NULL too. */
	video = 7;
	obs_service_get_max_bitrate(full, &video, NULL);
	assert_int_equal(video, 6000);
	obs_service_get_max_bitrate(full, NULL, NULL);
	assert_int_equal(calls, 3);
}

static void test_supported_resolutions(void **state)
{
	(void)state;
	SKIP_WITHOUT_OBS();
	struct obs_service_resolution *res = (void *)1;
	size_t count = 7;
	obs_service_get_supported_resolutions(full, &res, &count);
	assert_int_equal(count, 2);
	assert_non_null(res);
	assert_int_equal(res[0].cx, 1920);
	assert_int_equal(res[1].cy, 720);
	assert_ptr_equal(last_data, obs_obj_get_data(full));
	bfree(res);

	res = (void *)1;
	count = 7;
	obs_service_get_supported_resolutions(bare, &res, &count);
	assert_null(res);
	assert_int_equal(count, 0);

	/* A null service or out-parameter returns before anything is written. */
	res = (void *)1;
	count = 7;
	obs_service_get_supported_resolutions(NULL, &res, &count);
	assert_ptr_equal(res, (void *)1);
	assert_int_equal(count, 7);
	obs_service_get_supported_resolutions(full, NULL, &count);
	assert_int_equal(count, 7);
	obs_service_get_supported_resolutions(full, &res, NULL);
	assert_ptr_equal(res, (void *)1);
	assert_int_equal(calls, 1);
}

static void test_supported_codecs(void **state)
{
	(void)state;
	SKIP_WITHOUT_OBS();
	assert_ptr_equal(obs_service_get_supported_video_codecs(full), video_codecs);
	assert_ptr_equal(last_data, obs_obj_get_data(full));
	assert_ptr_equal(obs_service_get_supported_audio_codecs(full), audio_codecs);
	assert_null(obs_service_get_supported_video_codecs(bare));
	assert_null(obs_service_get_supported_audio_codecs(bare));
	/* Characterized, not endorsed: neither codec getter checks for a null
	 * service, so they are not called with one here. */
	assert_int_equal(calls, 2);
}

#define CASE(fn) cmocka_unit_test_setup(fn, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		CASE(test_protocol),
		CASE(test_connect_info),
		CASE(test_can_try_to_connect),
		CASE(test_preferred_output_type),
		CASE(test_max_fps),
		CASE(test_max_bitrate),
		CASE(test_supported_resolutions),
		CASE(test_supported_codecs),
	};
	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
