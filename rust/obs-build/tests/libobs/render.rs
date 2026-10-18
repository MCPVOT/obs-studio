//! Uses the unchanged C headers for graphics/video structs and callbacks.
use obs_build::{NativeBuild, SharedLibrary};
use std::path::{Path, PathBuf};

pub fn build(build: &NativeBuild, root: &Path, core: &Path) -> PathBuf {
    std::fs::create_dir_all(&build.output).unwrap();
    let source = build.output.join("graphics-probe.c");
    let content = SOURCE.replace("/* WHIP_PROBE */", include_str!("whip-probe.c"));
    if std::fs::read_to_string(&source).ok().as_deref() != Some(&content) {
        std::fs::write(&source, content).unwrap();
    }
    let mut library = SharedLibrary {
        name: "graphics-probe".into(),
        sources: vec![source],
        include_dirs: vec![
            root.into(),
            root.join("libobs"),
            build.output.join("config"),
        ],
        dependencies: vec![core.into()],
        compile_options: vec!["-std=gnu17".into()],
        ..SharedLibrary::default()
    };
    let mut packages = vec!["simde", "x11"];
    if std::env::var_os("OBS_BUILD_WHIP_ENDPOINT").is_some() {
        assert!(std::env::var_os("OBS_BUILD_WEBRTC").is_some());
        assert!(super::enabled("OBS_BUILD_RECORD"));
        library.definitions.push(("OBS_FIXTURE_WHIP".into(), None));
        library.link_options.push("-lm".into());
    }
    if std::env::var_os("OBS_BUILD_JACK_CAPTURE").is_some() {
        packages.push("jack");
        library.definitions.push(("OBS_FIXTURE_JACK".into(), None));
    }
    if std::env::var_os("OBS_BUILD_SNDIO_CAPTURE_DEVICE").is_some() {
        packages.push("sndio");
        library.definitions.push(("OBS_FIXTURE_SNDIO".into(), None));
    }
    for package in packages {
        let dependency = pkg_config::Config::new()
            .cargo_metadata(false)
            .probe(package)
            .unwrap();
        library.include_dirs.extend(dependency.include_paths);
        for path in dependency.link_paths {
            library.link_options.push(format!("-L{}", path.display()));
        }
        library
            .link_options
            .extend(dependency.libs.iter().map(|name| format!("-l{name}")));
    }
    build.shared_library(&library).unwrap()
}

const SOURCE: &str = r#"
#include <X11/Xlib.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <obs.h>
#include <obs-nix-platform.h>
#include <util/platform.h>
#include <util/threading.h>
#ifdef OBS_FIXTURE_JACK
#include <jack/jack.h>
#endif
#ifdef OBS_FIXTURE_SNDIO
#include <sndio.h>
#include <poll.h>
#include <pthread.h>
#endif

static Display *test_display;
static atomic_uint frames;
static atomic_uint invalid_frames;
static atomic_uint expected_y = 16;
static uint32_t fixture_width = 64, fixture_height = 64;

#if defined(OBS_FIXTURE_JACK) || defined(OBS_FIXTURE_SNDIO)
struct capture_probe {
    os_event_t *ready;
    atomic_uint packets;
#ifdef OBS_FIXTURE_JACK
    jack_port_t *ports[2];
#endif
};

static void receive_audio(void *opaque, obs_source_t *source,
                          const struct audio_data *audio, bool muted)
{
    (void)source;
    struct capture_probe *probe = opaque;
    if (muted || !audio->frames || !audio->data[0] || !audio->data[1])
        return;
    const float left = ((const float *)audio->data[0])[0];
    const float right = ((const float *)audio->data[1])[0];
    if (left > 0.24f && left < 0.26f && right < -0.12f && right > -0.13f &&
        atomic_fetch_add(&probe->packets, 1) + 1 >= 3)
        os_event_signal(probe->ready);
}
#endif

#ifdef OBS_FIXTURE_JACK
static int send_jack_audio(jack_nframes_t frames, void *opaque)
{
    struct capture_probe *probe = opaque;
    float *left = jack_port_get_buffer(probe->ports[0], frames);
    float *right = jack_port_get_buffer(probe->ports[1], frames);
    for (jack_nframes_t i = 0; i < frames; i++) {
        left[i] = 0.25f;
        right[i] = -0.125f;
    }
    return 0;
}

EXPORT int obs_fixture_capture_jack(void)
{
    struct capture_probe probe = {0};
    if (os_event_init(&probe.ready, OS_EVENT_TYPE_AUTO))
        return 90;
    obs_source_t *source = obs_source_create_private("jack_output_capture",
        "cc-build-jack-capture", NULL);
    jack_client_t *writer = NULL;
    int status = 91;
    if (!source)
        goto cleanup;
    obs_source_add_audio_capture_callback(source, receive_audio, &probe);
    writer = jack_client_open("cc-build-jack-writer", JackNoStartServer, NULL);
    if (!writer)
        goto cleanup;
    for (unsigned i = 0; i < 2; i++) {
        probe.ports[i] = jack_port_register(writer, i ? "right" : "left",
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (!probe.ports[i])
            goto cleanup;
    }
    if (jack_set_process_callback(writer, send_jack_audio, &probe) || jack_activate(writer))
        goto cleanup;
    if (jack_connect(writer, jack_port_name(probe.ports[0]),
                     "OBS Studio: cc-build-jack-capture:in_1") ||
        jack_connect(writer, jack_port_name(probe.ports[1]),
                     "OBS Studio: cc-build-jack-capture:in_2"))
        goto cleanup;
    status = os_event_timedwait(probe.ready, 5000) ? 92 : 0;
    if (!status)
        blog(LOG_INFO, "cc::Build JACK capture: %u packets with distinct stereo samples",
             atomic_load(&probe.packets));
cleanup:
    if (writer)
        jack_client_close(writer);
    if (source) {
        obs_source_remove_audio_capture_callback(source, receive_audio, &probe);
        obs_source_release(source);
        obs_wait_for_destroy_queue();
    }
    os_event_destroy(probe.ready);
    return status;
}
#endif

#ifdef OBS_FIXTURE_SNDIO
struct sndio_writer {
    struct sio_hdl *handle;
    atomic_bool stop;
};

static void *send_sndio_audio(void *opaque)
{
    struct sndio_writer *writer = opaque;
    int16_t samples[512 * 2];
    for (size_t i = 0; i < 512; i++) {
        samples[2 * i] = 8192;
        samples[2 * i + 1] = -4096;
    }
    unsigned nfds = sio_nfds(writer->handle);
    struct pollfd *fds = calloc(nfds, sizeof(*fds));
    size_t offset = 0;
    while (fds && !atomic_load(&writer->stop)) {
        int count = sio_pollfd(writer->handle, fds, POLLOUT);
        if (poll(fds, count, 100) < 0)
            break;
        int events = sio_revents(writer->handle, fds);
        if (events & POLLHUP)
            break;
        if (events & POLLOUT) {
            size_t written = sio_write(writer->handle,
                (const uint8_t *)samples + offset, sizeof(samples) - offset);
            offset = (offset + written) % sizeof(samples);
        }
    }
    free(fds);
    return NULL;
}

EXPORT int obs_fixture_capture_sndio(const char *device)
{
    struct capture_probe probe = {0};
    struct sndio_writer writer = {0};
    pthread_t thread;
    bool started = false;
    if (os_event_init(&probe.ready, OS_EVENT_TYPE_AUTO))
        return 93;
    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, "device", device);
    obs_source_t *source = obs_source_create_private("sndio_output_capture",
        "cc-build-sndio-capture", settings);
    obs_data_release(settings);
    int status = 94;
    if (!source)
        goto cleanup;
    obs_source_add_audio_capture_callback(source, receive_audio, &probe);
    writer.handle = sio_open(device, SIO_PLAY, 1);
    if (!writer.handle)
        goto cleanup;
    struct sio_par parameters;
    sio_initpar(&parameters);
    parameters.bits = 16;
    parameters.bps = 2;
    parameters.sig = 1;
    parameters.le = 1;
    parameters.rate = 48000;
    parameters.pchan = 2;
    if (!sio_setpar(writer.handle, &parameters) || !sio_start(writer.handle))
        goto cleanup;
    if (pthread_create(&thread, NULL, send_sndio_audio, &writer))
        goto cleanup;
    started = true;
    status = os_event_timedwait(probe.ready, 5000) ? 95 : 0;
    if (!status)
        blog(LOG_INFO, "cc::Build sndio capture: %u packets with distinct stereo samples",
             atomic_load(&probe.packets));
cleanup:
    if (started) {
        atomic_store(&writer.stop, true);
        pthread_join(thread, NULL);
    }
    if (writer.handle)
        sio_close(writer.handle);
    if (source) {
        obs_source_remove_audio_capture_callback(source, receive_audio, &probe);
        obs_source_release(source);
        obs_wait_for_destroy_queue();
    }
    os_event_destroy(probe.ready);
    return status;
}
#endif

EXPORT bool obs_fixture_open_display(void)
{
    XInitThreads();
    test_display = XOpenDisplay(NULL);
    if (!test_display)
        return false;
    obs_set_nix_platform(OBS_NIX_PLATFORM_X11_EGL);
    obs_set_nix_platform_display(test_display);
    return true;
}

static void receive_frame(void *unused, struct video_data *frame)
{
    (void)unused;
    if (frame->data[0] && atomic_load(&expected_y) == 235 &&
        atomic_load(&frames) == 0 && frame->data[0][0] == 16)
        return; /* Drain black frames already in the video pipeline. */
    if (!frame->data[0] || frame->linesize[0] < fixture_width || frame->data[0][0] != atomic_load(&expected_y))
        atomic_fetch_add(&invalid_frames, 1);
    atomic_fetch_add(&frames, 1);
}

/* WHIP_PROBE */

static int record_clip(const char *path, const char *output_id)
{
    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, "rate_control", "CBR");
    obs_data_set_int(settings, "bitrate", 500);
    bool nvenc = getenv("OBS_BUILD_NVENC_ENCODE") != NULL;
    if (!nvenc) {
        obs_data_set_string(settings, "preset", "ultrafast");
        obs_data_set_string(settings, "tune", "zerolatency");
    }
    obs_encoder_t *video = obs_video_encoder_create(nvenc ? "obs_nvenc_h264_soft" : "obs_x264",
        "cc-build-video", settings, NULL);
    obs_data_release(settings);
    settings = obs_data_create();
    obs_data_set_int(settings, "bitrate", 128);
    const char *audio_id = getenv("OBS_BUILD_FDK") ? "libfdk_aac" : "ffmpeg_aac";
    obs_encoder_t *audio = obs_audio_encoder_create(audio_id, "cc-build-audio", settings, 0, NULL);
    if (audio)
        blog(LOG_INFO, "cc::Build audio encoder: %s", obs_encoder_get_id(audio));
    obs_data_release(settings);
    settings = obs_data_create();
    obs_data_set_string(settings, "path", path);
    obs_output_t *output = obs_output_create(output_id, "cc-build-recording", settings, NULL);
    obs_data_release(settings);
    int status = 0;
    if (!video || !audio || !output) {
        status = 85;
        goto cleanup;
    }
    obs_encoder_set_video(video, obs_get_video());
    obs_encoder_set_audio(audio, obs_get_audio());
    obs_output_set_video_encoder(output, video);
    obs_output_set_audio_encoder(output, audio, 0);
    if (!obs_output_start(output)) {
        blog(LOG_ERROR, "cc::Build recording start failed: %s", obs_output_get_last_error(output));
        status = 86;
        goto cleanup;
    }
    for (unsigned i = 0; i < 400 && obs_output_active(output) && obs_output_get_total_frames(output) < 15; ++i)
        os_sleep_ms(10);
    if (obs_output_get_total_frames(output) < 15)
        status = 87;
    obs_output_stop(output);
    for (unsigned i = 0; i < 500 && obs_output_active(output); ++i)
        os_sleep_ms(10);
    if (obs_output_active(output)) {
        obs_output_force_stop(output);
        status = 88;
    }
    blog(LOG_INFO, "cc::Build recording: %s (status %d)", path, status);
cleanup:
    obs_output_release(output);
    obs_encoder_release(video);
    obs_encoder_release(audio);
    return status;
}

EXPORT int obs_fixture_initialize(const char *renderer, const char *data_path, bool audio_enabled)
{
    // Hardware encoders have minimum dimensions above the tiny software probe.
    if (getenv("OBS_BUILD_NVENC_ENCODE")) {
        fixture_width = 640;
        fixture_height = 360;
    }
    obs_add_data_path(data_path);
    if (audio_enabled) {
        struct obs_audio_info audio = { .samples_per_sec = 48000, .speakers = SPEAKERS_STEREO };
        if (!obs_reset_audio(&audio))
            return 84;
    }
    struct obs_video_info video = {
        .graphics_module = renderer,
        .fps_num = 30, .fps_den = 1,
        .base_width = fixture_width, .base_height = fixture_height,
        .output_width = fixture_width, .output_height = fixture_height,
        .output_format = VIDEO_FORMAT_NV12,
        .gpu_conversion = true,
        .colorspace = VIDEO_CS_709,
        .range = VIDEO_RANGE_PARTIAL,
        .scale_type = OBS_SCALE_BICUBIC,
    };
    int result = obs_reset_video(&video);
    return result;
}

EXPORT int obs_fixture_render(const char *record_path, const char *output_id)
{
    obs_enter_graphics();
    blog(LOG_INFO, "cc::Build graphics device: %s", gs_get_device_name());
    obs_leave_graphics();
    obs_scene_t *scene = obs_scene_create("cc-build-rendering");
    if (!scene)
        return 81;
    obs_set_output_source(0, obs_scene_get_source(scene));
    atomic_store(&frames, 0);
    atomic_store(&invalid_frames, 0);
    obs_add_raw_video_callback(NULL, receive_frame, NULL);
    for (unsigned i = 0; i < 200 && atomic_load(&frames) < 3; ++i)
        os_sleep_ms(10);
    obs_remove_raw_video_callback(receive_frame, NULL);
    int source_status = 0;
    obs_source_t *color = NULL;
    if (getenv("OBS_BUILD_SOURCES") || getenv("OBS_BUILD_APP_GUI")) {
        obs_data_t *settings = obs_data_create();
        obs_data_set_int(settings, "width", fixture_width);
        obs_data_set_int(settings, "height", fixture_height);
        obs_data_set_int(settings, "color", 0xFFFFFFFF);
        color = obs_source_create("color_source_v3", "cc-build-white", settings, NULL);
        obs_data_release(settings);
        if (!color || obs_source_get_width(color) != fixture_width || obs_source_get_height(color) != fixture_height) {
            source_status = 89;
        } else {
            obs_scene_add(scene, color);
            atomic_store(&frames, 0);
            atomic_store(&expected_y, 235);
            obs_add_raw_video_callback(NULL, receive_frame, NULL);
            for (unsigned i = 0; i < 200 && atomic_load(&frames) < 3; ++i)
                os_sleep_ms(10);
            obs_remove_raw_video_callback(receive_frame, NULL);
            blog(LOG_INFO, "cc::Build color source rendered %u white frames", atomic_load(&frames));
        }
    }
    int streaming_status = 0;
#ifdef OBS_FIXTURE_WHIP
    if (getenv("OBS_BUILD_WHIP_ENDPOINT"))
        streaming_status = stream_whip(getenv("OBS_BUILD_WHIP_ENDPOINT"));
#endif
    int recording_status = record_path ? record_clip(record_path, output_id) : 0;
    obs_set_output_source(0, NULL);
    if (color) {
        obs_source_remove(color);
        obs_source_release(color);
    }
    // Public sources also belong to the main canvas until explicitly removed.
    obs_source_remove(obs_scene_get_source(scene));
    obs_scene_release(scene);
    // Match frontend scene teardown: queued scene destruction can release its
    // child sources. Finish it before obs_shutdown force-frees remaining ones.
    while (obs_wait_for_destroy_queue()) {}
    /* The owning test calls obs_shutdown after this probe. */
    if (atomic_load(&frames) < 3)
        return 82;
    if (atomic_load(&invalid_frames))
        return 83;
    return source_status ? source_status : (streaming_status ? streaming_status : recording_status);
}

EXPORT void obs_fixture_close_display(void)
{
    obs_set_nix_platform_display(NULL);
    XCloseDisplay(test_display);
    test_display = NULL;
}
"#;
