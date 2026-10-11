#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cmocka.h>

#include <gio/gio.h>
#include <X11/Xlib.h>

#include <obs.h>
#include <obs-nix-platform.h>
#include <util/platform.h>
#include <util/threading.h>

#include "screencast-portal.h"

/* obsproject/obs-studio#13809 (reported by mattpotts85): a ScreenCast portal
 * reply that is handled while the capture is being destroyed crashed OBS in
 * start(), in g_variant_new() with a NULL session handle.
 *
 * Portal replies are handled on the main loop, but libobs destroys sources on
 * its destruction thread. Cancelling the portal requests from there cannot
 * stop a reply that the main loop is already handling, so the reply runs
 * against a capture whose session is closed, or already freed.
 *
 * The test runs the real screencast portal code against a fake portal on a
 * private D-Bus bus (GTestDBus), never the user's session bus. The
 * SelectSources reply is answered at once; the source is released from inside
 * that reply, right when start() begins, and the destruction thread is held
 * just after it has closed the session. Holding only widens a window that
 * exists anyway: without it the crash happens in a fraction of the runs.
 *
 * It needs an X display for obs_startup() (xvfb-run is enough) and dbus-daemon
 * for the private bus, and exits with SKIP_EXIT without them, which ctest
 * reports as skipped. It runs in a child process, because the failure is a
 * crash. */

#define SKIP_EXIT 77

const char *obs_module_text(const char *val)
{
	return val;
}

/* ------------------------------------------------------------------ */
/* Fake org.freedesktop.portal.ScreenCast, served from its own thread  */

static const char portal_xml[] = "<node>"
				 " <interface name='org.freedesktop.portal.ScreenCast'>"
				 "  <method name='CreateSession'>"
				 "   <arg type='a{sv}' direction='in'/><arg type='o' direction='out'/>"
				 "  </method>"
				 "  <method name='SelectSources'>"
				 "   <arg type='o' direction='in'/><arg type='a{sv}' direction='in'/>"
				 "   <arg type='o' direction='out'/>"
				 "  </method>"
				 "  <method name='Start'>"
				 "   <arg type='o' direction='in'/><arg type='s' direction='in'/>"
				 "   <arg type='a{sv}' direction='in'/><arg type='o' direction='out'/>"
				 "  </method>"
				 "  <property name='AvailableSourceTypes' type='u' access='read'/>"
				 "  <property name='AvailableCursorModes' type='u' access='read'/>"
				 "  <property name='version' type='u' access='read'/>"
				 " </interface>"
				 "</node>";

struct portal {
	GMainContext *context;
	GMainLoop *loop;
	GDBusConnection *connection;
	const char *address;
	os_event_t *ready;
	pthread_t thread;
};

static char *sender_path_part(const char *sender)
{
	char *part = g_strdup(sender + 1);

	g_strdelimit(part, ".", '_');
	return part;
}

static void respond(GDBusConnection *connection, const char *sender, const char *path, GVariant *results)
{
	g_dbus_connection_emit_signal(connection, sender, path, "org.freedesktop.portal.Request", "Response",
				      g_variant_new("(u@a{sv})", 0, results), NULL);
}

static void portal_method_call(GDBusConnection *connection, const char *sender, const char *object_path,
			       const char *interface_name, const char *method_name, GVariant *parameters,
			       GDBusMethodInvocation *invocation, gpointer user_data)
{
	g_autoptr(GVariant) options = NULL;
	g_autofree char *sender_part = sender_path_part(sender);
	g_autofree char *request_path = NULL;
	const char *handle_token = NULL;
	GVariantBuilder results;

	UNUSED_PARAMETER(object_path);
	UNUSED_PARAMETER(interface_name);
	UNUSED_PARAMETER(user_data);

	options = g_variant_get_child_value(parameters, g_variant_n_children(parameters) - 1);
	g_variant_lookup(options, "handle_token", "&s", &handle_token);
	request_path = g_strdup_printf("/org/freedesktop/portal/desktop/request/%s/%s", sender_part, handle_token);
	g_dbus_method_invocation_return_value(invocation, g_variant_new("(o)", request_path));

	g_variant_builder_init(&results, G_VARIANT_TYPE_VARDICT);
	if (strcmp(method_name, "CreateSession") == 0) {
		const char *session_token = NULL;
		g_autofree char *session_path = NULL;

		g_variant_lookup(options, "session_handle_token", "&s", &session_token);
		session_path =
			g_strdup_printf("/org/freedesktop/portal/desktop/session/%s/%s", sender_part, session_token);
		g_variant_builder_add(&results, "{sv}", "session_handle", g_variant_new_string(session_path));
	} else if (strcmp(method_name, "Start") == 0) {
		GVariantBuilder streams;

		g_variant_builder_init(&streams, G_VARIANT_TYPE("a(ua{sv})"));
		g_variant_builder_add(&streams, "(u@a{sv})", 4242,
				      g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0));
		g_variant_builder_add(&results, "{sv}", "streams", g_variant_builder_end(&streams));
	}
	respond(connection, sender, request_path, g_variant_builder_end(&results));
}

static GVariant *portal_get_property(GDBusConnection *connection, const char *sender, const char *object_path,
				     const char *interface_name, const char *property_name, GError **error,
				     gpointer user_data)
{
	UNUSED_PARAMETER(connection);
	UNUSED_PARAMETER(sender);
	UNUSED_PARAMETER(object_path);
	UNUSED_PARAMETER(interface_name);
	UNUSED_PARAMETER(error);
	UNUSED_PARAMETER(user_data);

	if (strcmp(property_name, "AvailableSourceTypes") == 0)
		return g_variant_new_uint32(3);
	if (strcmp(property_name, "AvailableCursorModes") == 0)
		return g_variant_new_uint32(1);
	return g_variant_new_uint32(4);
}

static const GDBusInterfaceVTable portal_vtable = {
	.method_call = portal_method_call,
	.get_property = portal_get_property,
};

static void *portal_thread(void *data)
{
	struct portal *portal = data;
	g_autoptr(GDBusNodeInfo) node = NULL;
	g_autoptr(GVariant) reply = NULL;

	g_main_context_push_thread_default(portal->context);

	portal->connection = g_dbus_connection_new_for_address_sync(
		portal->address,
		G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, NULL,
		NULL, NULL);
	node = g_dbus_node_info_new_for_xml(portal_xml, NULL);
	g_dbus_connection_register_object(portal->connection, "/org/freedesktop/portal/desktop", node->interfaces[0],
					  &portal_vtable, NULL, NULL, NULL);
	reply = g_dbus_connection_call_sync(portal->connection, "org.freedesktop.DBus", "/org/freedesktop/DBus",
					    "org.freedesktop.DBus", "RequestName",
					    g_variant_new("(su)", "org.freedesktop.portal.Desktop", 0), NULL,
					    G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
	os_event_signal(portal->ready);

	g_main_loop_run(portal->loop);

	g_main_context_pop_thread_default(portal->context);
	return NULL;
}

/* ------------------------------------------------------------------ */
/* The capture under test                                             */

static pthread_t main_thread;
static obs_source_t *capture_source;
static os_event_t *destroy_reached_cancel;

static void log_handler(int level, const char *format, va_list args, void *param)
{
	bool on_main = pthread_equal(pthread_self(), main_thread);
	char message[1024];

	UNUSED_PARAMETER(param);

	vsnprintf(message, sizeof(message), format, args);
	if (level <= LOG_WARNING || getenv("PORTAL_TEST_VERBOSE"))
		fprintf(stderr, "%s\n", message);

	if (on_main && capture_source && strstr(message, "[pipewire] Asking for")) {
		/* start() has begun handling the SelectSources reply: drop the
		 * last reference, so libobs destroys the capture on its
		 * destruction thread, and let start() go on once that has
		 * closed the session. */
		obs_source_t *source = capture_source;

		capture_source = NULL;
		obs_source_release(source);
		os_event_timedwait(destroy_reached_cancel, 1000);
	} else if (!on_main && strstr(message, "[portals] Request cancelled")) {
		/* destroy() is cancelling the portal requests: hold it here
		 * for a moment, with the session already closed. */
		os_event_signal(destroy_reached_cancel);
		os_sleep_ms(300);
	}
}

static void iterate_main_loop(uint32_t ms)
{
	uint64_t end = os_gettime_ns() + ms * 1000000ULL;

	while (os_gettime_ns() < end) {
		while (g_main_context_iteration(NULL, FALSE))
			;
		os_sleep_ms(1);
	}
}

static int run_scenario(void)
{
	struct portal portal = {0};
	/* Not torn down: the portal code keeps its bus connection, which
	 * g_test_dbus_down() would wait for. The child exits instead, and
	 * GTestDBus stops the bus when it does. */
	GTestDBus *bus;
	g_autofree char *dbus_daemon = g_find_program_in_path("dbus-daemon");
	g_autofree char *display_name = g_strdup(getenv("DISPLAY"));
	Display *display;

	if (!display_name || !dbus_daemon) {
		fprintf(stderr, "skipped: needs an X display and dbus-daemon\n");
		return SKIP_EXIT;
	}

	/* A private bus: never the user's session bus. Bringing it up unsets
	 * DISPLAY, which obs_startup() needs. */
	bus = g_test_dbus_new(G_TEST_DBUS_NONE);
	g_test_dbus_up(bus);
	g_setenv("DISPLAY", display_name, TRUE);

	portal.address = g_test_dbus_get_bus_address(bus);
	portal.context = g_main_context_new();
	portal.loop = g_main_loop_new(portal.context, FALSE);
	os_event_init(&portal.ready, OS_EVENT_TYPE_MANUAL);
	pthread_create(&portal.thread, NULL, portal_thread, &portal);
	os_event_wait(portal.ready);

	display = XOpenDisplay(NULL);
	if (!display) {
		fprintf(stderr, "skipped: cannot open the X display\n");
		return SKIP_EXIT;
	}

	main_thread = pthread_self();
	os_event_init(&destroy_reached_cancel, OS_EVENT_TYPE_AUTO);
	base_set_log_handler(log_handler, NULL);

	obs_set_nix_platform(OBS_NIX_PLATFORM_X11_EGL);
	obs_set_nix_platform_display(display);
	if (!obs_startup("en-US", NULL, NULL)) {
		fprintf(stderr, "skipped: obs_startup failed\n");
		return SKIP_EXIT;
	}

	screencast_portal_load();

	capture_source = obs_source_create("pipewire-screen-capture-source", "capture", NULL, NULL);
	if (!capture_source) {
		fprintf(stderr, "the screen capture source was not registered\n");
		return 1;
	}

	/* CreateSession, SelectSources, Start, OpenPipeWireRemote. */
	iterate_main_loop(1500);
	if (capture_source) {
		fprintf(stderr, "the SelectSources reply was never handled\n");
		return 1;
	}

	obs_wait_for_destroy_queue();
	iterate_main_loop(200);

	obs_shutdown();
	screencast_portal_unload();
	return 0;
}

static void reply_during_destroy_is_dropped(void **state)
{
	UNUSED_PARAMETER(state);
	int status = 0;
	pid_t pid;

	fflush(stderr);
	pid = fork();
	assert_true(pid >= 0);
	if (pid == 0) {
		/* Report a crash as a signal, not through cmocka's handlers. */
		signal(SIGSEGV, SIG_DFL);
		signal(SIGABRT, SIG_DFL);
		signal(SIGBUS, SIG_DFL);
		/* A hang fails too. */
		signal(SIGALRM, SIG_DFL);
		alarm(20);
		_exit(run_scenario());
	}

	assert_int_equal(waitpid(pid, &status, 0), pid);
	if (WIFEXITED(status) && WEXITSTATUS(status) == SKIP_EXIT)
		skip();

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		fprintf(stderr, "a portal reply handled during destroy %s %d\n",
			WIFSIGNALED(status) ? "crashed with signal" : "failed with exit status",
			WIFSIGNALED(status) ? WTERMSIG(status) : WEXITSTATUS(status));
		fail();
	}
}

static bool have_prerequisites(void)
{
	g_autofree char *dbus_daemon = g_find_program_in_path("dbus-daemon");
	Display *display;

	if (!dbus_daemon) {
		fprintf(stderr, "skipped: needs dbus-daemon for a private bus\n");
		return false;
	}

	display = getenv("DISPLAY") ? XOpenDisplay(NULL) : NULL;
	if (!display) {
		fprintf(stderr, "skipped: needs an X display (run under xvfb-run)\n");
		return false;
	}
	XCloseDisplay(display);
	return true;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(reply_during_destroy_is_dropped),
	};

	if (!have_prerequisites())
		return SKIP_EXIT;

	return cmocka_run_group_tests(tests, NULL, NULL);
}
