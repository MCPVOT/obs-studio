/* Native fixture extension; the public OBS API drives the real WHIP module. */
#ifdef OBS_FIXTURE_WHIP
#include <inttypes.h>
#include <math.h>
#include <pthread.h>

struct whip_tone {
	obs_source_t *source;
	os_event_t *stop;
	pthread_t thread;
};

static void *whip_tone_thread(void *opaque)
{
	struct whip_tone *tone = opaque;
	float samples[2][480];
	for (unsigned i = 0; i < 480; ++i) {
		float wave = sinf(6.28318530718f * (float)(i % 48) / 48.0f);
		samples[0][i] = 0.25f * wave;
		samples[1][i] = -0.125f * wave;
	}
	uint64_t next = os_gettime_ns();
	while (os_event_try(tone->stop) == EAGAIN) {
		struct obs_source_audio audio = {
			.data = {(uint8_t *)samples[0], (uint8_t *)samples[1]},
			.frames = 480,
			.speakers = SPEAKERS_STEREO,
			.format = AUDIO_FORMAT_FLOAT_PLANAR,
			.samples_per_sec = 48000,
			.timestamp = next,
		};
		obs_source_output_audio(tone->source, &audio);
		next += 10000000ULL;
		os_sleepto_ns(next);
	}
	return NULL;
}

static const char *whip_tone_name(void *unused)
{
	(void)unused;
	return "WHIP integration tone";
}

static void *whip_tone_create(obs_data_t *settings, obs_source_t *source)
{
	(void)settings;
	struct whip_tone *tone = bzalloc(sizeof(*tone));
	tone->source = source;
	if (os_event_init(&tone->stop, OS_EVENT_TYPE_MANUAL)) {
		bfree(tone);
		return NULL;
	}
	if (pthread_create(&tone->thread, NULL, whip_tone_thread, tone)) {
		os_event_destroy(tone->stop);
		bfree(tone);
		return NULL;
	}
	return tone;
}

static void whip_tone_destroy(void *opaque)
{
	struct whip_tone *tone = opaque;
	os_event_signal(tone->stop);
	pthread_join(tone->thread, NULL);
	os_event_destroy(tone->stop);
	bfree(tone);
}

struct whip_stop {
	os_event_t *event;
	int code;
};

static void whip_stopped(void *opaque, calldata_t *data)
{
	struct whip_stop *stopped = opaque;
	stopped->code = (int)calldata_int(data, "code");
	os_event_signal(stopped->event);
}

static int stream_whip(const char *endpoint)
{
	struct obs_source_info info = {
		.id = "cc_build_whip_tone",
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_AUDIO,
		.get_name = whip_tone_name,
		.create = whip_tone_create,
		.destroy = whip_tone_destroy,
	};
	obs_register_source(&info);
	obs_source_t *tone = obs_source_create(info.id, "whip tone", NULL, NULL);
	obs_set_output_source(1, tone);
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "server", endpoint);
	obs_data_set_string(settings, "bearer_token", "cc-build-whip-token");
	obs_service_t *service = obs_service_create("whip_custom", "whip service", settings, NULL);
	obs_data_release(settings);
	settings = obs_data_create();
	obs_data_set_string(settings, "rate_control", "CBR");
	obs_data_set_string(settings, "preset", "ultrafast");
	obs_data_set_string(settings, "tune", "zerolatency");
	obs_data_set_int(settings, "bitrate", 500);
	obs_data_set_int(settings, "keyint_sec", 1);
	obs_data_set_int(settings, "bf", 0);
	obs_data_set_bool(settings, "repeat_headers", true);
	obs_encoder_t *video = obs_video_encoder_create("obs_x264", "whip video", settings, NULL);
	obs_data_release(settings);
	settings = obs_data_create();
	obs_data_set_int(settings, "bitrate", 128);
	obs_encoder_t *audio = obs_audio_encoder_create("ffmpeg_opus", "whip audio", settings, 0, NULL);
	obs_data_release(settings);
	obs_output_t *output = obs_output_create("whip_output", "whip stream", NULL, NULL);
	struct whip_stop stopped = {0};
	int status = 0;
	if (!tone || !service || !video || !audio || !output || os_event_init(&stopped.event, OS_EVENT_TYPE_MANUAL)) {
		status = 100;
		goto cleanup;
	}
	signal_handler_connect(obs_output_get_signal_handler(output), "stop", whip_stopped, &stopped);
	obs_encoder_set_video(video, obs_get_video());
	obs_encoder_set_audio(audio, obs_get_audio());
	obs_output_set_service(output, service);
	obs_output_set_video_encoder(output, video);
	obs_output_set_audio_encoder(output, audio, 0);
	if (!obs_output_start(output)) {
		status = 101;
		goto disconnect;
	}
	for (unsigned i = 0;
	     i < 1500 && obs_output_get_total_frames(output) < 90 && os_event_try(stopped.event) == EAGAIN; ++i)
		os_sleep_ms(10);
	uint32_t sent_frames = obs_output_get_total_frames(output);
	uint64_t bytes = obs_output_get_total_bytes(output);
	if (sent_frames < 90 || !bytes)
		status = 102;
	obs_output_stop(output);
	if (os_event_timedwait(stopped.event, 10000)) {
		obs_output_force_stop(output);
		status = 103;
		os_event_timedwait(stopped.event, 10000);
	}
	if (stopped.code != OBS_OUTPUT_SUCCESS)
		status = 104;
	blog(LOG_INFO, "cc::Build WHIP transmitted frames=%u bytes=%" PRIu64 " status=%d", sent_frames, bytes, status);
disconnect:
	signal_handler_disconnect(obs_output_get_signal_handler(output), "stop", whip_stopped, &stopped);
cleanup:
	if (status && output)
		blog(LOG_ERROR, "WHIP fixture failed: %d, %s", status, obs_output_get_last_error(output));
	obs_output_release(output);
	obs_encoder_release(video);
	obs_encoder_release(audio);
	obs_service_release(service);
	if (stopped.event)
		os_event_destroy(stopped.event);
	obs_set_output_source(1, NULL);
	if (tone)
		obs_source_remove(tone);
	obs_source_release(tone);
	while (obs_wait_for_destroy_queue()) {
	}
	return status;
}
#endif
