#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/proplist.h>
#include <pulse/stream.h>
#include <pulse/thread-mainloop.h>

#include <util/bmem.h>
#include <util/c99defs.h>
#include <obs.h>

#include "pulseaudio-wrapper.h"

/* obs-rust/obs-studio#158: get_default_id must not leak the server's default
 * sink name when it is empty (no sinks, a headless box, a removed null sink).
 *
 * pulseaudio-wrapper.c is compiled into this test against the fake libpulse
 * below: a context that is always ready and a server whose default sink name
 * the test chooses. */

static const char *fake_default_sink = "";
static int fake_object;

pa_threaded_mainloop *pa_threaded_mainloop_new(void)
{
	return (pa_threaded_mainloop *)&fake_object;
}
int pa_threaded_mainloop_start(pa_threaded_mainloop *m)
{
	UNUSED_PARAMETER(m);
	return 0;
}
void pa_threaded_mainloop_stop(pa_threaded_mainloop *m)
{
	UNUSED_PARAMETER(m);
}
void pa_threaded_mainloop_free(pa_threaded_mainloop *m)
{
	UNUSED_PARAMETER(m);
}
void pa_threaded_mainloop_lock(pa_threaded_mainloop *m)
{
	UNUSED_PARAMETER(m);
}
void pa_threaded_mainloop_unlock(pa_threaded_mainloop *m)
{
	UNUSED_PARAMETER(m);
}
void pa_threaded_mainloop_wait(pa_threaded_mainloop *m)
{
	UNUSED_PARAMETER(m);
}
void pa_threaded_mainloop_signal(pa_threaded_mainloop *m, int wait_for_accept)
{
	UNUSED_PARAMETER(m);
	UNUSED_PARAMETER(wait_for_accept);
}
void pa_threaded_mainloop_accept(pa_threaded_mainloop *m)
{
	UNUSED_PARAMETER(m);
}
pa_mainloop_api *pa_threaded_mainloop_get_api(pa_threaded_mainloop *m)
{
	UNUSED_PARAMETER(m);
	return NULL;
}

pa_proplist *pa_proplist_new(void)
{
	return (pa_proplist *)&fake_object;
}
void pa_proplist_free(pa_proplist *p)
{
	UNUSED_PARAMETER(p);
}
int pa_proplist_sets(pa_proplist *p, const char *key, const char *value)
{
	UNUSED_PARAMETER(p);
	UNUSED_PARAMETER(key);
	UNUSED_PARAMETER(value);
	return 0;
}

pa_context *pa_context_new_with_proplist(pa_mainloop_api *api, const char *name, const pa_proplist *p)
{
	UNUSED_PARAMETER(api);
	UNUSED_PARAMETER(name);
	UNUSED_PARAMETER(p);
	return (pa_context *)&fake_object;
}
void pa_context_set_state_callback(pa_context *c, pa_context_notify_cb_t cb, void *userdata)
{
	UNUSED_PARAMETER(c);
	UNUSED_PARAMETER(cb);
	UNUSED_PARAMETER(userdata);
}
int pa_context_connect(pa_context *c, const char *server, pa_context_flags_t flags, const pa_spawn_api *api)
{
	UNUSED_PARAMETER(c);
	UNUSED_PARAMETER(server);
	UNUSED_PARAMETER(flags);
	UNUSED_PARAMETER(api);
	return 0;
}
void pa_context_disconnect(pa_context *c)
{
	UNUSED_PARAMETER(c);
}
void pa_context_unref(pa_context *c)
{
	UNUSED_PARAMETER(c);
}
pa_context_state_t pa_context_get_state(const pa_context *c)
{
	UNUSED_PARAMETER(c);
	return PA_CONTEXT_READY;
}

pa_operation *pa_context_get_server_info(pa_context *c, pa_server_info_cb_t cb, void *userdata)
{
	pa_server_info info;
	memset(&info, 0, sizeof(info));
	info.default_sink_name = fake_default_sink;
	cb(c, &info, userdata);
	return (pa_operation *)&fake_object;
}
pa_operation_state_t pa_operation_get_state(const pa_operation *o)
{
	UNUSED_PARAMETER(o);
	return PA_OPERATION_DONE;
}
void pa_operation_unref(pa_operation *o)
{
	UNUSED_PARAMETER(o);
}

/* Referenced by the rest of pulseaudio-wrapper.c, never reached here. */
pa_operation *pa_context_get_sink_info_by_name(pa_context *c, const char *name, pa_sink_info_cb_t cb, void *userdata)
{
	UNUSED_PARAMETER(c);
	UNUSED_PARAMETER(name);
	UNUSED_PARAMETER(cb);
	UNUSED_PARAMETER(userdata);
	return NULL;
}
pa_operation *pa_context_get_sink_info_list(pa_context *c, pa_sink_info_cb_t cb, void *userdata)
{
	UNUSED_PARAMETER(c);
	UNUSED_PARAMETER(cb);
	UNUSED_PARAMETER(userdata);
	return NULL;
}
pa_operation *pa_context_get_source_info_by_name(pa_context *c, const char *name, pa_source_info_cb_t cb,
						 void *userdata)
{
	UNUSED_PARAMETER(c);
	UNUSED_PARAMETER(name);
	UNUSED_PARAMETER(cb);
	UNUSED_PARAMETER(userdata);
	return NULL;
}
pa_operation *pa_context_get_source_info_list(pa_context *c, pa_source_info_cb_t cb, void *userdata)
{
	UNUSED_PARAMETER(c);
	UNUSED_PARAMETER(cb);
	UNUSED_PARAMETER(userdata);
	return NULL;
}
void pa_context_set_subscribe_callback(pa_context *c, pa_context_subscribe_cb_t cb, void *userdata)
{
	UNUSED_PARAMETER(c);
	UNUSED_PARAMETER(cb);
	UNUSED_PARAMETER(userdata);
}
pa_operation *pa_context_subscribe(pa_context *c, pa_subscription_mask_t m, pa_context_success_cb_t cb,
				   void *userdata)
{
	UNUSED_PARAMETER(c);
	UNUSED_PARAMETER(m);
	UNUSED_PARAMETER(cb);
	UNUSED_PARAMETER(userdata);
	return NULL;
}
pa_stream *pa_stream_new_with_proplist(pa_context *c, const char *name, const pa_sample_spec *ss,
				       const pa_channel_map *map, pa_proplist *p)
{
	UNUSED_PARAMETER(c);
	UNUSED_PARAMETER(name);
	UNUSED_PARAMETER(ss);
	UNUSED_PARAMETER(map);
	UNUSED_PARAMETER(p);
	return NULL;
}
int pa_stream_connect_playback(pa_stream *s, const char *dev, const pa_buffer_attr *attr, pa_stream_flags_t flags,
			       const pa_cvolume *volume, pa_stream *sync_stream)
{
	UNUSED_PARAMETER(s);
	UNUSED_PARAMETER(dev);
	UNUSED_PARAMETER(attr);
	UNUSED_PARAMETER(flags);
	UNUSED_PARAMETER(volume);
	UNUSED_PARAMETER(sync_stream);
	return -1;
}
void pa_stream_set_write_callback(pa_stream *s, pa_stream_request_cb_t cb, void *userdata)
{
	UNUSED_PARAMETER(s);
	UNUSED_PARAMETER(cb);
	UNUSED_PARAMETER(userdata);
}
void pa_stream_set_underflow_callback(pa_stream *s, pa_stream_notify_cb_t cb, void *userdata)
{
	UNUSED_PARAMETER(s);
	UNUSED_PARAMETER(cb);
	UNUSED_PARAMETER(userdata);
}

static long allocs_after_default_id(const char *server_default_sink, const char *expected_id)
{
	fake_default_sink = server_default_sink;
	long before = bnum_allocs();

	char *id = NULL;
	get_default_id(&id);
	assert_non_null(id);
	assert_string_equal(id, expected_id);
	bfree(id);

	return bnum_allocs() - before;
}

static void named_default_sink_frees_everything(void **state)
{
	UNUSED_PARAMETER(state);
	assert_int_equal(allocs_after_default_id("alsa_output.pci", "alsa_output.pci"), 0);
}

static void empty_default_sink_frees_everything(void **state)
{
	UNUSED_PARAMETER(state);
	assert_int_equal(allocs_after_default_id("", ""), 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(named_default_sink_frees_everything),
		cmocka_unit_test(empty_default_sink_frees_everything),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
