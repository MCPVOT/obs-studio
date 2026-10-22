//! GIO host and C ABI for `util/platform-nix-portal.c.h` (#1631).
//! The request state lives in the safe core. This module owns the session
//! connection, the cancellable, and the GLib callbacks.
//!
//! The opaque handle is a Rust `Box` (C uses `bzalloc`). A NULL reason
//! becomes an empty string; `g_variant_new_string` does not accept NULL.

use core::ffi::{CStr, c_char, c_int, c_void};
use core::ptr::{self, NonNull};
use std::ffi::CString;
use std::sync::{Arc, Mutex};

use crate::platform::unix::portal_inhibit::{
    CALL_FLAGS_NONE, CALL_TIMEOUT_INFINITE, CallResult, Host, PORTAL_NAME, PORTAL_PATH,
    PROPERTIES_IFACE, Portal,
};

/// `G_IO_ERROR_CANCELLED` in `gio/gioenums.h`.
const IO_ERROR_CANCELLED: c_int = 19;

#[repr(C)]
struct GError {
    domain: u32,
    code: c_int,
    message: *mut c_char,
}

unsafe extern "C" {
    fn g_bus_get_sync(
        bus_type: c_int,
        cancellable: *mut c_void,
        error: *mut *mut GError,
    ) -> *mut c_void;
    fn g_dbus_connection_get_unique_name(connection: *mut c_void) -> *const c_char;
    fn g_dbus_connection_call_sync(
        connection: *mut c_void,
        destination: *const c_char,
        object_path: *const c_char,
        interface_name: *const c_char,
        method_name: *const c_char,
        parameters: *mut c_void,
        reply_type: *const c_void,
        flags: c_int,
        timeout_msec: c_int,
        cancellable: *mut c_void,
        error: *mut *mut GError,
    ) -> *mut c_void;
    fn g_dbus_connection_call(
        connection: *mut c_void,
        destination: *const c_char,
        object_path: *const c_char,
        interface_name: *const c_char,
        method_name: *const c_char,
        parameters: *mut c_void,
        reply_type: *const c_void,
        flags: c_int,
        timeout_msec: c_int,
        cancellable: *mut c_void,
        callback: Option<unsafe extern "C" fn(*mut c_void, *mut c_void, *mut c_void)>,
        user_data: *mut c_void,
    );
    fn g_dbus_connection_call_finish(
        connection: *mut c_void,
        result: *mut c_void,
        error: *mut *mut GError,
    ) -> *mut c_void;
    fn g_dbus_connection_signal_subscribe(
        connection: *mut c_void,
        sender: *const c_char,
        interface_name: *const c_char,
        member: *const c_char,
        object_path: *const c_char,
        arg0: *const c_char,
        flags: u32,
        callback: Option<
            unsafe extern "C" fn(
                *mut c_void,
                *const c_char,
                *const c_char,
                *const c_char,
                *const c_char,
                *mut c_void,
                *mut c_void,
            ),
        >,
        user_data: *mut c_void,
        user_data_free: Option<unsafe extern "C" fn(*mut c_void)>,
    ) -> u32;
    fn g_dbus_connection_signal_unsubscribe(connection: *mut c_void, id: u32);
    fn g_cancellable_new() -> *mut c_void;
    fn g_cancellable_cancel(cancellable: *mut c_void);
    fn g_variant_new(format: *const c_char, ...) -> *mut c_void;
    fn g_variant_new_string(string: *const c_char) -> *mut c_void;
    fn g_variant_get(value: *mut c_void, format: *const c_char, ...);
    fn g_variant_unref(value: *mut c_void);
    fn g_variant_type_new(type_string: *const c_char) -> *mut c_void;
    fn g_variant_type_free(type_: *mut c_void);
    fn g_variant_builder_new(type_: *const c_void) -> *mut c_void;
    fn g_variant_builder_add(builder: *mut c_void, format: *const c_char, ...);
    fn g_variant_builder_unref(builder: *mut c_void);
    fn g_object_unref(object: *mut c_void);
    fn g_error_free(error: *mut GError);
    fn g_error_matches(error: *const GError, domain: u32, code: c_int) -> i32;
    fn g_io_error_quark() -> u32;
    fn blog(log_level: c_int, format: *const c_char, ...);
}

/// Session bus connection. One reference, dropped once.
pub struct Connection(NonNull<c_void>);

// SAFETY: `GDBusConnection` is safe to use from more than one thread. Drop
// unrefs the single reference this handle owns.
unsafe impl Send for Connection {}
unsafe impl Sync for Connection {}

impl Drop for Connection {
    fn drop(&mut self) {
        // SAFETY: `self.0` came from `g_bus_get_sync` and owns one reference.
        unsafe { g_object_unref(self.0.as_ptr()) };
    }
}

fn c_string(text: &str) -> CString {
    CString::new(text).unwrap_or_default()
}

/// Reads and frees the GError from one failed GIO call.
unsafe fn take_error(error: *mut GError) -> String {
    if error.is_null() {
        return "unknown D-Bus error".into();
    }
    // SAFETY: GIO returns a live GError with a NUL-terminated message.
    let message = unsafe { CStr::from_ptr((*error).message) }
        .to_string_lossy()
        .into_owned();
    // SAFETY: ownership of the GError is ours after the call.
    unsafe { g_error_free(error) };
    message
}

fn finish_call(source: *mut c_void, result: *mut c_void) -> CallResult {
    let mut error = ptr::null_mut();
    // SAFETY: `source` is the connection GIO passed the callback, and
    // `result` is the matching async result.
    let reply = unsafe { g_dbus_connection_call_finish(source, result, &raw mut error) };
    if !reply.is_null() {
        // SAFETY: a non-NULL finish reply owns one reference.
        unsafe { g_variant_unref(reply) };
    }
    if error.is_null() {
        return CallResult::Ok;
    }
    // SAFETY: `error` is the GError GIO just returned.
    let cancelled = unsafe { g_error_matches(error, g_io_error_quark(), IO_ERROR_CANCELLED) } != 0;
    // SAFETY: the same GError, freed inside `take_error`.
    let message = unsafe { take_error(error) };
    if cancelled {
        CallResult::Cancelled
    } else {
        CallResult::Failed(message)
    }
}

fn response_code(parameters: *mut c_void) -> u32 {
    let mut code = 0u32;
    // `g_variant_get` is variadic, so this out-pointer has no inferred type.
    let mut dict: *mut c_void = ptr::null_mut();
    // SAFETY: the signal parameters are the portal's `(ua{sv})` tuple.
    // `@a{sv}` returns one owned reference.
    unsafe {
        g_variant_get(
            parameters,
            c"(u@a{sv})".as_ptr(),
            &raw mut code,
            &raw mut dict,
        );
        if !dict.is_null() {
            g_variant_unref(dict);
        }
    }
    code
}

struct SignalCtx {
    on_response: Box<dyn Fn(u32)>,
}

unsafe extern "C" fn signal_cb(
    _connection: *mut c_void,
    _sender: *const c_char,
    _path: *const c_char,
    _interface: *const c_char,
    _member: *const c_char,
    parameters: *mut c_void,
    user_data: *mut c_void,
) {
    // SAFETY: `user_data` is the `Arc<SignalCtx>` passed to subscribe. This
    // borrows one count for the call and puts the GIO-owned count back, so
    // unsubscribe can free it from inside the callback.
    let ctx = unsafe { Arc::from_raw(user_data.cast::<SignalCtx>()) };
    let keep = Arc::clone(&ctx);
    let _restore = Arc::into_raw(ctx);
    (keep.on_response)(response_code(parameters));
}

unsafe extern "C" fn signal_free(user_data: *mut c_void) {
    // SAFETY: GIO calls this once, with the pointer from subscribe.
    drop(unsafe { Arc::from_raw(user_data.cast::<SignalCtx>()) });
}

struct CallCtx {
    cancellable: Option<Arc<Mutex<Option<usize>>>>,
    on_done: Box<dyn Fn(CallResult)>,
}

unsafe extern "C" fn call_cb(source: *mut c_void, result: *mut c_void, user_data: *mut c_void) {
    // SAFETY: `user_data` is the `Box<CallCtx>` from the call. GIO invokes
    // this once and does not free it.
    let ctx = unsafe { Box::from_raw(user_data.cast::<CallCtx>()) };
    if let Some(slot) = &ctx.cancellable
        && let Some(bits) = slot.lock().expect("cancellable").take()
    {
        // SAFETY: the bits are the `g_cancellable_new` pointer stored below.
        unsafe { g_object_unref(bits as *mut c_void) };
    }
    (ctx.on_done)(finish_call(source, result));
}

/// Production host. One inhibitor owns one of these.
pub struct GioHost {
    cancellable: Arc<Mutex<Option<usize>>>,
}

impl GioHost {
    fn new() -> Self {
        Self {
            cancellable: Arc::new(Mutex::new(None)),
        }
    }
}

impl Host for GioHost {
    type Conn = Connection;

    fn bus_get(&self, bus_type: i32) -> Result<Connection, String> {
        let mut error = ptr::null_mut();
        // SAFETY: NULL cancellable and one out-pointer, as `g_bus_get_sync` takes them.
        let connection = unsafe { g_bus_get_sync(bus_type, ptr::null_mut(), &raw mut error) };
        match NonNull::new(connection) {
            Some(connection) => Ok(Connection(connection)),
            // SAFETY: GIO owns the error returned on failure.
            None => Err(unsafe { take_error(error) }),
        }
    }

    fn unique_name(&self, conn: &Connection) -> String {
        // SAFETY: `conn` is a live session connection.
        let name = unsafe { g_dbus_connection_get_unique_name(conn.0.as_ptr()) };
        if name.is_null() {
            return String::new();
        }
        // SAFETY: GIO returns a NUL-terminated unique name owned by the connection.
        unsafe { CStr::from_ptr(name) }
            .to_string_lossy()
            .into_owned()
    }

    fn properties_get(&self, conn: &Connection, interface: &str, property: &str) -> bool {
        let interface = c_string(interface);
        let property = c_string(property);
        let destination = c_string(PORTAL_NAME);
        let path = c_string(PORTAL_PATH);
        let iface = c_string(PROPERTIES_IFACE);
        // SAFETY: `(ss)` copies the two C strings. GIO consumes the floating variant.
        let params =
            unsafe { g_variant_new(c"(ss)".as_ptr(), interface.as_ptr(), property.as_ptr()) };
        let reply_type = unsafe { g_variant_type_new(c"(v)".as_ptr()) };
        // SAFETY: the connection and the static names are live. A NULL error
        // pointer matches the C file: a missing portal is a NULL reply.
        let reply = unsafe {
            g_dbus_connection_call_sync(
                conn.0.as_ptr(),
                destination.as_ptr(),
                path.as_ptr(),
                iface.as_ptr(),
                c"Get".as_ptr(),
                params,
                reply_type,
                CALL_FLAGS_NONE,
                CALL_TIMEOUT_INFINITE,
                ptr::null_mut(),
                ptr::null_mut(),
            )
        };
        // SAFETY: `reply_type` is the type allocated above.
        unsafe { g_variant_type_free(reply_type) };
        if reply.is_null() {
            false
        } else {
            // SAFETY: a non-NULL sync reply owns one reference.
            unsafe { g_variant_unref(reply) };
            true
        }
    }

    fn request_id(&self) -> u32 {
        // SAFETY: `rand` has no state this process shares with a lock. The C
        // file calls it the same way.
        unsafe { libc::rand() as u32 }
    }

    fn subscribe(
        &self,
        conn: &Connection,
        sender: &str,
        interface: &str,
        member: &str,
        path: &str,
        flags: u32,
        on_response: Box<dyn Fn(u32)>,
    ) -> u32 {
        let sender = c_string(sender);
        let interface = c_string(interface);
        let member = c_string(member);
        let path = c_string(path);
        // Arc is the C gatomicrefcount. Unsubscribe can free this user_data
        // while the callback is still on the stack. The closure is not shared.
        #[allow(clippy::arc_with_non_send_sync)]
        let ctx = Arc::new(SignalCtx { on_response });
        // SAFETY: the names outlive the call. GIO copies the match rule and
        // keeps `user_data` until unsubscribe.
        unsafe {
            g_dbus_connection_signal_subscribe(
                conn.0.as_ptr(),
                sender.as_ptr(),
                interface.as_ptr(),
                member.as_ptr(),
                path.as_ptr(),
                ptr::null(),
                flags,
                Some(signal_cb),
                Arc::into_raw(ctx).cast_mut().cast(),
                Some(signal_free),
            )
        }
    }

    fn unsubscribe(&self, conn: &Connection, id: u32) {
        // SAFETY: `id` came from `subscribe` on this connection.
        unsafe { g_dbus_connection_signal_unsubscribe(conn.0.as_ptr(), id) };
    }

    fn inhibit(
        &self,
        conn: &Connection,
        destination: &str,
        path: &str,
        interface: &str,
        method: &str,
        call_flags: i32,
        timeout: i32,
        window: &str,
        inhibit_flags: u32,
        token: &str,
        reason: &str,
        on_done: Box<dyn Fn(CallResult)>,
    ) {
        let destination = c_string(destination);
        let path = c_string(path);
        let interface = c_string(interface);
        let method = c_string(method);
        let window = c_string(window);
        let token = c_string(token);
        let reason = c_string(reason);
        let dict_type = unsafe { g_variant_type_new(c"a{sv}".as_ptr()) };
        // SAFETY: `g_variant_builder_new` copies the type. The two `{sv}`
        // entries consume the floating strings.
        let builder = unsafe { g_variant_builder_new(dict_type) };
        unsafe {
            g_variant_builder_add(
                builder,
                c"{sv}".as_ptr(),
                c"handle_token".as_ptr(),
                g_variant_new_string(token.as_ptr()),
            );
            g_variant_builder_add(
                builder,
                c"{sv}".as_ptr(),
                c"reason".as_ptr(),
                g_variant_new_string(reason.as_ptr()),
            );
        }
        // SAFETY: `(sua{sv})` consumes the builder's contents. The builder
        // object itself is freed afterwards.
        let params = unsafe {
            g_variant_new(
                c"(sua{sv})".as_ptr(),
                window.as_ptr(),
                inhibit_flags,
                builder,
            )
        };
        unsafe {
            g_variant_builder_unref(builder);
            g_variant_type_free(dict_type);
        }
        // SAFETY: a new cancellable has one reference. The slot and the call
        // share it; `cancel` or the callback unrefs it once.
        let cancellable = unsafe { g_cancellable_new() };
        *self.cancellable.lock().expect("cancellable") =
            NonNull::new(cancellable).map(|p| p.as_ptr() as usize);
        let ctx = Box::new(CallCtx {
            cancellable: Some(Arc::clone(&self.cancellable)),
            on_done,
        });
        // SAFETY: GIO consumes the floating `params` and refs the cancellable.
        // The callback frees `ctx`.
        unsafe {
            g_dbus_connection_call(
                conn.0.as_ptr(),
                destination.as_ptr(),
                path.as_ptr(),
                interface.as_ptr(),
                method.as_ptr(),
                params,
                ptr::null(),
                call_flags,
                timeout,
                cancellable,
                Some(call_cb),
                Box::into_raw(ctx).cast(),
            );
        }
    }

    fn close(
        &self,
        conn: &Connection,
        destination: &str,
        path: &str,
        interface: &str,
        method: &str,
        call_flags: i32,
        timeout: i32,
        on_done: Box<dyn Fn(CallResult)>,
    ) {
        let destination = c_string(destination);
        let path = c_string(path);
        let interface = c_string(interface);
        let method = c_string(method);
        // SAFETY: `()` is an empty tuple. GIO consumes the floating reference.
        let params = unsafe { g_variant_new(c"()".as_ptr()) };
        let reply_type = unsafe { g_variant_type_new(c"()".as_ptr()) };
        let ctx = Box::new(CallCtx {
            cancellable: None,
            on_done,
        });
        // SAFETY: Close has no cancellable. The callback frees `ctx`.
        unsafe {
            g_dbus_connection_call(
                conn.0.as_ptr(),
                destination.as_ptr(),
                path.as_ptr(),
                interface.as_ptr(),
                method.as_ptr(),
                params,
                reply_type,
                call_flags,
                timeout,
                ptr::null_mut(),
                Some(call_cb),
                Box::into_raw(ctx).cast(),
            );
            g_variant_type_free(reply_type);
        }
    }

    fn cancel(&self) {
        let cancellable = self.cancellable.lock().expect("cancellable").take();
        if let Some(bits) = cancellable {
            // SAFETY: the bits are the pointer stored in `inhibit`. Taking it
            // out first keeps the async callback from unrefing it as well.
            // `g_cancellable_cancel` may run that callback before it returns.
            unsafe {
                g_cancellable_cancel(bits as *mut c_void);
                g_object_unref(bits as *mut c_void);
            }
        }
    }

    fn log(&self, level: i32, message: &str) {
        let message = c_string(message);
        // SAFETY: the format contains one `%s`, with one C string argument.
        unsafe { blog(level, c"%s".as_ptr(), message.as_ptr()) };
    }
}

/// # Safety
/// The result is destroyed once by `portal_inhibit_info_destroy`.
#[unsafe(no_mangle)]
pub extern "C" fn portal_inhibit_info_create() -> *mut c_void {
    Portal::create(GioHost::new())
        .map_or(ptr::null_mut(), |info| Box::into_raw(Box::new(info)).cast())
}

/// # Safety
/// `info` is null or a live pointer from `portal_inhibit_info_create`.
/// `reason` is null or a NUL-terminated C string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn portal_inhibit(info: *mut c_void, reason: *const c_char, active: bool) {
    // SAFETY: null, or the live `Portal<GioHost>` from `Box::into_raw`.
    let Some(info) = (unsafe { info.cast::<Portal<GioHost>>().as_mut() }) else {
        return;
    };
    let reason = if reason.is_null() {
        String::new()
    } else {
        // SAFETY: the caller passes a NUL-terminated reason.
        unsafe { CStr::from_ptr(reason) }
            .to_string_lossy()
            .into_owned()
    };
    info.set_active(&reason, active);
}

/// # Safety
/// `info` is null or a live pointer from `portal_inhibit_info_create`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn portal_inhibit_info_destroy(info: *mut c_void) {
    if info.is_null() {
        Portal::<GioHost>::destroy(None);
        return;
    }
    // SAFETY: this takes back the unique allocation once.
    let info = unsafe { Box::from_raw(info.cast::<Portal<GioHost>>()) };
    Portal::destroy(Some(*info));
}
