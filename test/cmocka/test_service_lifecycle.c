/* Characterization of the service lifecycle and references in
 * libobs/obs-service.c.h (#1920, #1846 unit 8 slice 3):
 * obs_service_create(), _create_private(), _release(), _get_ref(),
 * _get_weak_service(), obs_weak_service_get_service(), _addref(),
 * _release(), _references_service(), obs_service_get_module() and
 * obs_service_load_state(). It pins the CURRENT behaviour before the Rust
 * port, through the public API only, so the same test runs on libobs with
 * the C backend, the hybrid libobs and libobs-c-reference. The internal
 * activate/deactivate/initialize calls are covered by the port's Rust tests.
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

#define SVC_ID "test_service_lifecycle"
#define NULL_ID "test_service_lifecycle_null"

struct svc {
	int tag;
};

static int creates;
static int destroys;
static const void *destroyed_data;
static obs_service_t *created_for;

static const char *svc_name(void *type_data)
{
	(void)type_data;
	return "Lifecycle";
}

static void *svc_create(obs_data_t *settings, obs_service_t *service)
{
	(void)settings;
	creates++;
	created_for = service;
	struct svc *s = bzalloc(sizeof(*s));
	s->tag = 0x11fe;
	return s;
}

/* create() fails: the service still exists, with NULL data. */
static void *null_create(obs_data_t *settings, obs_service_t *service)
{
	(void)settings;
	(void)service;
	creates++;
	return NULL;
}

static void svc_destroy(void *data)
{
	destroys++;
	destroyed_data = data;
	bfree(data);
}

static const char *svc_protocol(void *data)
{
	(void)data;
	return "RTMP";
}

static bool obs_up;

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
		.id = SVC_ID,
		.get_name = svc_name,
		.create = svc_create,
		.destroy = svc_destroy,
		.get_protocol = svc_protocol,
	};
	obs_register_service(&info);

	info.id = NULL_ID;
	info.create = null_create;
	obs_register_service(&info);
	return 0;
}

static int group_teardown(void **state)
{
	(void)state;
	if (obs_up)
		obs_shutdown();
	obs_up = false;
	return 0;
}

static int setup(void **state)
{
	(void)state;
	creates = destroys = 0;
	destroyed_data = NULL;
	created_for = NULL;
	return 0;
}

#define REQUIRE_OBS()           \
	do {                    \
		if (!obs_up)    \
			skip(); \
	} while (false)

static void test_create_and_release(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_service_t *s = obs_service_create(SVC_ID, "public one", NULL, NULL);
	assert_non_null(s);
	assert_int_equal(creates, 1);
	assert_ptr_equal(created_for, s);
	assert_string_equal(obs_service_get_name(s), "public one");

	/* A public service is listed by name; it holds no extra ref. */
	obs_service_t *found = obs_get_service_by_name("public one");
	assert_ptr_equal(found, s);
	obs_service_release(found);
	assert_int_equal(destroys, 0);

	obs_service_release(s);
	assert_int_equal(destroys, 1);
	assert_non_null(destroyed_data);
	assert_null(obs_get_service_by_name("public one"));
}

static void test_create_private(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_service_t *s = obs_service_create_private(SVC_ID, "private one", NULL);
	assert_non_null(s);
	assert_int_equal(creates, 1);
	/* Private services are not listed. */
	assert_null(obs_get_service_by_name("private one"));
	obs_service_release(s);
	assert_int_equal(destroys, 1);
}

static void test_create_unknown_id(void **state)
{
	(void)state;
	REQUIRE_OBS();
	assert_null(obs_service_create("test_service_lifecycle_missing", "x", NULL, NULL));
	assert_null(obs_service_create_private("test_service_lifecycle_missing", "x", NULL));
	assert_int_equal(creates, 0);
}

static void test_create_returns_null_data(void **state)
{
	(void)state;
	REQUIRE_OBS();
	/* Characterized, not endorsed: a failed create() still returns a
	 * service; destroy is then not called. */
	obs_service_t *s = obs_service_create_private(NULL_ID, "null data", NULL);
	assert_non_null(s);
	assert_int_equal(creates, 1);
	obs_service_release(s);
	assert_int_equal(destroys, 0);
}

static void test_settings_are_kept(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "bitrate", 1234);
	obs_service_t *s = obs_service_create_private(SVC_ID, "with settings", settings);
	obs_data_t *got = obs_service_get_settings(s);
	assert_int_equal(obs_data_get_int(got, "bitrate"), 1234);
	obs_data_release(got);
	obs_data_release(settings);
	obs_service_release(s);
}

static void test_strong_refs(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_service_t *s = obs_service_create_private(SVC_ID, "refs", NULL);
	obs_service_t *r = obs_service_get_ref(s);
	assert_ptr_equal(r, s);

	obs_service_release(s);
	assert_int_equal(destroys, 0);
	obs_service_release(r);
	assert_int_equal(destroys, 1);

	assert_null(obs_service_get_ref(NULL));
	obs_service_release(NULL);
}

static void test_weak_refs(void **state)
{
	(void)state;
	REQUIRE_OBS();
	obs_service_t *s = obs_service_create_private(SVC_ID, "weak", NULL);
	obs_weak_service_t *w = obs_service_get_weak_service(s);
	assert_non_null(w);
	assert_true(obs_weak_service_references_service(w, s));

	obs_service_t *other = obs_service_create_private(SVC_ID, "other", NULL);
	assert_false(obs_weak_service_references_service(w, other));
	obs_service_release(other);
	destroys = 0;

	obs_service_t *strong = obs_weak_service_get_service(w);
	assert_ptr_equal(strong, s);
	obs_service_release(strong);

	obs_weak_service_addref(w);
	obs_weak_service_release(w);

	/* The weak ref outlives the service and then yields NULL. */
	obs_service_release(s);
	assert_int_equal(destroys, 1);
	assert_null(obs_weak_service_get_service(w));
	obs_weak_service_release(w);

	assert_null(obs_service_get_weak_service(NULL));
	assert_null(obs_weak_service_get_service(NULL));
	assert_false(obs_weak_service_references_service(NULL, NULL));
	obs_weak_service_addref(NULL);
	obs_weak_service_release(NULL);
}

static void test_module_lookup(void **state)
{
	(void)state;
	REQUIRE_OBS();
	/* Registered outside any module: no module, so "missing". */
	assert_null(obs_service_get_module(SVC_ID));
	assert_int_equal(obs_service_load_state(SVC_ID), OBS_MODULE_MISSING);
	assert_null(obs_service_get_module("test_service_lifecycle_missing"));
	assert_int_equal(obs_service_load_state("test_service_lifecycle_missing"), OBS_MODULE_MISSING);
}

#define CASE(fn) cmocka_unit_test_setup(fn, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		CASE(test_create_and_release), CASE(test_create_private),
		CASE(test_create_unknown_id),  CASE(test_create_returns_null_data),
		CASE(test_settings_are_kept),  CASE(test_strong_refs),
		CASE(test_weak_refs),          CASE(test_module_lookup),
	};
	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
