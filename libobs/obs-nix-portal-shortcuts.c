/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* The session and BindShortcuts handling follows the GlobalShortcuts portal
 * client by Adam Fallon (obsproject/obs-studio#13680); feeding the hotkey
 * thread one shortcut per key combination follows the xx-hotkey-v1 client by
 * Aurelle (obsproject/obs-studio#13661). */

#include "obs-nix-portal-shortcuts.h"
#include "util/threading.h"

#include <gio/gio.h>
#include <unistd.h>

#define PORTAL_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_PATH "/org/freedesktop/portal/desktop"
#define SHORTCUTS_IFACE "org.freedesktop.portal.GlobalShortcuts"
#define REQUEST_IFACE "org.freedesktop.portal.Request"
#define SESSION_IFACE "org.freedesktop.portal.Session"
#define REGISTRY_IFACE "org.freedesktop.host.portal.Registry"
#define APP_ID "com.obsproject.Studio"

/* How often the bindings are compared with what the portal has bound. A
 * change is bound once it has been stable for one interval, so loading a
 * profile does not rebind for every source. */
/* A binding change is bound once the bindings have not changed for this
 * long, so loading a profile or scene collection binds once, not once per
 * source. */
#define SYNC_DELAY_MS 500
/* While a tapped shortcut waits for the hotkey thread (25 ms cycle). */
#define FLUSH_INTERVAL_MS 25

typedef DARRAY(obs_key_combination_t) combo_array_t;

struct obs_nix_portal_shortcuts {
	struct obs_nix_hotkey_state *state;
	obs_nix_portal_key_name_func key_name;

	pthread_t thread;
	GMainContext *context;
	volatile bool stopping;
	/* Armed by the hotkey signals from any thread; when it fires, the
	 * portal thread brings the session in line with the bindings. */
	GSource *sync_source;
	bool sync_due;

	GDBusConnection *conn;
	char *sender;
	unsigned int token;
	char *session_path;
	guint activated_id;
	guint deactivated_id;
	guint changed_id;
	guint closed_id;
	guint watch_id;
	/* The portal left the bus; a new instance is bound from scratch. */
	bool portal_gone;
	GSource *flush_source;

	/* With a session: the combinations bound in it. */
	combo_array_t bound;
	/* Without one: combinations the user cancelled, the portal refused or
	 * the desktop closed. They are not bound again (no repeated prompt)
	 * until the bindings gain a combination or the portal restarts. */
	combo_array_t declined;
};

/* ------------------------------------------------------------------------ */
/* Shortcut ids and triggers                                                 */

static const struct {
	uint32_t modifier;
	const char *id;
	const char *trigger;
} modifier_names[] = {
	{INTERACT_CONTROL_KEY, "ctrl", "CTRL"},
	{INTERACT_ALT_KEY, "alt", "ALT"},
	{INTERACT_SHIFT_KEY, "shift", "SHIFT"},
	{INTERACT_COMMAND_KEY, "super", "LOGO"},
};

#define NUM_MODIFIERS (sizeof(modifier_names) / sizeof(modifier_names[0]))

static inline bool bindable(obs_key_combination_t combo)
{
	/* Portal triggers are keyboard shortcuts; modifier-only and mouse
	 * combinations keep working only while OBS is focused. */
	if (combo.key <= OBS_KEY_NONE || combo.key >= OBS_KEY_LAST_VALUE)
		return false;
	return combo.key < OBS_KEY_MOUSE1 || combo.key > OBS_KEY_MOUSE29;
}

bool obs_nix_portal_shortcut_id(obs_key_combination_t combo, struct dstr *id)
{
	if (!bindable(combo))
		return false;

	dstr_free(id);
	for (size_t i = 0; i < NUM_MODIFIERS; i++) {
		if (combo.modifiers & modifier_names[i].modifier) {
			dstr_cat(id, modifier_names[i].id);
			dstr_cat(id, "+");
		}
	}
	dstr_cat(id, obs_key_to_name(combo.key));
	return true;
}

bool obs_nix_portal_shortcut_combo(const char *id, obs_key_combination_t *combo)
{
	obs_key_combination_t result = {0, OBS_KEY_NONE};
	const char *part = id;
	const char *plus;

	while ((plus = strchr(part, '+')) != NULL) {
		size_t len = plus - part;
		bool known = false;
		for (size_t i = 0; i < NUM_MODIFIERS; i++) {
			if (strlen(modifier_names[i].id) == len && strncmp(part, modifier_names[i].id, len) == 0) {
				result.modifiers |= modifier_names[i].modifier;
				known = true;
				break;
			}
		}
		if (!known)
			return false;
		part = plus + 1;
	}

	if (strncmp(part, "OBS_KEY_", 8) != 0)
		return false;
	result.key = obs_key_from_name(part);
	if (!bindable(result))
		return false;

	*combo = result;
	return true;
}

bool obs_nix_portal_shortcut_trigger(obs_key_combination_t combo, obs_nix_portal_key_name_func key_name,
				     struct dstr *trigger)
{
	struct dstr name = {0};
	if (!bindable(combo) || !key_name || !key_name(combo.key, &name) || dstr_is_empty(&name)) {
		dstr_free(&name);
		return false;
	}

	dstr_free(trigger);
	for (size_t i = 0; i < NUM_MODIFIERS; i++) {
		if (combo.modifiers & modifier_names[i].modifier) {
			dstr_cat(trigger, modifier_names[i].trigger);
			dstr_cat(trigger, "+");
		}
	}
	dstr_cat_dstr(trigger, &name);
	dstr_free(&name);
	return true;
}

/* ------------------------------------------------------------------------ */
/* Collecting the combinations to bind                                       */

static inline bool same_combo(obs_key_combination_t a, obs_key_combination_t b)
{
	return a.key == b.key && a.modifiers == b.modifiers;
}

static bool has_combo(const combo_array_t *combos, obs_key_combination_t combo)
{
	for (size_t i = 0; i < combos->num; i++) {
		if (same_combo(combos->array[i], combo))
			return true;
	}
	return false;
}

/* Every combination in a is also in b. */
static bool covered_by(const combo_array_t *a, const combo_array_t *b)
{
	for (size_t i = 0; i < a->num; i++) {
		if (!has_combo(b, a->array[i]))
			return false;
	}
	return true;
}

static bool collect_binding(void *data, size_t idx, obs_hotkey_binding_t *binding)
{
	UNUSED_PARAMETER(idx);
	combo_array_t *combos = data;
	obs_key_combination_t combo = obs_hotkey_binding_get_key_combination(binding);

	if (bindable(combo) && !has_combo(combos, combo))
		da_push_back(*combos, &combo);
	return true;
}

/* ------------------------------------------------------------------------ */
/* Portal requests                                                           */

struct request_result {
	bool done;
	uint32_t response;
	GVariant *results;
};

static void request_response(GDBusConnection *conn, const char *sender_name, const char *object_path,
			     const char *interface_name, const char *signal_name, GVariant *parameters,
			     gpointer user_data)
{
	UNUSED_PARAMETER(conn);
	UNUSED_PARAMETER(sender_name);
	UNUSED_PARAMETER(object_path);
	UNUSED_PARAMETER(interface_name);
	UNUSED_PARAMETER(signal_name);
	struct request_result *result = user_data;

	if (result->done || !g_variant_is_of_type(parameters, G_VARIANT_TYPE("(ua{sv})")))
		return;
	g_variant_get(parameters, "(u@a{sv})", &result->response, &result->results);
	result->done = true;
}

static char *new_token(struct obs_nix_portal_shortcuts *portal, const char *kind)
{
	return g_strdup_printf("obs_shortcuts_%s_%u_%u", kind, (unsigned)getpid(), ++portal->token);
}

static inline bool waiting_for_portal(struct obs_nix_portal_shortcuts *portal)
{
	return !os_atomic_load_bool(&portal->stopping) && !portal->portal_gone;
}

/* Calls a portal method that answers through a Request object and waits for
 * the Response (BindShortcuts may wait for the user in a dialog). Returns
 * the results on success, NULL if the call failed or was cancelled, or if
 * the client is stopping or the portal went away meanwhile. params carries
 * handle_token = token in its options. */
static GVariant *portal_request(struct obs_nix_portal_shortcuts *portal, const char *method, GVariant *params,
				const char *token)
{
	struct request_result result = {0};
	char *path = g_strdup_printf(PORTAL_PATH "/request/%s/%s", portal->sender, token);

	/* Subscribed before the call, so a fast Response is not missed. */
	guint signal_id = g_dbus_connection_signal_subscribe(portal->conn, PORTAL_NAME, REQUEST_IFACE, "Response", path,
							     NULL, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE, request_response,
							     &result, NULL);

	GError *error = NULL;
	GVariant *reply = g_dbus_connection_call_sync(portal->conn, PORTAL_NAME, PORTAL_PATH, SHORTCUTS_IFACE, method,
						      params, G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
						      &error);
	if (!reply) {
		blog(LOG_WARNING, "[portal] GlobalShortcuts.%s failed: %s", method, error->message);
		g_error_free(error);
		g_dbus_connection_signal_unsubscribe(portal->conn, signal_id);
		g_free(path);
		return NULL;
	}

	const char *handle = NULL;
	g_variant_get(reply, "(&o)", &handle);
	if (strcmp(handle, path) != 0) {
		/* Portal older than 0.9 ignores handle_token. */
		g_dbus_connection_signal_unsubscribe(portal->conn, signal_id);
		signal_id = g_dbus_connection_signal_subscribe(portal->conn, PORTAL_NAME, REQUEST_IFACE, "Response",
							       handle, NULL, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
							       request_response, &result, NULL);
	}
	g_variant_unref(reply);

	/* Only signal handlers and the sync timer run in here; the timer just
	 * marks a sync as due, so sync_shortcuts() is never re-entered. */
	while (!result.done && waiting_for_portal(portal))
		g_main_context_iteration(portal->context, true);

	g_dbus_connection_signal_unsubscribe(portal->conn, signal_id);
	g_free(path);

	if (!result.done)
		return NULL;
	if (result.response != 0) {
		blog(LOG_INFO, "[portal] GlobalShortcuts.%s was %s", method,
		     result.response == 1 ? "cancelled" : "refused");
		g_clear_pointer(&result.results, g_variant_unref);
		return NULL;
	}
	return result.results;
}

/* ------------------------------------------------------------------------ */
/* Activated / Deactivated                                                   */

static gboolean flush_released(gpointer data)
{
	struct obs_nix_portal_shortcuts *portal = data;
	if (obs_nix_hotkey_state_flush(portal->state))
		return G_SOURCE_CONTINUE;

	g_source_unref(portal->flush_source);
	portal->flush_source = NULL;
	return G_SOURCE_REMOVE;
}

static void set_shortcut(struct obs_nix_portal_shortcuts *portal, obs_key_combination_t combo, bool active)
{
	obs_nix_hotkey_state_set_shortcut(portal->state, combo, active);

	/* A release the hotkey thread has not seen yet is kept until it has
	 * (so a tap still presses); drop it once it has. */
	if (!active && !portal->flush_source && obs_nix_hotkey_state_flush(portal->state)) {
		portal->flush_source = g_timeout_source_new(FLUSH_INTERVAL_MS);
		g_source_set_callback(portal->flush_source, flush_released, portal, NULL);
		g_source_attach(portal->flush_source, portal->context);
	}
}

static void release_bound(struct obs_nix_portal_shortcuts *portal)
{
	for (size_t i = 0; i < portal->bound.num; i++)
		set_shortcut(portal, portal->bound.array[i], false);
}

static void shortcut_signal(GDBusConnection *conn, const char *sender_name, const char *object_path,
			    const char *interface_name, const char *signal_name, GVariant *parameters,
			    gpointer user_data)
{
	UNUSED_PARAMETER(conn);
	UNUSED_PARAMETER(sender_name);
	UNUSED_PARAMETER(object_path);
	UNUSED_PARAMETER(interface_name);
	struct obs_nix_portal_shortcuts *portal = user_data;
	const char *session_path, *id;
	obs_key_combination_t combo;

	if (!g_variant_is_of_type(parameters, G_VARIANT_TYPE("(osta{sv})")))
		return;
	g_variant_get(parameters, "(&o&st@a{sv})", &session_path, &id, NULL, NULL);
	if (!portal->session_path || strcmp(session_path, portal->session_path) != 0)
		return;
	if (!obs_nix_portal_shortcut_combo(id, &combo) || !has_combo(&portal->bound, combo))
		return;

	set_shortcut(portal, combo, strcmp(signal_name, "Activated") == 0);
}

static void log_shortcuts(GVariant *shortcuts, const char *what)
{
	GVariantIter iter;
	const char *id;
	GVariant *props;
	g_variant_iter_init(&iter, shortcuts);
	while (g_variant_iter_next(&iter, "(&s@a{sv})", &id, &props)) {
		const char *trigger = NULL;
		g_variant_lookup(props, "trigger_description", "&s", &trigger);
		blog(LOG_INFO, "[portal] global shortcut %s %s: %s", id, what,
		     trigger && *trigger ? trigger : "(no trigger)");
		g_variant_unref(props);
	}
}

/* The user changed a trigger in the desktop settings. The shortcut id names
 * the OBS combination, so the new trigger already drives the same hotkeys;
 * only log it. */
static void shortcuts_changed(GDBusConnection *conn, const char *sender_name, const char *object_path,
			      const char *interface_name, const char *signal_name, GVariant *parameters,
			      gpointer user_data)
{
	UNUSED_PARAMETER(conn);
	UNUSED_PARAMETER(sender_name);
	UNUSED_PARAMETER(object_path);
	UNUSED_PARAMETER(interface_name);
	UNUSED_PARAMETER(signal_name);
	struct obs_nix_portal_shortcuts *portal = user_data;
	const char *session_path;
	GVariant *shortcuts;

	if (!g_variant_is_of_type(parameters, G_VARIANT_TYPE("(oa(sa{sv}))")))
		return;
	g_variant_get(parameters, "(&o@a(sa{sv}))", &session_path, &shortcuts);
	if (portal->session_path && strcmp(session_path, portal->session_path) == 0)
		log_shortcuts(shortcuts, "changed");
	g_variant_unref(shortcuts);
}

/* ------------------------------------------------------------------------ */
/* Sessions                                                                  */

/* Forgets the session without telling the portal; held shortcuts are
 * released. */
static void forget_session(struct obs_nix_portal_shortcuts *portal)
{
	g_clear_pointer(&portal->session_path, g_free);
	release_bound(portal);
	da_resize(portal->bound, 0);
}

static void close_session(struct obs_nix_portal_shortcuts *portal)
{
	if (!portal->session_path)
		return;

	if (!portal->portal_gone) {
		GVariant *reply = g_dbus_connection_call_sync(portal->conn, PORTAL_NAME, portal->session_path,
							      SESSION_IFACE, "Close", NULL, NULL,
							      G_DBUS_CALL_FLAGS_NO_AUTO_START, 1000, NULL, NULL);
		if (reply)
			g_variant_unref(reply);
	}
	forget_session(portal);
}

/* The desktop closed the session (the user revoked it, or the backend went
 * away): do not bind the same shortcuts again behind the user's back, as
 * that may prompt again. A new combination, or a portal restart, binds. */
static void session_closed(GDBusConnection *conn, const char *sender_name, const char *object_path,
			   const char *interface_name, const char *signal_name, GVariant *parameters,
			   gpointer user_data)
{
	UNUSED_PARAMETER(conn);
	UNUSED_PARAMETER(sender_name);
	UNUSED_PARAMETER(interface_name);
	UNUSED_PARAMETER(signal_name);
	UNUSED_PARAMETER(parameters);
	struct obs_nix_portal_shortcuts *portal = user_data;

	if (!portal->session_path || strcmp(object_path, portal->session_path) != 0)
		return;

	blog(LOG_INFO, "[portal] The desktop closed the GlobalShortcuts session; "
		       "global hotkeys are bound again when a new key combination is assigned");
	da_copy(portal->declined, portal->bound);
	forget_session(portal);
}

static bool create_session(struct obs_nix_portal_shortcuts *portal)
{
	char *token = new_token(portal, "request");
	char *session_token = new_token(portal, "session");

	GVariantBuilder options;
	g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
	g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token));
	g_variant_builder_add(&options, "{sv}", "session_handle_token", g_variant_new_string(session_token));

	GVariant *results = portal_request(portal, "CreateSession", g_variant_new("(a{sv})", &options), token);
	g_free(token);
	g_free(session_token);
	if (!results)
		return false;

	GVariant *handle = g_variant_lookup_value(results, "session_handle", NULL);
	if (handle && (g_variant_is_of_type(handle, G_VARIANT_TYPE_STRING) ||
		       g_variant_is_of_type(handle, G_VARIANT_TYPE_OBJECT_PATH)))
		portal->session_path = g_variant_dup_string(handle, NULL);
	g_clear_pointer(&handle, g_variant_unref);
	g_variant_unref(results);

	if (!portal->session_path) {
		blog(LOG_WARNING, "[portal] GlobalShortcuts.CreateSession returned no session");
		return false;
	}
	return true;
}

static bool bind_shortcuts(struct obs_nix_portal_shortcuts *portal, const combo_array_t *combos)
{
	GVariantBuilder shortcuts;
	struct dstr id = {0}, trigger = {0}, description = {0};

	g_variant_builder_init(&shortcuts, G_VARIANT_TYPE("a(sa{sv})"));
	for (size_t i = 0; i < combos->num; i++) {
		if (!obs_nix_portal_shortcut_id(combos->array[i], &id))
			continue;

		GVariantBuilder props;
		g_variant_builder_init(&props, G_VARIANT_TYPE_VARDICT);
		bool has_trigger = obs_nix_portal_shortcut_trigger(combos->array[i], portal->key_name, &trigger);
		/* Keep the description stable: some portals cannot update it. */
		dstr_printf(&description, "OBS Studio hotkey %s", has_trigger ? trigger.array : id.array);
		g_variant_builder_add(&props, "{sv}", "description", g_variant_new_string(description.array));
		if (has_trigger)
			g_variant_builder_add(&props, "{sv}", "preferred_trigger", g_variant_new_string(trigger.array));
		g_variant_builder_add(&shortcuts, "(sa{sv})", id.array, &props);
	}
	dstr_free(&id);
	dstr_free(&trigger);
	dstr_free(&description);

	char *token = new_token(portal, "request");
	GVariantBuilder options;
	g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
	g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token));

	GVariant *results = portal_request(
		portal, "BindShortcuts",
		g_variant_new("(oa(sa{sv})sa{sv})", portal->session_path, &shortcuts, "", &options), token);
	g_free(token);
	if (!results)
		return false;

	GVariant *bound = g_variant_lookup_value(results, "shortcuts", G_VARIANT_TYPE("a(sa{sv})"));
	if (bound) {
		log_shortcuts(bound, "bound");
		g_variant_unref(bound);
	}
	g_variant_unref(results);
	return true;
}

/* Brings the portal session in line with the current bindings. The portal
 * accepts one BindShortcuts call per session, so a change closes the
 * session and binds the new set in a fresh one; the ids of combinations
 * that stay bound do not change, so the desktop keeps their triggers. */
static void sync_shortcuts(struct obs_nix_portal_shortcuts *portal)
{
	combo_array_t current;
	da_init(current);
	obs_enum_hotkey_bindings(collect_binding, &current);

	bool unchanged = portal->session_path
				 ? covered_by(&current, &portal->bound) && covered_by(&portal->bound, &current)
				 : current.num == 0 || covered_by(&current, &portal->declined);
	if (unchanged) {
		da_free(current);
		return;
	}

	close_session(portal);
	if (current.num == 0) {
		da_free(current);
		return;
	}

	/* Set before binding: Activated may be dispatched right after the
	 * BindShortcuts response, while bind_shortcuts() is still waiting. */
	da_copy(portal->bound, current);
	if (!create_session(portal) || !bind_shortcuts(portal, &current)) {
		close_session(portal);
		da_move(portal->declined, current);
		return;
	}
	da_resize(portal->declined, 0);
	da_free(current);
}

/* ------------------------------------------------------------------------ */
/* Thread                                                                    */

/* Host (non-sandboxed) apps tell the portal who they are, so the desktop
 * stores their shortcuts under a stable app id; this must be the first
 * portal call on the connection. Sandboxed apps and older portals refuse
 * it, which is fine. */
static void register_app_id(struct obs_nix_portal_shortcuts *portal)
{
	GVariant *reply = g_dbus_connection_call_sync(portal->conn, PORTAL_NAME, PORTAL_PATH, REGISTRY_IFACE,
						      "Register", g_variant_new("(sa{sv})", APP_ID, NULL), NULL,
						      G_DBUS_CALL_FLAGS_NONE, 5000, NULL, NULL);
	if (reply)
		g_variant_unref(reply);
}

static void portal_vanished(GDBusConnection *conn, const char *name, gpointer user_data)
{
	UNUSED_PARAMETER(conn);
	UNUSED_PARAMETER(name);
	struct obs_nix_portal_shortcuts *portal = user_data;

	if (portal->portal_gone)
		return;
	blog(LOG_INFO, "[portal] The portal left the session bus; global hotkeys are paused");
	portal->portal_gone = true;
	forget_session(portal);
}

/* A restarted portal knows nothing of us: register again and bind. */
static void portal_appeared(GDBusConnection *conn, const char *name, const char *owner, gpointer user_data)
{
	UNUSED_PARAMETER(conn);
	UNUSED_PARAMETER(name);
	UNUSED_PARAMETER(owner);
	struct obs_nix_portal_shortcuts *portal = user_data;

	if (!portal->portal_gone)
		return;
	blog(LOG_INFO, "[portal] The portal is back; binding global hotkeys again");
	portal->portal_gone = false;
	register_app_id(portal);
	da_resize(portal->declined, 0);
	portal->sync_due = true;
}

static bool connect_portal(struct obs_nix_portal_shortcuts *portal)
{
	GError *error = NULL;
	char *address = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (!address) {
		blog(LOG_INFO, "[portal] No D-Bus session bus (%s); global hotkeys are disabled", error->message);
		g_error_free(error);
		return false;
	}

	/* A private connection, so the app id registered below and the
	 * dispatch of its signals belong to this thread only. */
	portal->conn = g_dbus_connection_new_for_address_sync(
		address, G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
		NULL, NULL, &error);
	g_free(address);
	if (!portal->conn) {
		blog(LOG_INFO, "[portal] Cannot connect to the session bus (%s); global hotkeys are disabled",
		     error->message);
		g_error_free(error);
		return false;
	}

	char *sender = g_strdup(g_dbus_connection_get_unique_name(portal->conn) + 1);
	g_strdelimit(sender, ".", '_');
	portal->sender = sender;

	register_app_id(portal);

	GVariant *reply =
		g_dbus_connection_call_sync(portal->conn, PORTAL_NAME, PORTAL_PATH, "org.freedesktop.DBus.Properties",
					    "Get", g_variant_new("(ss)", SHORTCUTS_IFACE, "version"),
					    G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
	if (!reply) {
		blog(LOG_INFO,
		     "[portal] GlobalShortcuts portal not available (%s); "
		     "hotkeys only work while OBS is focused",
		     error->message);
		g_error_free(error);
		return false;
	}

	GVariant *version = NULL;
	g_variant_get(reply, "(v)", &version);
	blog(LOG_INFO, "[portal] GlobalShortcuts portal version %u; global hotkeys enabled",
	     g_variant_is_of_type(version, G_VARIANT_TYPE_UINT32) ? g_variant_get_uint32(version) : 0);
	g_variant_unref(version);
	g_variant_unref(reply);

	portal->activated_id =
		g_dbus_connection_signal_subscribe(portal->conn, PORTAL_NAME, SHORTCUTS_IFACE, "Activated", PORTAL_PATH,
						   NULL, G_DBUS_SIGNAL_FLAGS_NONE, shortcut_signal, portal, NULL);
	portal->deactivated_id = g_dbus_connection_signal_subscribe(portal->conn, PORTAL_NAME, SHORTCUTS_IFACE,
								    "Deactivated", PORTAL_PATH, NULL,
								    G_DBUS_SIGNAL_FLAGS_NONE, shortcut_signal, portal,
								    NULL);
	portal->changed_id = g_dbus_connection_signal_subscribe(portal->conn, PORTAL_NAME, SHORTCUTS_IFACE,
								"ShortcutsChanged", PORTAL_PATH, NULL,
								G_DBUS_SIGNAL_FLAGS_NONE, shortcuts_changed, portal,
								NULL);
	portal->closed_id = g_dbus_connection_signal_subscribe(portal->conn, PORTAL_NAME, SESSION_IFACE, "Closed", NULL,
							       NULL, G_DBUS_SIGNAL_FLAGS_NONE, session_closed, portal,
							       NULL);
	portal->watch_id = g_bus_watch_name_on_connection(portal->conn, PORTAL_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE,
							  portal_appeared, portal_vanished, portal, NULL);
	return true;
}

static void disconnect_portal(struct obs_nix_portal_shortcuts *portal)
{
	if (!portal->conn)
		return;

	if (portal->watch_id)
		g_bus_unwatch_name(portal->watch_id);
	guint *ids[] = {&portal->activated_id, &portal->deactivated_id, &portal->changed_id, &portal->closed_id};
	for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
		if (*ids[i])
			g_dbus_connection_signal_unsubscribe(portal->conn, *ids[i]);
	}
	g_dbus_connection_close_sync(portal->conn, NULL, NULL);
	g_clear_object(&portal->conn);
}

static void *portal_thread(void *data)
{
	struct obs_nix_portal_shortcuts *portal = data;

	os_set_thread_name("libobs: portal shortcuts");
	g_main_context_push_thread_default(portal->context);

	if (connect_portal(portal)) {
		/* Bindings loaded before the client started. */
		portal->sync_due = true;
		while (!os_atomic_load_bool(&portal->stopping)) {
			if (portal->sync_due && !portal->portal_gone) {
				portal->sync_due = false;
				sync_shortcuts(portal);
			} else {
				g_main_context_iteration(portal->context, true);
			}
		}

		close_session(portal);
		obs_nix_hotkey_state_flush(portal->state);
	}

	if (portal->flush_source) {
		g_source_destroy(portal->flush_source);
		g_clear_pointer(&portal->flush_source, g_source_unref);
	}
	disconnect_portal(portal);
	/* Run pending unsubscribe notifications before the context goes. */
	while (g_main_context_iteration(portal->context, false))
		;

	g_main_context_pop_thread_default(portal->context);
	return NULL;
}

/* ------------------------------------------------------------------------ */
/* Binding changes                                                           */

struct sync_source {
	GSource base;
	struct obs_nix_portal_shortcuts *portal;
};

static gboolean sync_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	UNUSED_PARAMETER(callback);
	UNUSED_PARAMETER(user_data);
	g_source_set_ready_time(source, -1);
	((struct sync_source *)source)->portal->sync_due = true;
	return G_SOURCE_CONTINUE;
}

static GSourceFuncs sync_source_funcs = {NULL, NULL, sync_dispatch, NULL, NULL, NULL};

/* hotkey_bindings_changed / hotkey_unregister, on any thread: (re)start the
 * delay before the portal thread binds the new set. */
static void bindings_changed(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	struct obs_nix_portal_shortcuts *portal = data;
	g_source_set_ready_time(portal->sync_source, g_get_monotonic_time() + SYNC_DELAY_MS * G_TIME_SPAN_MILLISECOND);
}

static const char *binding_signals[] = {"hotkey_bindings_changed", "hotkey_unregister"};

static void connect_binding_signals(struct obs_nix_portal_shortcuts *portal, bool connect)
{
	signal_handler_t *signals = obs->hotkeys.signals;
	if (!signals)
		return;
	for (size_t i = 0; i < sizeof(binding_signals) / sizeof(binding_signals[0]); i++) {
		if (connect)
			signal_handler_connect(signals, binding_signals[i], bindings_changed, portal);
		else
			signal_handler_disconnect(signals, binding_signals[i], bindings_changed, portal);
	}
}

static void free_portal(struct obs_nix_portal_shortcuts *portal)
{
	g_source_destroy(portal->sync_source);
	g_source_unref(portal->sync_source);
	g_main_context_unref(portal->context);
	g_free(portal->sender);
	g_free(portal->session_path);
	da_free(portal->bound);
	da_free(portal->declined);
	bfree(portal);
}

struct obs_nix_portal_shortcuts *obs_nix_portal_shortcuts_start(struct obs_nix_hotkey_state *state,
								obs_nix_portal_key_name_func key_name)
{
	struct obs_nix_portal_shortcuts *portal = bzalloc(sizeof(*portal));
	portal->state = state;
	portal->key_name = key_name;
	portal->context = g_main_context_new();

	portal->sync_source = g_source_new(&sync_source_funcs, sizeof(struct sync_source));
	((struct sync_source *)portal->sync_source)->portal = portal;
	g_source_attach(portal->sync_source, portal->context);

	connect_binding_signals(portal, true);
	if (pthread_create(&portal->thread, NULL, portal_thread, portal) != 0) {
		blog(LOG_WARNING, "[portal] Failed to start the portal shortcuts thread");
		connect_binding_signals(portal, false);
		free_portal(portal);
		return NULL;
	}
	return portal;
}

void obs_nix_portal_shortcuts_stop(struct obs_nix_portal_shortcuts *portal)
{
	if (!portal)
		return;

	/* No signal handler runs after this returns. */
	connect_binding_signals(portal, false);

	os_atomic_set_bool(&portal->stopping, true);
	g_main_context_wakeup(portal->context);
	pthread_join(portal->thread, NULL);

	free_portal(portal);
}
