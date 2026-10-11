/* Global hotkeys through the xdg-desktop-portal GlobalShortcuts interface
 * (upstream obsproject/obs-studio#13169, #10701, #10538).
 *
 * A fake org.freedesktop.portal.GlobalShortcuts runs on a private D-Bus
 * session bus (GTestDBus starts its own dbus-daemon; the user's session bus
 * is never touched). The portal client binds the combinations of the real
 * libobs hotkey bindings (obs-hotkey.c with an isolated core), and the fake
 * portal's Activated/Deactivated signals must press and release the hotkeys
 * through the real libobs hotkey thread. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <gio/gio.h>

#include <obs-internal.h>
#include <util/profiler.h>

#include "obs-nix-hotkey-state.h"
#include "obs-nix-portal-shortcuts.h"

#define PORTAL_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_PATH "/org/freedesktop/portal/desktop"
#define SHORTCUTS_IFACE "org.freedesktop.portal.GlobalShortcuts"
#define REGISTRY_IFACE "org.freedesktop.host.portal.Registry"

struct obs_core *hotkey_test_obs;

static struct obs_nix_hotkey_state key_state;
static pthread_t hotkey_thread;
static profiler_name_store_t *name_store;

bool obs_hotkeys_platform_is_pressed(obs_hotkeys_platform_t *context, obs_key_t key)
{
	UNUSED_PARAMETER(context);
	return obs_nix_hotkey_state_is_pressed(&key_state, key);
}

profiler_name_store_t *obs_get_profiler_name_store(void)
{
	return name_store;
}

/* ------------------------------------------------------------------------ */
/* Fake portal                                                               */

static const char fake_portal_xml[] =
	"<node>"
	"  <interface name='org.freedesktop.portal.GlobalShortcuts'>"
	"    <method name='CreateSession'>"
	"      <arg type='a{sv}' name='options' direction='in'/>"
	"      <arg type='o' name='handle' direction='out'/>"
	"    </method>"
	"    <method name='BindShortcuts'>"
	"      <arg type='o' name='session_handle' direction='in'/>"
	"      <arg type='a(sa{sv})' name='shortcuts' direction='in'/>"
	"      <arg type='s' name='parent_window' direction='in'/>"
	"      <arg type='a{sv}' name='options' direction='in'/>"
	"      <arg type='o' name='request_handle' direction='out'/>"
	"    </method>"
	"    <method name='ListShortcuts'>"
	"      <arg type='o' name='session_handle' direction='in'/>"
	"      <arg type='a{sv}' name='options' direction='in'/>"
	"      <arg type='o' name='request_handle' direction='out'/>"
	"    </method>"
	"    <signal name='Activated'>"
	"      <arg type='o' name='session_handle'/><arg type='s' name='shortcut_id'/>"
	"      <arg type='t' name='timestamp'/><arg type='a{sv}' name='options'/>"
	"    </signal>"
	"    <signal name='Deactivated'>"
	"      <arg type='o' name='session_handle'/><arg type='s' name='shortcut_id'/>"
	"      <arg type='t' name='timestamp'/><arg type='a{sv}' name='options'/>"
	"    </signal>"
	"    <signal name='ShortcutsChanged'>"
	"      <arg type='o' name='session_handle'/><arg type='a(sa{sv})' name='shortcuts'/>"
	"    </signal>"
	"    <property name='version' type='u' access='read'/>"
	"  </interface>"
	"  <interface name='org.freedesktop.host.portal.Registry'>"
	"    <method name='Register'>"
	"      <arg type='s' name='app_id' direction='in'/>"
	"      <arg type='a{sv}' name='options' direction='in'/>"
	"    </method>"
	"  </interface>"
	"  <interface name='org.freedesktop.portal.Session'>"
	"    <method name='Close'/>"
	"  </interface>"
	"</node>";

struct fake_session {
	char *path;
	char *sender;
	guint registration;
	bool bound;
	bool closed;
};

static struct fake_portal {
	GTestDBus *bus;
	GDBusConnection *conn;
	GDBusNodeInfo *info;
	guint registration;
	guint registry_registration;
	struct fake_session sessions[16];
	size_t num_sessions;
	size_t num_binds;
	/* org.freedesktop.host.portal.Registry.Register calls */
	size_t num_registers;
	char *registered_app_id;
	/* Register came after a GlobalShortcuts call on that connection */
	bool register_too_late;
	/* Last BindShortcuts call: a(sa{sv}) */
	GVariant *last_shortcuts;
	char *last_bind_session;
} fake;

static char *request_path(const char *sender, const char *token)
{
	char *escaped = g_strdup(sender + 1);
	g_strdelimit(escaped, ".", '_');
	char *path = g_strdup_printf("/org/freedesktop/portal/desktop/request/%s/%s", escaped, token);
	g_free(escaped);
	return path;
}

static void respond(const char *sender, const char *path, GVariant *results)
{
	g_dbus_connection_emit_signal(fake.conn, sender, path, "org.freedesktop.portal.Request", "Response",
				      g_variant_new("(u@a{sv})", 0, results), NULL);
}

static struct fake_session *find_session(const char *path)
{
	for (size_t i = 0; i < fake.num_sessions; i++) {
		if (strcmp(fake.sessions[i].path, path) == 0)
			return &fake.sessions[i];
	}
	return NULL;
}

static void session_method(GDBusConnection *conn, const char *sender, const char *path, const char *iface,
			   const char *method, GVariant *params, GDBusMethodInvocation *invocation, gpointer data)
{
	UNUSED_PARAMETER(conn);
	UNUSED_PARAMETER(sender);
	UNUSED_PARAMETER(iface);
	UNUSED_PARAMETER(method);
	UNUSED_PARAMETER(params);
	struct fake_session *session = data;
	UNUSED_PARAMETER(path);
	session->closed = true;
	g_dbus_method_invocation_return_value(invocation, NULL);
}

static const GDBusInterfaceVTable session_vtable = {session_method, NULL, NULL, {0}};

static void portal_method(GDBusConnection *conn, const char *sender, const char *path, const char *iface,
			  const char *method, GVariant *params, GDBusMethodInvocation *invocation, gpointer data)
{
	UNUSED_PARAMETER(path);
	UNUSED_PARAMETER(iface);
	UNUSED_PARAMETER(data);

	if (fake.num_registers == 0 && fake.num_sessions == 0)
		fake.register_too_late = true;

	if (strcmp(method, "CreateSession") == 0) {
		g_autoptr(GVariant) options = NULL;
		const char *token = "t", *session_token = "s";
		g_variant_get(params, "(@a{sv})", &options);
		g_variant_lookup(options, "handle_token", "&s", &token);
		g_variant_lookup(options, "session_handle_token", "&s", &session_token);

		char *req = request_path(sender, token);
		char *escaped = g_strdup(sender + 1);
		g_strdelimit(escaped, ".", '_');
		char *session_path =
			g_strdup_printf("/org/freedesktop/portal/desktop/session/%s/%s", escaped, session_token);
		g_free(escaped);

		struct fake_session *session = &fake.sessions[fake.num_sessions++];
		session->path = session_path;
		session->sender = g_strdup(sender);
		GDBusInterfaceInfo *session_iface =
			g_dbus_node_info_lookup_interface(fake.info, "org.freedesktop.portal.Session");
		session->registration = g_dbus_connection_register_object(conn, session_path, session_iface,
									  &session_vtable, session, NULL, NULL);

		g_dbus_method_invocation_return_value(invocation, g_variant_new("(o)", req));

		GVariantBuilder results;
		g_variant_builder_init(&results, G_VARIANT_TYPE_VARDICT);
		/* xdg-desktop-portal sends the handle as a string. */
		g_variant_builder_add(&results, "{sv}", "session_handle", g_variant_new_string(session_path));
		respond(sender, req, g_variant_builder_end(&results));
		g_free(req);
		return;
	}

	if (strcmp(method, "BindShortcuts") == 0) {
		const char *session_path, *parent;
		g_autoptr(GVariant) shortcuts = NULL;
		g_autoptr(GVariant) options = NULL;
		const char *token = "t";
		g_variant_get(params, "(&o@a(sa{sv})&s@a{sv})", &session_path, &shortcuts, &parent, &options);
		g_variant_lookup(options, "handle_token", "&s", &token);

		struct fake_session *session = find_session(session_path);
		if (!session || session->closed || session->bound) {
			/* Like xdg-desktop-portal: one BindShortcuts per session. */
			g_dbus_method_invocation_return_dbus_error(invocation, "org.freedesktop.portal.Error.Failed",
								   "Invalid session state");
			return;
		}
		session->bound = true;
		fake.num_binds++;
		g_clear_pointer(&fake.last_shortcuts, g_variant_unref);
		fake.last_shortcuts = g_variant_ref(shortcuts);
		g_free(fake.last_bind_session);
		fake.last_bind_session = g_strdup(session_path);

		char *req = request_path(sender, token);
		g_dbus_method_invocation_return_value(invocation, g_variant_new("(o)", req));

		/* Echo the shortcuts back with a trigger description. */
		GVariantBuilder list;
		g_variant_builder_init(&list, G_VARIANT_TYPE("a(sa{sv})"));
		GVariantIter iter;
		const char *id;
		GVariant *props;
		g_variant_iter_init(&iter, shortcuts);
		while (g_variant_iter_next(&iter, "(&s@a{sv})", &id, &props)) {
			const char *trigger = "";
			const char *description = "";
			g_variant_lookup(props, "preferred_trigger", "&s", &trigger);
			g_variant_lookup(props, "description", "&s", &description);
			GVariantBuilder out;
			g_variant_builder_init(&out, G_VARIANT_TYPE_VARDICT);
			g_variant_builder_add(&out, "{sv}", "description", g_variant_new_string(description));
			g_variant_builder_add(&out, "{sv}", "trigger_description", g_variant_new_string(trigger));
			g_variant_builder_add(&list, "(s@a{sv})", id, g_variant_builder_end(&out));
			g_variant_unref(props);
		}
		GVariantBuilder results;
		g_variant_builder_init(&results, G_VARIANT_TYPE_VARDICT);
		g_variant_builder_add(&results, "{sv}", "shortcuts", g_variant_builder_end(&list));
		respond(sender, req, g_variant_builder_end(&results));
		g_free(req);
		return;
	}

	g_dbus_method_invocation_return_dbus_error(invocation, "org.freedesktop.DBus.Error.UnknownMethod", method);
}

static void registry_method(GDBusConnection *conn, const char *sender, const char *path, const char *iface,
			    const char *method, GVariant *params, GDBusMethodInvocation *invocation, gpointer data)
{
	UNUSED_PARAMETER(conn);
	UNUSED_PARAMETER(sender);
	UNUSED_PARAMETER(path);
	UNUSED_PARAMETER(iface);
	UNUSED_PARAMETER(method);
	UNUSED_PARAMETER(data);
	const char *app_id;
	g_variant_get(params, "(&s@a{sv})", &app_id, NULL);
	g_free(fake.registered_app_id);
	fake.registered_app_id = g_strdup(app_id);
	fake.num_registers++;
	g_dbus_method_invocation_return_value(invocation, NULL);
}

static const GDBusInterfaceVTable registry_vtable = {registry_method, NULL, NULL, {0}};

static GVariant *portal_property(GDBusConnection *conn, const char *sender, const char *path, const char *iface,
				 const char *property, GError **error, gpointer data)
{
	UNUSED_PARAMETER(conn);
	UNUSED_PARAMETER(sender);
	UNUSED_PARAMETER(path);
	UNUSED_PARAMETER(iface);
	UNUSED_PARAMETER(error);
	UNUSED_PARAMETER(data);
	if (fake.num_registers == 0)
		fake.register_too_late = true;
	if (strcmp(property, "version") == 0)
		return g_variant_new_uint32(1);
	return NULL;
}

static const GDBusInterfaceVTable portal_vtable = {portal_method, portal_property, NULL, {0}};

static void pump(void)
{
	while (g_main_context_iteration(NULL, FALSE))
		;
}

/* Runs the fake portal while waiting for cond. */
static bool wait_until(bool (*cond)(void *), void *data)
{
	for (int i = 0; i < 400; i++) {
		pump();
		if (cond(data))
			return true;
		os_sleep_ms(10);
	}
	return false;
}

static void serve_for(int ms)
{
	for (int i = 0; i < ms / 10; i++) {
		pump();
		os_sleep_ms(10);
	}
}

/* Takes or drops the portal's well-known name, as a portal starting or
 * exiting does. */
static void fake_portal_own_name(bool own)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GVariant) reply = g_dbus_connection_call_sync(
		fake.conn, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
		own ? "RequestName" : "ReleaseName",
		own ? g_variant_new("(su)", PORTAL_NAME, 4 /* DO_NOT_QUEUE */) : g_variant_new("(s)", PORTAL_NAME),
		G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
	assert_non_null(reply);
	g_dbus_connection_flush_sync(fake.conn, NULL, NULL);
}

static void fake_portal_start(bool own_portal)
{
	memset(&fake, 0, sizeof(fake));
	fake.bus = g_test_dbus_new(G_TEST_DBUS_NONE);
	g_test_dbus_up(fake.bus);

	g_autoptr(GError) error = NULL;
	fake.conn = g_dbus_connection_new_for_address_sync(g_test_dbus_get_bus_address(fake.bus),
							   G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
								   G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
							   NULL, NULL, &error);
	assert_non_null(fake.conn);

	if (!own_portal)
		return;

	fake.info = g_dbus_node_info_new_for_xml(fake_portal_xml, &error);
	assert_non_null(fake.info);
	fake.registration = g_dbus_connection_register_object(
		fake.conn, PORTAL_PATH, g_dbus_node_info_lookup_interface(fake.info, SHORTCUTS_IFACE), &portal_vtable,
		NULL, NULL, &error);
	assert_true(fake.registration > 0);
	fake.registry_registration = g_dbus_connection_register_object(
		fake.conn, PORTAL_PATH, g_dbus_node_info_lookup_interface(fake.info, REGISTRY_IFACE), &registry_vtable,
		NULL, NULL, &error);
	assert_true(fake.registry_registration > 0);

	fake_portal_own_name(true);
}

static void fake_portal_stop(void)
{
	pump();
	for (size_t i = 0; i < fake.num_sessions; i++) {
		if (fake.sessions[i].registration)
			g_dbus_connection_unregister_object(fake.conn, fake.sessions[i].registration);
		g_free(fake.sessions[i].path);
		g_free(fake.sessions[i].sender);
	}
	if (fake.registration)
		g_dbus_connection_unregister_object(fake.conn, fake.registration);
	if (fake.registry_registration)
		g_dbus_connection_unregister_object(fake.conn, fake.registry_registration);
	g_clear_pointer(&fake.registered_app_id, g_free);
	g_clear_pointer(&fake.last_shortcuts, g_variant_unref);
	g_clear_pointer(&fake.last_bind_session, g_free);
	g_clear_pointer(&fake.info, g_dbus_node_info_unref);
	g_dbus_connection_close_sync(fake.conn, NULL, NULL);
	g_clear_object(&fake.conn);
	g_test_dbus_down(fake.bus);
	g_clear_object(&fake.bus);
}

static void emit_shortcut(const char *session_path, const char *id, bool active)
{
	const char *sender = fake.sessions[fake.num_sessions - 1].sender;
	g_dbus_connection_emit_signal(fake.conn, sender, PORTAL_PATH, SHORTCUTS_IFACE,
				      active ? "Activated" : "Deactivated",
				      g_variant_new("(ost@a{sv})", session_path, id, (guint64)0,
						    g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0)),
				      NULL);
	g_dbus_connection_flush_sync(fake.conn, NULL, NULL);
}

/* The user gave a shortcut another trigger in the desktop settings. */
static void emit_shortcuts_changed(const char *session_path, const char *id, const char *trigger)
{
	const char *sender = fake.sessions[fake.num_sessions - 1].sender;
	GVariantBuilder props;
	g_variant_builder_init(&props, G_VARIANT_TYPE_VARDICT);
	g_variant_builder_add(&props, "{sv}", "description", g_variant_new_string("changed"));
	g_variant_builder_add(&props, "{sv}", "trigger_description", g_variant_new_string(trigger));
	GVariantBuilder list;
	g_variant_builder_init(&list, G_VARIANT_TYPE("a(sa{sv})"));
	g_variant_builder_add(&list, "(s@a{sv})", id, g_variant_builder_end(&props));
	g_dbus_connection_emit_signal(fake.conn, sender, PORTAL_PATH, SHORTCUTS_IFACE, "ShortcutsChanged",
				      g_variant_new("(o@a(sa{sv}))", session_path, g_variant_builder_end(&list)), NULL);
	g_dbus_connection_flush_sync(fake.conn, NULL, NULL);
}

static void emit_session_closed(struct fake_session *session)
{
	session->closed = true;
	g_dbus_connection_emit_signal(fake.conn, session->sender, session->path, "org.freedesktop.portal.Session",
				      "Closed",
				      g_variant_new("(@a{sv})", g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0)),
				      NULL);
	g_dbus_connection_flush_sync(fake.conn, NULL, NULL);
}

/* ------------------------------------------------------------------------ */
/* libobs side                                                               */

struct hotkey_log {
	volatile long pressed;
	volatile long presses;
};

static void hotkey_callback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	struct hotkey_log *log = data;
	os_atomic_set_long(&log->pressed, pressed);
	if (pressed)
		os_atomic_inc_long(&log->presses);
}

static bool test_key_name(obs_key_t key, struct dstr *name)
{
	const char *full = obs_key_to_name(key);
	if (strncmp(full, "OBS_KEY_", 8) != 0 || key == OBS_KEY_NONE)
		return false;
	dstr_copy(name, full + 8);
	if (name->len == 1)
		dstr_to_lower(name);
	return true;
}

static bool dbus_daemon_available(void)
{
	char *path = g_find_program_in_path("dbus-daemon");
	bool found = path != NULL;
	g_free(path);
	return found;
}

static int setup(void **state)
{
	UNUSED_PARAMETER(state);
	if (!dbus_daemon_available())
		return 0;
	memset(&key_state, 0, sizeof(key_state));
	name_store = profiler_name_store_create();
	hotkey_test_obs = bzalloc(sizeof(*hotkey_test_obs));
	assert_int_equal(pthread_mutex_init_recursive(&hotkey_test_obs->hotkeys.mutex), 0);
	assert_int_equal(os_event_init(&hotkey_test_obs->hotkeys.stop_event, OS_EVENT_TYPE_MANUAL), 0);
	hotkey_test_obs->hotkeys.strict_modifiers = true;
	hotkey_test_obs->hotkeys.signals = signal_handler_create();
	static const char *signals[] = {"void hotkey_register(ptr key)", "void hotkey_unregister(ptr key)",
					"void hotkey_bindings_changed(ptr key)", NULL};
	assert_true(signal_handler_add_array(hotkey_test_obs->hotkeys.signals, signals));
	assert_int_equal(pthread_create(&hotkey_thread, NULL, obs_hotkey_thread, NULL), 0);
	return 0;
}

static int teardown(void **state)
{
	UNUSED_PARAMETER(state);
	if (!hotkey_test_obs)
		return 0;
	os_event_signal(hotkey_test_obs->hotkeys.stop_event);
	pthread_join(hotkey_thread, NULL);
	os_event_destroy(hotkey_test_obs->hotkeys.stop_event);
	obs_hotkeys_free();
	obs_nix_hotkey_state_free(&key_state);
	signal_handler_destroy(hotkey_test_obs->hotkeys.signals);
	pthread_mutex_destroy(&hotkey_test_obs->hotkeys.mutex);
	bfree(hotkey_test_obs);
	hotkey_test_obs = NULL;
	profiler_name_store_free(name_store);
	name_store = NULL;
	return 0;
}

#define REQUIRE_DBUS_DAEMON()                                                           \
	do {                                                                            \
		if (!dbus_daemon_available()) {                                         \
			fprintf(stderr, "dbus-daemon not found; skipping portal test\n"); \
			skip();                                                         \
		}                                                                       \
	} while (false)

static obs_hotkey_id register_bound(const char *name, struct hotkey_log *log, obs_key_combination_t combo)
{
	obs_hotkey_id id = obs_hotkey_register_frontend(name, name, hotkey_callback, log);
	assert_int_not_equal(id, OBS_INVALID_HOTKEY_ID);
	obs_hotkey_load_bindings(id, &combo, 1);
	return id;
}

static bool binds_at_least(void *data)
{
	return fake.num_binds >= (size_t)(uintptr_t)data;
}

static bool session_is_closed(void *data)
{
	return ((struct fake_session *)data)->closed;
}

static bool log_pressed(void *data)
{
	return os_atomic_load_long(&((struct hotkey_log *)data)->pressed) != 0;
}

static bool log_released(void *data)
{
	return os_atomic_load_long(&((struct hotkey_log *)data)->pressed) == 0;
}

static bool bound_shortcut(const char *want_id, const char **trigger, const char **description);

/* No further BindShortcuts arrives while the fake portal runs for ms. */
static void expect_no_bind_after(size_t count, int ms, const char *why)
{
	serve_for(ms);
	if (fake.num_binds != count) {
		fprintf(stderr, "%s: expected %zu BindShortcuts calls, got %zu\n", why, count, fake.num_binds);
		fail();
	}
}

static bool bound_has(const char *want_id)
{
	const char *trigger, *description;
	return bound_shortcut(want_id, &trigger, &description);
}

static void expect_bind(size_t count)
{
	if (!wait_until(binds_at_least, (void *)(uintptr_t)count)) {
		fprintf(stderr, "portal never received BindShortcuts #%zu (sessions %zu, binds %zu)\n", count,
			fake.num_sessions, fake.num_binds);
		fail();
	}
}

/* Shortcut id -> preferred_trigger of the last BindShortcuts call. */
static bool bound_shortcut(const char *want_id, const char **trigger, const char **description)
{
	GVariantIter iter;
	const char *id;
	GVariant *props;
	bool found = false;
	g_variant_iter_init(&iter, fake.last_shortcuts);
	while (g_variant_iter_next(&iter, "(&s@a{sv})", &id, &props)) {
		if (!found && strcmp(id, want_id) == 0) {
			found = true;
			*trigger = NULL;
			*description = NULL;
			g_variant_lookup(props, "preferred_trigger", "&s", trigger);
			g_variant_lookup(props, "description", "&s", description);
		}
		g_variant_unref(props);
	}
	return found;
}

static const obs_key_combination_t f13 = {0, OBS_KEY_F13};
static const obs_key_combination_t f14 = {0, OBS_KEY_F14};
static const obs_key_combination_t ctrl_shift_a = {INTERACT_CONTROL_KEY | INTERACT_SHIFT_KEY, OBS_KEY_A};

static void shortcut_ids_round_trip(void **state)
{
	UNUSED_PARAMETER(state);
	const obs_key_combination_t combos[] = {
		f13,
		ctrl_shift_a,
		{INTERACT_ALT_KEY | INTERACT_COMMAND_KEY, OBS_KEY_F1},
		{INTERACT_SHIFT_KEY | INTERACT_CONTROL_KEY | INTERACT_ALT_KEY | INTERACT_COMMAND_KEY, OBS_KEY_SPACE},
	};
	for (size_t i = 0; i < sizeof(combos) / sizeof(combos[0]); i++) {
		struct dstr id = {0};
		obs_key_combination_t back = {0, OBS_KEY_NONE};
		if (!obs_nix_portal_shortcut_id(combos[i], &id) || !obs_nix_portal_shortcut_combo(id.array, &back)) {
			fprintf(stderr, "combination %zu has no shortcut id round trip (id '%s')\n", i,
				id.array ? id.array : "");
			dstr_free(&id);
			fail();
		}
		assert_int_equal(back.key, combos[i].key);
		assert_int_equal(back.modifiers, combos[i].modifiers);
		dstr_free(&id);
	}

	struct dstr id = {0};
	assert_true(obs_nix_portal_shortcut_id(ctrl_shift_a, &id));
	assert_string_equal(id.array, "ctrl+shift+OBS_KEY_A");
	dstr_free(&id);

	obs_key_combination_t combo;
	assert_false(obs_nix_portal_shortcut_combo("", &combo));
	assert_false(obs_nix_portal_shortcut_combo("ctrl+", &combo));
	assert_false(obs_nix_portal_shortcut_combo("hyper+OBS_KEY_A", &combo));
	assert_false(obs_nix_portal_shortcut_combo("OBS_KEY_NOT_A_KEY", &combo));
	assert_false(obs_nix_portal_shortcut_combo("some-other-app-shortcut", &combo));

	struct dstr trigger = {0};
	assert_true(obs_nix_portal_shortcut_trigger(ctrl_shift_a, test_key_name, &trigger));
	assert_string_equal(trigger.array, "CTRL+SHIFT+a");
	assert_true(obs_nix_portal_shortcut_trigger((obs_key_combination_t){INTERACT_ALT_KEY | INTERACT_COMMAND_KEY,
									    OBS_KEY_F13},
						    test_key_name, &trigger));
	assert_string_equal(trigger.array, "ALT+LOGO+F13");
	dstr_free(&trigger);
}

/* One portal shortcut per distinct combination, with the OBS binding as the
 * preferred trigger; then Activated/Deactivated press and release every
 * hotkey bound to it through the libobs hotkey thread. */
static void portal_drives_push_to_talk(void **state)
{
	UNUSED_PARAMETER(state);
	REQUIRE_DBUS_DAEMON();
	fake_portal_start(true);

	struct hotkey_log talk = {0}, mute = {0}, record = {0};
	register_bound("push-to-talk", &talk, f13);
	register_bound("push-to-mute", &mute, f13);
	register_bound("record", &record, ctrl_shift_a);

	struct obs_nix_portal_shortcuts *portal = obs_nix_portal_shortcuts_start(&key_state, test_key_name);
	expect_bind(1);

	/* Host app id, registered before any GlobalShortcuts call. */
	if (fake.num_registers != 1 || fake.register_too_late || !fake.registered_app_id) {
		fprintf(stderr, "Registry.Register: %zu calls, %s, app id '%s'\n", fake.num_registers,
			fake.register_too_late ? "after a GlobalShortcuts call" : "in time",
			fake.registered_app_id ? fake.registered_app_id : "");
		fail();
	}
	assert_string_equal(fake.registered_app_id, "com.obsproject.Studio");

	assert_int_equal(g_variant_n_children(fake.last_shortcuts), 2);
	const char *trigger, *description;
	assert_true(bound_shortcut("OBS_KEY_F13", &trigger, &description));
	assert_non_null(trigger);
	assert_string_equal(trigger, "F13");
	assert_non_null(description);
	assert_true(bound_shortcut("ctrl+shift+OBS_KEY_A", &trigger, &description));
	assert_non_null(trigger);
	assert_string_equal(trigger, "CTRL+SHIFT+a");

	emit_shortcut(fake.last_bind_session, "OBS_KEY_F13", true);
	if (!wait_until(log_pressed, &talk) || !wait_until(log_pressed, &mute)) {
		fprintf(stderr, "Activated did not press the hotkeys (talk %ld, mute %ld)\n", talk.pressed,
			mute.pressed);
		fail();
	}
	serve_for(150);
	assert_true(os_atomic_load_long(&talk.pressed));
	assert_int_equal(os_atomic_load_long(&record.presses), 0);

	emit_shortcut(fake.last_bind_session, "OBS_KEY_F13", false);
	if (!wait_until(log_released, &talk) || !wait_until(log_released, &mute)) {
		fprintf(stderr, "Deactivated did not release the hotkeys\n");
		fail();
	}

	emit_shortcut(fake.last_bind_session, "ctrl+shift+OBS_KEY_A", true);
	assert_true(wait_until(log_pressed, &record));
	emit_shortcut(fake.last_bind_session, "ctrl+shift+OBS_KEY_A", false);
	assert_true(wait_until(log_released, &record));

	/* The user moves the shortcut to another trigger: the id still names
	 * the OBS combination, so nothing is rebound and it still drives the
	 * same hotkeys. */
	emit_shortcuts_changed(fake.last_bind_session, "OBS_KEY_F13", "Pause");
	emit_shortcut(fake.last_bind_session, "OBS_KEY_F13", true);
	assert_true(wait_until(log_pressed, &talk));
	emit_shortcut(fake.last_bind_session, "OBS_KEY_F13", false);
	assert_true(wait_until(log_released, &talk));

	/* An id this client never bound presses nothing. */
	emit_shortcut(fake.last_bind_session, "OBS_KEY_F14", true);
	serve_for(150);
	assert_false(obs_nix_hotkey_state_is_pressed(&key_state, OBS_KEY_F14));

	/* Nothing changed, so nothing is bound again. */
	expect_no_bind_after(1, 1000, "unchanged bindings");

	obs_nix_portal_shortcuts_stop(portal);
	fake_portal_stop();
}

/* Changing the bindings rebinds in a new session (the portal accepts one
 * BindShortcuts per session), and the old session is closed. */
static void rebinds_when_bindings_change(void **state)
{
	UNUSED_PARAMETER(state);
	REQUIRE_DBUS_DAEMON();
	fake_portal_start(true);

	struct hotkey_log talk = {0};
	obs_hotkey_id id = register_bound("push-to-talk", &talk, f13);

	struct obs_nix_portal_shortcuts *portal = obs_nix_portal_shortcuts_start(&key_state, test_key_name);
	expect_bind(1);
	char *first_session = g_strdup(fake.last_bind_session);

	obs_hotkey_load_bindings(id, (obs_key_combination_t *)&f14, 1);
	expect_bind(2);

	assert_string_not_equal(first_session, fake.last_bind_session);
	struct fake_session *old = find_session(first_session);
	assert_non_null(old);
	assert_true(old->closed);
	const char *trigger, *description;
	assert_int_equal(g_variant_n_children(fake.last_shortcuts), 1);
	assert_true(bound_shortcut("OBS_KEY_F14", &trigger, &description));

	/* A late signal from the closed session must not press anything. */
	emit_shortcut(first_session, "OBS_KEY_F14", true);
	serve_for(150);
	assert_int_equal(os_atomic_load_long(&talk.presses), 0);

	emit_shortcut(fake.last_bind_session, "OBS_KEY_F14", true);
	assert_true(wait_until(log_pressed, &talk));
	emit_shortcut(fake.last_bind_session, "OBS_KEY_F14", false);
	assert_true(wait_until(log_released, &talk));

	/* The last hotkey goes away: the session is closed, nothing bound. */
	struct fake_session *second = find_session(fake.last_bind_session);
	assert_non_null(second);
	obs_hotkey_unregister(id);
	if (!wait_until(session_is_closed, second)) {
		fprintf(stderr, "session not closed after the last hotkey was unregistered\n");
		fail();
	}
	expect_no_bind_after(2, 300, "no hotkeys left");

	g_free(first_session);
	obs_nix_portal_shortcuts_stop(portal);
	fake_portal_stop();
}

/* Many binding changes in a row (loading a profile or a scene collection)
 * are bound once, with the final set. */
static void burst_of_changes_binds_once(void **state)
{
	UNUSED_PARAMETER(state);
	REQUIRE_DBUS_DAEMON();
	fake_portal_start(true);

	struct hotkey_log talk = {0}, other = {0};
	obs_hotkey_id id = register_bound("push-to-talk", &talk, f13);

	struct obs_nix_portal_shortcuts *portal = obs_nix_portal_shortcuts_start(&key_state, test_key_name);
	expect_bind(1);

	const obs_key_combination_t steps[] = {f14, ctrl_shift_a, f13, f14};
	for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
		obs_hotkey_load_bindings(id, (obs_key_combination_t *)&steps[i], 1);
		serve_for(50);
	}
	register_bound("other", &other, ctrl_shift_a);

	expect_bind(2);
	expect_no_bind_after(2, 1000, "one burst of binding changes");
	assert_int_equal(g_variant_n_children(fake.last_shortcuts), 2);
	assert_true(bound_has("OBS_KEY_F14"));
	assert_true(bound_has("ctrl+shift+OBS_KEY_A"));

	obs_nix_portal_shortcuts_stop(portal);
	fake_portal_stop();
}

/* The desktop closes the session (the user revoked it): held hotkeys are
 * released and the same shortcuts are not bound again behind the user's
 * back; assigning a new combination in OBS binds again. */
static void closed_session_is_not_rebound(void **state)
{
	UNUSED_PARAMETER(state);
	REQUIRE_DBUS_DAEMON();
	fake_portal_start(true);

	struct hotkey_log talk = {0}, record = {0};
	register_bound("push-to-talk", &talk, f13);

	struct obs_nix_portal_shortcuts *portal = obs_nix_portal_shortcuts_start(&key_state, test_key_name);
	expect_bind(1);

	emit_shortcut(fake.last_bind_session, "OBS_KEY_F13", true);
	assert_true(wait_until(log_pressed, &talk));

	struct fake_session *session = find_session(fake.last_bind_session);
	assert_non_null(session);
	emit_session_closed(session);
	if (!wait_until(log_released, &talk)) {
		fprintf(stderr, "push-to-talk still held after the desktop closed the session\n");
		fail();
	}
	expect_no_bind_after(1, 1500, "session closed by the desktop");

	register_bound("record", &record, ctrl_shift_a);
	expect_bind(2);
	assert_string_not_equal(session->path, fake.last_bind_session);
	assert_true(bound_has("OBS_KEY_F13"));
	assert_true(bound_has("ctrl+shift+OBS_KEY_A"));

	emit_shortcut(fake.last_bind_session, "OBS_KEY_F13", true);
	assert_true(wait_until(log_pressed, &talk));
	emit_shortcut(fake.last_bind_session, "OBS_KEY_F13", false);
	assert_true(wait_until(log_released, &talk));

	obs_nix_portal_shortcuts_stop(portal);
	fake_portal_stop();
}

/* The portal restarts (its bus name goes away and comes back): held hotkeys
 * are released, and the client registers its app id with the new instance
 * and binds again. */
static void rebinds_after_portal_restart(void **state)
{
	UNUSED_PARAMETER(state);
	REQUIRE_DBUS_DAEMON();
	fake_portal_start(true);

	struct hotkey_log talk = {0};
	register_bound("push-to-talk", &talk, f13);

	struct obs_nix_portal_shortcuts *portal = obs_nix_portal_shortcuts_start(&key_state, test_key_name);
	expect_bind(1);
	char *first_session = g_strdup(fake.last_bind_session);

	emit_shortcut(fake.last_bind_session, "OBS_KEY_F13", true);
	assert_true(wait_until(log_pressed, &talk));

	fake_portal_own_name(false);
	if (!wait_until(log_released, &talk)) {
		fprintf(stderr, "push-to-talk still held after the portal left the bus\n");
		fail();
	}
	expect_no_bind_after(1, 300, "portal gone");

	fake_portal_own_name(true);
	expect_bind(2);
	assert_string_not_equal(first_session, fake.last_bind_session);
	assert_int_equal(fake.num_registers, 2);

	emit_shortcut(fake.last_bind_session, "OBS_KEY_F13", true);
	assert_true(wait_until(log_pressed, &talk));
	emit_shortcut(fake.last_bind_session, "OBS_KEY_F13", false);
	assert_true(wait_until(log_released, &talk));

	g_free(first_session);
	obs_nix_portal_shortcuts_stop(portal);
	fake_portal_stop();
}

/* No portal on the bus: the client gives up quietly, nothing is pressed, and
 * stopping it does not hang. */
static void no_portal_falls_back(void **state)
{
	UNUSED_PARAMETER(state);
	REQUIRE_DBUS_DAEMON();
	fake_portal_start(false);

	struct hotkey_log talk = {0};
	register_bound("push-to-talk", &talk, f13);

	uint64_t start = os_gettime_ns();
	struct obs_nix_portal_shortcuts *portal = obs_nix_portal_shortcuts_start(&key_state, test_key_name);
	serve_for(300);
	obs_nix_portal_shortcuts_stop(portal);
	uint64_t elapsed_ms = (os_gettime_ns() - start) / 1000000;

	assert_int_equal(os_atomic_load_long(&talk.presses), 0);
	for (obs_key_t key = 0; key < OBS_KEY_LAST_VALUE; key++)
		assert_false(obs_nix_hotkey_state_is_pressed(&key_state, key));
	if (elapsed_ms > 3000) {
		fprintf(stderr, "stopping the portal client took %" PRIu64 " ms\n", elapsed_ms);
		fail();
	}
	fake_portal_stop();
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(shortcut_ids_round_trip),
		cmocka_unit_test_setup_teardown(portal_drives_push_to_talk, setup, teardown),
		cmocka_unit_test_setup_teardown(rebinds_when_bindings_change, setup, teardown),
		cmocka_unit_test_setup_teardown(burst_of_changes_binds_once, setup, teardown),
		cmocka_unit_test_setup_teardown(closed_session_is_not_rebound, setup, teardown),
		cmocka_unit_test_setup_teardown(rebinds_after_portal_restart, setup, teardown),
		cmocka_unit_test_setup_teardown(no_portal_falls_back, setup, teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
