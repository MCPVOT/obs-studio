/* Characterization of the service settings, properties and identity
 * functions in libobs/obs-service.c.h (#1913, #1846 unit 8 slice 2):
 * obs_service_get_display_name(), _defaults(), obs_get_service_properties(),
 * obs_service_get_name(), _get_type(), _get_id(), _get_type_data(),
 * _update(), _get_settings(), _properties(), _apply_encoder_settings(). The
 * signal and proc handler getters are not in a public header, so the Rust
 * port tests them. It pins the CURRENT behaviour before
 * the Rust port, through the public API only, so the same test runs on
 * libobs with the C backend, the hybrid libobs and libobs-c-reference. Cases
 * marked "characterized, not endorsed" are odd but real.
 *
 * Two service types are registered: "full" fills every optional callback
 * this slice reaches, "bare" only the required ones (get_name, create,
 * destroy, get_protocol). Each callback records the data pointer it got.
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

#define FULL_ID "test_service_settings_full"
#define BARE_ID "test_service_settings_bare"

struct svc {
	int tag;
};

static int type_data_tag;

static const void *last_data;
static int calls;
static long long modified_bitrate;
static obs_data_t *last_video;
static obs_data_t *last_audio;
static long long updated_bitrate;

static const char *svc_name(void *type_data)
{
	return type_data == &type_data_tag ? "Settings full" : "Settings bare";
}

static void *svc_create(obs_data_t *settings, obs_service_t *service)
{
	(void)settings;
	(void)service;
	struct svc *s = bzalloc(sizeof(*s));
	s->tag = 0x5e7;
	return s;
}

static void svc_destroy(void *data)
{
	bfree(data);
}

static const char *svc_protocol(void *data)
{
	(void)data;
	return "RTMP";
}

static void full_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "bitrate", 2500);
	obs_data_set_default_string(settings, "server", "auto");
}

static bool bitrate_modified(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	(void)props;
	(void)p;
	modified_bitrate = obs_data_get_int(settings, "bitrate");
	return false;
}

static obs_properties_t *full_properties(void *data)
{
	last_data = data;
	calls++;
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p = obs_properties_add_int(props, "bitrate", "Bitrate", 0, 100000, 1);
	obs_property_set_modified_callback(p, bitrate_modified);
	return props;
}

static void full_update(void *data, obs_data_t *settings)
{
	last_data = data;
	calls++;
	updated_bitrate = obs_data_get_int(settings, "bitrate");
}

static void full_apply_encoder(void *data, obs_data_t *video, obs_data_t *audio)
{
	last_data = data;
	calls++;
	last_video = video;
	last_audio = audio;
}

static bool obs_up;
static obs_service_t *full;
static obs_service_t *bare;

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
		.get_protocol = svc_protocol,
		.get_defaults = full_defaults,
		.get_properties = full_properties,
		.update = full_update,
		.apply_encoder_settings = full_apply_encoder,
		.type_data = &type_data_tag,
	};
	obs_register_service(&info);

	struct obs_service_info bare_info = {
		.id = BARE_ID,
		.get_name = svc_name,
		.create = svc_create,
		.destroy = svc_destroy,
		.get_protocol = svc_protocol,
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
	calls = 0;
	modified_bitrate = -1;
	updated_bitrate = -1;
	last_video = last_audio = NULL;
	return 0;
}

#define REQUIRE_OBS()           \
	do {                    \
		if (!obs_up)    \
			skip(); \
	} while (false)

static const void *data_of(obs_service_t *service)
{
	/* The plugin data is not public; the full type's get_properties hands
	 * it to us. */
	obs_properties_t *props = obs_service_properties(service);
	obs_properties_destroy(props);
	return last_data;
}

static void test_display_name(void **state)
{
	(void)state;
	REQUIRE_OBS();
	assert_string_equal(obs_service_get_display_name(FULL_ID), "Settings full");
	assert_string_equal(obs_service_get_display_name(BARE_ID), "Settings bare");
	assert_null(obs_service_get_display_name("test_service_settings_missing"));
}

static void test_defaults_by_id(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_data_t *d = obs_service_defaults(FULL_ID);
	assert_non_null(d);
	assert_int_equal(obs_data_get_int(d, "bitrate"), 2500);
	assert_string_equal(obs_data_get_string(d, "server"), "auto");
	assert_false(obs_data_has_user_value(d, "bitrate"));
	obs_data_release(d);

	/* No get_defaults: an empty object, not NULL. */
	d = obs_service_defaults(BARE_ID);
	assert_non_null(d);
	assert_null(obs_data_first(d));
	obs_data_release(d);

	assert_null(obs_service_defaults("test_service_settings_missing"));
}

static void test_properties_by_id(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_properties_t *props = obs_get_service_properties(FULL_ID);
	assert_non_null(props);
	assert_non_null(obs_properties_get(props, "bitrate"));
	/* Built with NULL data, then the type's defaults applied. */
	assert_null(last_data);
	assert_int_equal(calls, 1);
	assert_int_equal(modified_bitrate, 2500);
	obs_properties_destroy(props);

	assert_null(obs_get_service_properties(BARE_ID));
	assert_null(obs_get_service_properties("test_service_settings_missing"));
}

static void test_identity(void **state)
{
	(void)state;
	REQUIRE_OBS();
	assert_string_equal(obs_service_get_name(full), "full");
	assert_string_equal(obs_service_get_type(full), FULL_ID);
	assert_string_equal(obs_service_get_id(full), FULL_ID);
	assert_ptr_equal(obs_service_get_type_data(full), &type_data_tag);
	assert_null(obs_service_get_type_data(bare));

	assert_null(obs_service_get_name(NULL));
	assert_null(obs_service_get_type(NULL));
	assert_null(obs_service_get_id(NULL));
	assert_null(obs_service_get_type_data(NULL));
}

static void test_properties_on_service(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_properties_t *props = obs_service_properties(full);
	assert_non_null(props);
	assert_non_null(last_data);
	assert_int_equal(calls, 1);
	/* The service's own settings are applied. Characterized, not endorsed:
	 * unlike sources, a service never gets its type's defaults on create,
	 * so "bitrate" reads 0 here, not 2500. */
	assert_int_equal(modified_bitrate, 0);
	obs_properties_destroy(props);

	assert_null(obs_service_properties(bare));
	assert_null(obs_service_properties(NULL));
}

static void test_update_and_settings(void **state)
{
	(void)state;
	REQUIRE_OBS();
	const void *data = data_of(full);
	calls = 0;

	obs_data_t *s = obs_data_create();
	obs_data_set_int(s, "bitrate", 4000);
	obs_service_update(full, s);
	obs_data_release(s);
	assert_int_equal(calls, 1);
	assert_ptr_equal(last_data, data);
	assert_int_equal(updated_bitrate, 4000);

	/* get_settings hands out the service's own object with a new ref. */
	obs_data_t *a = obs_service_get_settings(full);
	obs_data_t *b = obs_service_get_settings(full);
	assert_non_null(a);
	assert_ptr_equal(a, b);
	assert_int_equal(obs_data_get_int(a, "bitrate"), 4000);
	/* No defaults on create (see test_properties_on_service). */
	assert_string_equal(obs_data_get_string(a, "server"), "");
	assert_false(obs_data_has_default_value(a, "server"));
	obs_data_release(a);
	obs_data_release(b);

	/* update merges: a key not in the new settings keeps its value. */
	s = obs_data_create();
	obs_data_set_string(s, "server", "rtmp://example.invalid/live");
	obs_service_update(full, s);
	obs_data_release(s);
	assert_int_equal(updated_bitrate, 4000);

	/* No update callback: the settings still change. */
	s = obs_data_create();
	obs_data_set_int(s, "x", 7);
	obs_service_update(bare, s);
	obs_data_release(s);
	a = obs_service_get_settings(bare);
	assert_int_equal(obs_data_get_int(a, "x"), 7);
	obs_data_release(a);

	obs_service_update(NULL, NULL);
	assert_null(obs_service_get_settings(NULL));
}

static void test_apply_encoder_settings(void **state)
{
	(void)state;
	REQUIRE_OBS();
	const void *data = data_of(full);
	calls = 0;

	obs_data_t *v = obs_data_create();
	obs_data_t *a = obs_data_create();

	obs_service_apply_encoder_settings(full, v, a);
	assert_int_equal(calls, 1);
	assert_ptr_equal(last_data, data);
	assert_ptr_equal(last_video, v);
	assert_ptr_equal(last_audio, a);

	obs_service_apply_encoder_settings(full, v, NULL);
	assert_int_equal(calls, 2);
	assert_ptr_equal(last_video, v);
	assert_null(last_audio);

	obs_service_apply_encoder_settings(full, NULL, a);
	assert_int_equal(calls, 3);
	assert_null(last_video);

	/* Both NULL: the plugin is not called. */
	obs_service_apply_encoder_settings(full, NULL, NULL);
	assert_int_equal(calls, 3);

	obs_service_apply_encoder_settings(bare, v, a);
	obs_service_apply_encoder_settings(NULL, v, a);
	assert_int_equal(calls, 3);

	obs_data_release(v);
	obs_data_release(a);
}

#define CASE(fn) cmocka_unit_test_setup(fn, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		CASE(test_display_name),           CASE(test_defaults_by_id),
		CASE(test_properties_by_id),       CASE(test_identity),
		CASE(test_properties_on_service),  CASE(test_update_and_settings),
		CASE(test_apply_encoder_settings),
	};
	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
