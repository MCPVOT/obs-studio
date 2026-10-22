/* The original portal inhibitor, with its GIO calls replaced by
 * deterministic stand-ins. Tier 3 compares the trace with the Rust core.
 * Real GLib variants, cancellables and reference counts. No session bus.
 */
#if defined(__linux__) || defined(__FreeBSD__)

#include "libobs/build/libobs-nix-dbus.pch.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

struct portal_inhibit_info;
struct portal_inhibit_info *oracle_portal_inhibit_info_create(void);
void oracle_portal_inhibit(struct portal_inhibit_info *info, const char *reason, bool active);
void oracle_portal_inhibit_info_destroy(struct portal_inhibit_info *info);
void oracle_portal_stub_config(bool bus_missing, bool portal_missing);
size_t oracle_portal_stub_take_trace(char *out, size_t capacity);
void oracle_portal_complete(bool error);
void oracle_portal_respond(unsigned int code);

static GDBusConnection *oracle_portal_bus_get(GBusType bus_type, GCancellable *cancellable, GError **error);
static const char *oracle_portal_unique_name(GDBusConnection *connection);
static GVariant *oracle_portal_call_sync(GDBusConnection *connection, const char *destination, const char *object_path,
					 const char *interface_name, const char *method_name, GVariant *parameters,
					 const GVariantType *reply_type, GDBusCallFlags flags, int timeout_msec,
					 GCancellable *cancellable, GError **error);
static unsigned int oracle_portal_subscribe(GDBusConnection *connection, const char *sender, const char *interface_name,
					    const char *member, const char *object_path, const char *arg0,
					    GDBusSignalFlags flags, GDBusSignalCallback callback, void *user_data,
					    GDestroyNotify free_func);
static void oracle_portal_unsubscribe(GDBusConnection *connection, unsigned int id);
static void oracle_portal_call(GDBusConnection *connection, const char *destination, const char *object_path,
			       const char *interface_name, const char *method_name, GVariant *parameters,
			       const GVariantType *reply_type, GDBusCallFlags flags, int timeout_msec,
			       GCancellable *cancellable, GAsyncReadyCallback callback, void *user_data);
static GVariant *oracle_portal_finish(GDBusConnection *connection, GAsyncResult *result, GError **error);
static uint32_t oracle_portal_rand(void);
static void oracle_portal_blog(int level, const char *format, ...) __attribute__((format(printf, 2, 3)));
static void oracle_portal_cancel(GCancellable *cancellable);

void oracle_dstr_printf(struct dstr *dst, const char *format, ...) __attribute__((format(printf, 2, 3)));
#define dstr_printf oracle_dstr_printf

#define portal_inhibit_info_create oracle_portal_inhibit_info_create
#define portal_inhibit oracle_portal_inhibit
#define portal_inhibit_info_destroy oracle_portal_inhibit_info_destroy
#define g_bus_get_sync oracle_portal_bus_get
#define g_dbus_connection_get_unique_name oracle_portal_unique_name
#define g_dbus_connection_call_sync oracle_portal_call_sync
#define g_dbus_connection_signal_subscribe oracle_portal_subscribe
#define g_dbus_connection_signal_unsubscribe oracle_portal_unsubscribe
#define g_dbus_connection_call oracle_portal_call
#define g_dbus_connection_call_finish oracle_portal_finish
#ifdef rand
#undef rand
#endif
#define rand oracle_portal_rand
#define blog oracle_portal_blog
#define g_cancellable_cancel oracle_portal_cancel

#include "libobs/util/platform-nix-portal.c.h"

#undef dstr_printf
#undef portal_inhibit_info_create
#undef portal_inhibit
#undef portal_inhibit_info_destroy
#undef g_bus_get_sync
#undef g_dbus_connection_get_unique_name
#undef g_dbus_connection_call_sync
#undef g_dbus_connection_signal_subscribe
#undef g_dbus_connection_signal_unsubscribe
#undef g_dbus_connection_call
#undef g_dbus_connection_call_finish
#undef rand
#undef blog
#undef g_cancellable_cancel

struct pending_call {
	GAsyncReadyCallback callback;
	void *data;
	GCancellable *cancel;
};

static bool op_bus_missing;
static bool op_portal_missing;
static unsigned int op_next_id = 7;
static GDBusConnection *op_connection;
static GDBusSignalCallback op_response;
static void *op_response_data;
static GDestroyNotify op_response_free;
static struct pending_call op_calls[8];
static size_t op_pending;
static bool op_finish_failed;
static int op_finish_error;
static char op_trace[8192];
static size_t op_trace_len;

static void op_append(const char *format, ...) __attribute__((format(printf, 1, 2)));
static void op_append(const char *format, ...)
{
	if (op_trace_len >= sizeof(op_trace) - 1)
		return;
	va_list args;
	va_start(args, format);
	int count = vsnprintf(op_trace + op_trace_len, sizeof(op_trace) - op_trace_len, format, args);
	va_end(args);
	if (count < 0)
		return;
	size_t written = (size_t)count;
	if (written >= sizeof(op_trace) - op_trace_len)
		written = sizeof(op_trace) - op_trace_len - 1;
	op_trace_len += written;
}

void oracle_portal_stub_config(bool bus_missing, bool portal_missing)
{
	op_bus_missing = bus_missing;
	op_portal_missing = portal_missing;
	op_next_id = 7;
	op_connection = NULL;
	op_response = NULL;
	op_response_data = NULL;
	op_response_free = NULL;
	op_pending = 0;
	op_finish_failed = false;
	op_finish_error = 0;
	op_trace_len = 0;
	op_trace[0] = '\0';
}

size_t oracle_portal_stub_take_trace(char *out, size_t capacity)
{
	size_t length = op_trace_len;
	if (out && capacity) {
		size_t amount = length < capacity - 1 ? length : capacity - 1;
		memcpy(out, op_trace, amount);
		out[amount] = '\0';
	}
	op_trace_len = 0;
	op_trace[0] = '\0';
	return length;
}

static GDBusConnection *oracle_portal_bus_get(GBusType bus_type, GCancellable *cancellable, GError **error)
{
	(void)cancellable;
	op_append("bus %d\n", (int)bus_type);
	if (op_bus_missing) {
		g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "no session bus");
		return NULL;
	}
	/* The C code only refs and unrefs this. The calls below are replaced. */
	op_connection = (GDBusConnection *)g_object_new(G_TYPE_OBJECT, NULL);
	return op_connection;
}

static const char *oracle_portal_unique_name(GDBusConnection *connection)
{
	(void)connection;
	op_append("unique %s\n", ":1.42");
	return ":1.42";
}

static GVariant *oracle_portal_call_sync(GDBusConnection *connection, const char *destination, const char *object_path,
					 const char *interface_name, const char *method_name, GVariant *parameters,
					 const GVariantType *reply_type, GDBusCallFlags flags, int timeout_msec,
					 GCancellable *cancellable, GError **error)
{
	(void)connection;
	(void)reply_type;
	(void)cancellable;
	(void)error;
	const char *requested = "";
	const char *property = "";
	g_variant_get(parameters, "(&s&s)", &requested, &property);
	op_append("get %s %s %s %s %s %s %d %d\n", destination, object_path, interface_name, method_name, requested,
		  property, (int)flags, timeout_msec);
	g_variant_unref(g_variant_ref_sink(parameters));
	if (op_portal_missing)
		return NULL;
	return g_variant_ref_sink(g_variant_new("(v)", g_variant_new_uint32(3)));
}

static unsigned int oracle_portal_subscribe(GDBusConnection *connection, const char *sender, const char *interface_name,
					    const char *member, const char *object_path, const char *arg0,
					    GDBusSignalFlags flags, GDBusSignalCallback callback, void *user_data,
					    GDestroyNotify free_func)
{
	(void)connection;
	(void)arg0;
	op_append("subscribe %s %s %s %s %u\n", sender, interface_name, member, object_path, (unsigned)flags);
	op_response = callback;
	op_response_data = user_data;
	op_response_free = free_func;
	return 17;
}

static void oracle_portal_unsubscribe(GDBusConnection *connection, unsigned int id)
{
	(void)connection;
	op_append("unsubscribe %u\n", id);
	GDBusSignalCallback callback = op_response;
	void *data = op_response_data;
	GDestroyNotify free_func = op_response_free;
	op_response = NULL;
	op_response_data = NULL;
	op_response_free = NULL;
	(void)callback;
	if (free_func)
		free_func(data);
}

static void oracle_portal_call(GDBusConnection *connection, const char *destination, const char *object_path,
			       const char *interface_name, const char *method_name, GVariant *parameters,
			       const GVariantType *reply_type, GDBusCallFlags flags, int timeout_msec,
			       GCancellable *cancellable, GAsyncReadyCallback callback, void *user_data)
{
	(void)connection;
	(void)reply_type;
	g_variant_ref_sink(parameters);
	if (strcmp(method_name, "Close") == 0) {
		op_append("close %s %s %s %s %d %d\n", destination, object_path, interface_name, method_name,
			  (int)flags, timeout_msec);
	} else {
		const char *window = "";
		unsigned int inhibit_flags = 0;
		GVariant *options = NULL;
		g_variant_get(parameters, "(&su@a{sv})", &window, &inhibit_flags, &options);
		const char *token = "";
		const char *reason = "";
		if (options) {
			g_variant_lookup(options, "handle_token", "&s", &token);
			g_variant_lookup(options, "reason", "&s", &reason);
			g_variant_unref(options);
		}
		op_append("inhibit %s %s %s %s %d %d [%s] %u [%s] [%s]\n", destination, object_path, interface_name,
			  method_name, (int)flags, timeout_msec, window, inhibit_flags, token, reason);
	}
	g_variant_unref(parameters);
	if (op_pending < sizeof(op_calls) / sizeof(op_calls[0])) {
		op_calls[op_pending].callback = callback;
		op_calls[op_pending].data = user_data;
		op_calls[op_pending].cancel = cancellable ? g_object_ref(cancellable) : NULL;
		op_pending++;
	}
}

static GVariant *oracle_portal_finish(GDBusConnection *connection, GAsyncResult *result, GError **error)
{
	(void)connection;
	(void)result;
	if (op_finish_failed) {
		g_set_error_literal(error, G_IO_ERROR, op_finish_error, "method failed");
		return NULL;
	}
	return g_variant_ref_sink(g_variant_new("()"));
}

static uint32_t oracle_portal_rand(void)
{
	uint32_t id = op_next_id++;
	op_append("rand %u\n", id);
	return id;
}

static void oracle_portal_blog(int level, const char *format, ...)
{
	char message[512];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	op_append("log %d %s\n", level, message);
}

static void oracle_portal_cancel(GCancellable *cancellable)
{
	op_append("cancel\n");
	g_cancellable_cancel(cancellable);
}

void oracle_portal_complete(bool error)
{
	if (op_pending == 0)
		return;
	struct pending_call call = op_calls[0];
	memmove(op_calls, op_calls + 1, --op_pending * sizeof(call));
	bool cancelled = call.cancel && g_cancellable_is_cancelled(call.cancel);
	op_finish_error = cancelled ? G_IO_ERROR_CANCELLED : G_IO_ERROR_FAILED;
	op_finish_failed = error || cancelled;
	/* NULL source: finish ignores it, and destroy may already have unref'd
	 * the stand-in connection. */
	call.callback(NULL, NULL, call.data);
	g_clear_object(&call.cancel);
}

void oracle_portal_respond(unsigned int code)
{
	if (!op_response)
		return;
	GVariantBuilder results;
	g_variant_builder_init(&results, G_VARIANT_TYPE_VARDICT);
	GVariant *parameters = g_variant_ref_sink(g_variant_new("(ua{sv})", code, &results));
	op_response(NULL, NULL, NULL, NULL, NULL, parameters, op_response_data);
	g_variant_unref(parameters);
}

#else
typedef int oracle_portal_inhibit_unused;
#endif
