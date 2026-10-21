/* The original session D-Bus inhibitor, with only its external GIO calls
 * replaced by deterministic stand-ins. This lets Tier 3 compare service
 * selection, variant arguments, cookies, retries and log lines. */
#if defined(__linux__) || defined(__FreeBSD__)

#include "libobs/build/libobs-nix-dbus.pch.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

struct dbus_sleep_info;
struct dbus_sleep_info *oracle_dbus_sleep_info_create(void);
void oracle_dbus_inhibit_sleep(struct dbus_sleep_info *info, const char *reason, bool active);
void oracle_dbus_sleep_info_destroy(struct dbus_sleep_info *info);

GDBusConnection *oracle_dbus_bus_get_sync(GBusType bus_type, GCancellable *cancellable, GError **error);
GVariant *oracle_dbus_call_sync(GDBusConnection *connection, const gchar *destination, const gchar *object_path,
				const gchar *interface_name, const gchar *method_name, GVariant *parameters,
				const GVariantType *reply_type, GDBusCallFlags flags, gint timeout_msec,
				GCancellable *cancellable, GError **error);
void oracle_dbus_blog(int level, const char *format, ...) __attribute__((format(printf, 2, 3)));

#define dbus_sleep_info_create oracle_dbus_sleep_info_create
#define dbus_inhibit_sleep oracle_dbus_inhibit_sleep
#define dbus_sleep_info_destroy oracle_dbus_sleep_info_destroy
#define g_bus_get_sync oracle_dbus_bus_get_sync
#define g_dbus_connection_call_sync oracle_dbus_call_sync
#define blog oracle_dbus_blog

#include "libobs/util/platform-nix-dbus.c.h"

#undef dbus_sleep_info_create
#undef dbus_inhibit_sleep
#undef dbus_sleep_info_destroy
#undef g_bus_get_sync
#undef g_dbus_connection_call_sync
#undef blog

static unsigned int od_owned_mask;
static bool od_fail_connection;
static bool od_fail_call;
static uint32_t od_cookie;
static char od_trace[8192];
static size_t od_trace_len;

void oracle_dbus_stub_config(unsigned int owned_mask, bool fail_connection, bool fail_call, uint32_t cookie);
size_t oracle_dbus_stub_take_trace(char *out, size_t capacity);

static void od_append(const char *format, ...) __attribute__((format(printf, 1, 2)));
static void od_append(const char *format, ...)
{
	if (od_trace_len >= sizeof(od_trace) - 1)
		return;
	va_list args;
	va_start(args, format);
	int count = vsnprintf(od_trace + od_trace_len, sizeof(od_trace) - od_trace_len, format, args);
	va_end(args);
	if (count < 0)
		return;
	size_t written = (size_t)count;
	if (written >= sizeof(od_trace) - od_trace_len)
		written = sizeof(od_trace) - od_trace_len - 1;
	od_trace_len += written;
}

void oracle_dbus_stub_config(unsigned int owned_mask, bool fail_connection, bool fail_call, uint32_t cookie)
{
	od_owned_mask = owned_mask;
	od_fail_connection = fail_connection;
	od_fail_call = fail_call;
	od_cookie = cookie;
	od_trace_len = 0;
	od_trace[0] = '\0';
}

size_t oracle_dbus_stub_take_trace(char *out, size_t capacity)
{
	size_t length = od_trace_len;
	if (out && capacity) {
		size_t amount = length < capacity - 1 ? length : capacity - 1;
		memcpy(out, od_trace, amount);
		out[amount] = '\0';
	}
	od_trace_len = 0;
	od_trace[0] = '\0';
	return length;
}

GDBusConnection *oracle_dbus_bus_get_sync(GBusType bus_type, GCancellable *cancellable, GError **error)
{
	(void)cancellable;
	od_append("connect %d\n", (int)bus_type);
	if (od_fail_connection) {
		g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "no session bus");
		return NULL;
	}
	/* The C code only refs/unrefs this object; the calls below are replaced. */
	return (GDBusConnection *)g_object_new(G_TYPE_OBJECT, NULL);
}

GVariant *oracle_dbus_call_sync(GDBusConnection *connection, const gchar *destination, const gchar *object_path,
				const gchar *interface_name, const gchar *method_name, GVariant *parameters,
				const GVariantType *reply_type, GDBusCallFlags flags, gint timeout_msec,
				GCancellable *cancellable, GError **error)
{
	(void)connection;
	(void)object_path;
	(void)interface_name;
	(void)reply_type;
	(void)timeout_msec;
	(void)cancellable;
	if (strcmp(method_name, "GetNameOwner") == 0) {
		const char *name = NULL;
		g_variant_get(parameters, "(&s)", &name);
		od_append("owner %s %d\n", name, (int)flags);
		for (size_t i = 0; i < num_services; i++) {
			if ((od_owned_mask & (1u << i)) && strcmp(name, services[i].name) == 0)
				return g_variant_new("(s)", ":1.1");
		}
		return NULL;
	}
	if (strcmp(method_name, "Inhibit") == 0) {
		const char *program = NULL;
		const char *reason = NULL;
		if (strcmp(destination, services[MATE_SM].name) == 0 ||
		    strcmp(destination, services[GNOME_SM].name) == 0) {
			uint32_t xid = 0, inhibit_flags = 0;
			g_variant_get(parameters, "(&su&su)", &program, &xid, &reason, &inhibit_flags);
			od_append("inhibit %s %s %s %u %u\n", destination, program, reason, xid, inhibit_flags);
		} else {
			g_variant_get(parameters, "(&s&s)", &program, &reason);
			od_append("inhibit %s %s %s\n", destination, program, reason);
		}
	} else {
		uint32_t cookie = 0;
		g_variant_get(parameters, "(u)", &cookie);
		od_append("uninhibit %s %s %u\n", destination, method_name, cookie);
	}
	if (od_fail_call) {
		g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "call failed");
		return NULL;
	}
	return strcmp(method_name, "Inhibit") == 0 ? g_variant_new("(u)", od_cookie) : g_variant_new("()");
}

void oracle_dbus_blog(int level, const char *format, ...)
{
	char message[512];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	od_append("log %d %s\n", level, message);
}

#else
typedef int oracle_dbus_inhibit_unused;
#endif
