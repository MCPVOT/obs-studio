/* The part of <jack/jack.h> that plugins/linux-jack uses, for tests that run
 * the JACK source without a JACK server or libjack. The test defines every
 * function declared here. */

#pragma once

#include <stdint.h>

typedef uint32_t jack_nframes_t;
typedef uint64_t jack_time_t;
typedef float jack_default_audio_sample_t;

typedef struct _jack_client jack_client_t;
typedef struct _jack_port jack_port_t;

typedef enum JackOptions {
	JackNullOption = 0x00,
	JackNoStartServer = 0x01,
} jack_options_t;

typedef enum JackStatus {
	JackFailure = 0x01,
} jack_status_t;

enum JackPortFlags {
	JackPortIsInput = 0x1,
	JackPortIsOutput = 0x2,
	JackPortIsPhysical = 0x4,
	JackPortCanMonitor = 0x8,
	JackPortIsTerminal = 0x10,
};

#define JACK_DEFAULT_AUDIO_TYPE "32 bit float mono audio"

typedef int (*JackProcessCallback)(jack_nframes_t nframes, void *arg);

jack_client_t *jack_client_open(const char *client_name, jack_options_t options, jack_status_t *status, ...);
int jack_client_close(jack_client_t *client);
jack_port_t *jack_port_register(jack_client_t *client, const char *port_name, const char *port_type,
				unsigned long flags, unsigned long buffer_size);
int jack_set_process_callback(jack_client_t *client, JackProcessCallback process_callback, void *arg);
int jack_activate(jack_client_t *client);
jack_nframes_t jack_get_sample_rate(jack_client_t *client);
void *jack_port_get_buffer(jack_port_t *port, jack_nframes_t nframes);
int jack_get_cycle_times(const jack_client_t *client, jack_nframes_t *current_frames, jack_time_t *current_usecs,
			 jack_time_t *next_usecs, float *period_usecs);
