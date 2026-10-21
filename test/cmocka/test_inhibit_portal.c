/* Characterizes the original platform-nix-portal.c.h before its Rust port.
 * Real GLib variants, cancellables and reference counts; deterministic D-Bus
 * callbacks, with no desktop bus, threads or sleeps. The entry points are
 * hidden, so this includes C and cannot test a Rust swap. The unchanged
 * test_inhibit_sleep_dbus provides public-API OFF/ON integration. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "build/libobs-nix-dbus.pch.h"

struct pending_call {
	GAsyncReadyCallback callback;
	gpointer data;
	GCancellable *cancel;
};

static struct {
	bool bus_missing;
	bool portal_missing;
	GDBusConnection *connection;
	GDBusSignalCallback response;
	gpointer response_data;
	GDestroyNotify response_destroy;
	struct pending_call calls[8];
	size_t pending;
	unsigned inhibits;
	unsigned closes;
	unsigned unsubscribes;
	char *path;
	GVariant *arguments;
	int finish_error;
	bool failed;
} transport;

static GDBusConnection *fake_bus_get(GBusType type, GCancellable *cancel, GError **error)
{
	assert_int_equal(type, G_BUS_TYPE_SESSION);
	assert_null(cancel);
	if (transport.bus_missing) {
		g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "characterization: no session bus");
		return NULL;
	}
	/* A real connection object, without opening any transport. */
	transport.connection = g_object_new(G_TYPE_DBUS_CONNECTION, NULL);
	return transport.connection;
}

static const gchar *fake_unique_name(GDBusConnection *connection)
{
	assert_ptr_equal(connection, transport.connection);
	return ":1.42";
}

static GVariant *fake_call_sync(GDBusConnection *connection, const gchar *name, const gchar *path,
				const gchar *interface, const gchar *method, GVariant *parameters,
				const GVariantType *reply_type, GDBusCallFlags flags, gint timeout,
				GCancellable *cancel, GError **error)
{
	(void)error;
	assert_ptr_equal(connection, transport.connection);
	assert_string_equal(name, "org.freedesktop.portal.Desktop");
	assert_string_equal(path, "/org/freedesktop/portal/desktop");
	assert_string_equal(interface, "org.freedesktop.DBus.Properties");
	assert_string_equal(method, "Get");
	assert_true(g_variant_type_equal(reply_type, G_VARIANT_TYPE("(v)")));
	assert_int_equal(flags, G_DBUS_CALL_FLAGS_NONE);
	assert_int_equal(timeout, -1);
	assert_null(cancel);
	const char *requested_interface, *property;
	g_variant_get(parameters, "(&s&s)", &requested_interface, &property);
	assert_string_equal(requested_interface, "org.freedesktop.portal.Inhibit");
	assert_string_equal(property, "version");
	g_variant_unref(g_variant_ref_sink(parameters));
	return transport.portal_missing ? NULL : g_variant_ref_sink(g_variant_new("(v)", g_variant_new_uint32(3)));
}

static guint fake_subscribe(GDBusConnection *connection, const gchar *sender, const gchar *interface,
			    const gchar *member, const gchar *path, const gchar *arg0, GDBusSignalFlags flags,
			    GDBusSignalCallback callback, gpointer data, GDestroyNotify destroy)
{
	assert_ptr_equal(connection, transport.connection);
	assert_string_equal(sender, "org.freedesktop.portal.Desktop");
	assert_string_equal(interface, "org.freedesktop.portal.Request");
	assert_string_equal(member, "Response");
	assert_null(arg0);
	assert_int_equal(flags, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE);
	assert_null(transport.response);
	transport.response = callback;
	transport.response_data = data;
	transport.response_destroy = destroy;
	g_free(transport.path);
	transport.path = g_strdup(path);
	return 17;
}

static void fake_unsubscribe(GDBusConnection *connection, guint id)
{
	assert_ptr_equal(connection, transport.connection);
	assert_int_equal(id, 17);
	assert_non_null(transport.response);
	transport.unsubscribes++;
	transport.response = NULL;
	transport.response_destroy(transport.response_data);
	transport.response_data = NULL;
}

static void fake_call(GDBusConnection *connection, const gchar *name, const gchar *path, const gchar *interface,
		      const gchar *method, GVariant *parameters, const GVariantType *reply_type, GDBusCallFlags flags,
		      gint timeout, GCancellable *cancel, GAsyncReadyCallback callback, gpointer data)
{
	assert_ptr_equal(connection, transport.connection);
	assert_string_equal(name, "org.freedesktop.portal.Desktop");
	assert_int_equal(flags, G_DBUS_CALL_FLAGS_NONE);
	assert_int_equal(timeout, -1);
	g_variant_ref_sink(parameters);
	if (strcmp(method, "Close") == 0) {
		assert_string_equal(interface, "org.freedesktop.portal.Request");
		assert_string_equal(path, transport.path);
		assert_true(g_variant_type_equal(reply_type, G_VARIANT_TYPE_UNIT));
		assert_true(g_variant_is_of_type(parameters, G_VARIANT_TYPE_UNIT));
		assert_null(cancel);
		transport.closes++;
	} else {
		assert_string_equal(method, "Inhibit");
		assert_string_equal(interface, "org.freedesktop.portal.Inhibit");
		assert_string_equal(path, "/org/freedesktop/portal/desktop");
		assert_null(reply_type);
		assert_non_null(cancel);
		transport.inhibits++;
		g_clear_pointer(&transport.arguments, g_variant_unref);
		transport.arguments = g_variant_ref(parameters);
	}
	g_variant_unref(parameters);
	assert_true(transport.pending < G_N_ELEMENTS(transport.calls));
	transport.calls[transport.pending++] =
		(struct pending_call){callback, data, cancel ? g_object_ref(cancel) : NULL};
}

static GVariant *fake_finish(GDBusConnection *connection, GAsyncResult *result, GError **error)
{
	(void)connection;
	(void)result;
	if (transport.failed) {
		g_set_error_literal(error, G_IO_ERROR, transport.finish_error, "characterization: method failed");
		return NULL;
	}
	return g_variant_ref_sink(g_variant_new("()"));
}

#define g_bus_get_sync fake_bus_get
#define g_dbus_connection_get_unique_name fake_unique_name
#define g_dbus_connection_call_sync fake_call_sync
#define g_dbus_connection_signal_subscribe fake_subscribe
#define g_dbus_connection_signal_unsubscribe fake_unsubscribe
#define g_dbus_connection_call fake_call
#define g_dbus_connection_call_finish fake_finish
#include "util/platform-nix-portal.c.h"
#undef g_bus_get_sync
#undef g_dbus_connection_get_unique_name
#undef g_dbus_connection_call_sync
#undef g_dbus_connection_signal_subscribe
#undef g_dbus_connection_signal_unsubscribe
#undef g_dbus_connection_call
#undef g_dbus_connection_call_finish

static void complete_call(bool error)
{
	assert_true(transport.pending > 0);
	struct pending_call call = transport.calls[0];
	memmove(transport.calls, transport.calls + 1, --transport.pending * sizeof(call));
	bool cancelled = call.cancel && g_cancellable_is_cancelled(call.cancel);
	transport.finish_error = cancelled ? G_IO_ERROR_CANCELLED : G_IO_ERROR_FAILED;
	transport.failed = error || cancelled;
	call.callback(NULL, NULL, call.data);
	g_clear_object(&call.cancel);
}

static void respond(unsigned code)
{
	assert_non_null(transport.response);
	GVariantBuilder results;
	g_variant_builder_init(&results, G_VARIANT_TYPE_VARDICT);
	GVariant *parameters = g_variant_ref_sink(g_variant_new("(ua{sv})", code, &results));
	transport.response(NULL, NULL, NULL, NULL, NULL, parameters, transport.response_data);
	g_variant_unref(parameters);
}

static long allocations;
static int setup(void **state)
{
	(void)state;
	memset(&transport, 0, sizeof(transport));
	allocations = bnum_allocs();
	return 0;
}

static int teardown(void **state)
{
	(void)state;
	assert_int_equal(transport.pending, 0);
	assert_null(transport.response);
	g_free(transport.path);
	g_clear_pointer(&transport.arguments, g_variant_unref);
	assert_int_equal(bnum_allocs(), allocations);
	return 0;
}

static void test_discovery(void **state)
{
	(void)state;
	portal_inhibit_info_destroy(NULL);
	transport.bus_missing = true;
	assert_null(portal_inhibit_info_create());
	transport.bus_missing = false;
	transport.portal_missing = true;
	assert_null(portal_inhibit_info_create());
	transport.portal_missing = false;
	struct portal_inhibit_info *info = portal_inhibit_info_create();
	assert_non_null(info);
	assert_string_equal(info->sender_name, "1_42");
	assert_false(info->active);
	portal_inhibit(info, "unused", false);
	assert_int_equal(transport.pending, 0);
	portal_inhibit_info_destroy(info);
}

static void test_request_and_success(void **state)
{
	(void)state;
	struct portal_inhibit_info *info = portal_inhibit_info_create();
	portal_inhibit(info, "recording ☃", true);
	assert_true(info->active);
	const char *window, *reason, *token;
	guint flags;
	GVariant *options;
	g_variant_get(transport.arguments, "(&su@a{sv})", &window, &flags, &options);
	assert_string_equal(window, "");
	assert_int_equal(flags, 0xC);
	assert_true(g_variant_lookup(options, "reason", "&s", &reason));
	assert_string_equal(reason, "recording ☃");
	assert_true(g_variant_lookup(options, "handle_token", "&s", &token));
	assert_true(g_str_has_prefix(token, "obs_inhibit_portal"));
	g_autofree char *expected = g_strdup_printf("/org/freedesktop/portal/desktop/request/1_42/%s", token);
	assert_string_equal(transport.path, expected);
	g_variant_unref(options);
	portal_inhibit(info, "ignored replacement reason", true);
	assert_int_equal(transport.inhibits, 1);
	complete_call(false);
	assert_null(info->cancellable);
	respond(0);
	assert_true(info->active);
	assert_int_equal(transport.unsubscribes, 1);
	portal_inhibit(info, "unused", false);
	assert_false(info->active);
	assert_null(info->request_path);
	assert_int_equal(transport.closes, 1);
	portal_inhibit(info, "unused", false);
	assert_int_equal(transport.closes, 1);
	complete_call(false);
	portal_inhibit_info_destroy(info);
}

static void test_rejected_responses(void **state)
{
	(void)state;
	const unsigned codes[] = {1, 2, 99};
	for (size_t i = 0; i < G_N_ELEMENTS(codes); i++) {
		struct portal_inhibit_info *info = portal_inhibit_info_create();
		portal_inhibit(info, "", true);
		complete_call(false);
		respond(codes[i]);
		assert_false(info->active);
		assert_null(info->request_path);
		assert_int_equal(info->signal_id, 0);
		portal_inhibit(info, "unused", false);
		assert_int_equal(transport.closes, 0);
		portal_inhibit(info, "retry", true);
		complete_call(false);
		respond(0);
		portal_inhibit_info_destroy(info);
	}
}

static void test_method_error(void **state)
{
	(void)state;
	struct portal_inhibit_info *info = portal_inhibit_info_create();
	portal_inhibit(info, "recording", true);
	complete_call(true);
	assert_false(info->active);
	assert_null(info->request_path);
	assert_null(info->cancellable);
	assert_int_equal(transport.unsubscribes, 1);
	portal_inhibit(info, "retry", true);
	assert_int_equal(transport.inhibits, 2);
	complete_call(false);
	respond(0);
	portal_inhibit_info_destroy(info);
}

static void test_cancel_pending(void **state)
{
	(void)state;
	struct portal_inhibit_info *info = portal_inhibit_info_create();
	portal_inhibit(info, "recording", true);
	portal_inhibit(info, "unused", false);
	assert_true(g_cancellable_is_cancelled(transport.calls[0].cancel));
	assert_false(info->active);
	assert_null(info->cancellable);
	assert_null(info->request_path);
	assert_int_equal(transport.closes, 0);
	complete_call(true);
	assert_int_equal(transport.unsubscribes, 1);
	portal_inhibit_info_destroy(info);
}

static void test_destroy_pending(void **state)
{
	(void)state;
	struct portal_inhibit_info *info = portal_inhibit_info_create();
	portal_inhibit(info, "recording", true);
	portal_inhibit_info_destroy(info);
	assert_true(g_cancellable_is_cancelled(transport.calls[0].cancel));
	assert_int_equal(transport.unsubscribes, 1);
	assert_int_equal(transport.closes, 0);
	/* The callback owns the remaining reference after owner destruction. */
	assert_true(bnum_allocs() > allocations);
	complete_call(true);
	assert_int_equal(bnum_allocs(), allocations);
}

static void test_destroy_pending_close(void **state)
{
	(void)state;
	struct portal_inhibit_info *info = portal_inhibit_info_create();
	portal_inhibit(info, "recording", true);
	complete_call(false);
	respond(0);
	portal_inhibit(info, "unused", false);
	portal_inhibit_info_destroy(info);
	assert_true(bnum_allocs() > allocations);
	complete_call(false);
	assert_int_equal(bnum_allocs(), allocations);
}

/* The Response signal can precede the Inhibit method reply. */
static void test_response_before_reply(void **state)
{
	(void)state;
	const unsigned codes[] = {0, 1, 2};
	for (size_t i = 0; i < G_N_ELEMENTS(codes); i++) {
		struct portal_inhibit_info *info = portal_inhibit_info_create();
		portal_inhibit(info, "recording", true);
		respond(codes[i]);
		assert_int_equal(info->active, codes[i] == 0);
		assert_int_equal(info->signal_id, 0);
		assert_non_null(info->cancellable);
		complete_call(false);
		assert_null(info->cancellable);
		assert_int_equal(info->active, codes[i] == 0);
		portal_inhibit_info_destroy(info);
	}
}

static void test_destroy_active(void **state)
{
	(void)state;
	struct portal_inhibit_info *info = portal_inhibit_info_create();
	portal_inhibit(info, "recording", true);
	complete_call(false);
	respond(0);
	portal_inhibit_info_destroy(info);
	/* Characterized, not endorsed: destroying an active handle does not
	 * send Request.Close; only explicit deactivation does. */
	assert_int_equal(transport.closes, 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_discovery, setup, teardown),
		cmocka_unit_test_setup_teardown(test_request_and_success, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rejected_responses, setup, teardown),
		cmocka_unit_test_setup_teardown(test_method_error, setup, teardown),
		cmocka_unit_test_setup_teardown(test_cancel_pending, setup, teardown),
		cmocka_unit_test_setup_teardown(test_destroy_pending, setup, teardown),
		cmocka_unit_test_setup_teardown(test_destroy_pending_close, setup, teardown),
		cmocka_unit_test_setup_teardown(test_response_before_reply, setup, teardown),
		cmocka_unit_test_setup_teardown(test_destroy_active, setup, teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
