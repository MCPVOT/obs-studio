//! GIO host and C ABI for `util/platform-nix-dbus.c.h` (#1576).
//! The selection and cookie state live in the safe core; this module owns
//! GIO objects and translates its synchronous calls.
//! The opaque handle is a Rust `Box` (C uses `bzalloc`), and a NULL reason
//! becomes an empty string rather than being passed to GLib's string API.

use core::ffi::{CStr, c_char, c_int, c_void};
use core::ptr::{self, NonNull};
use std::ffi::CString;

use crate::platform::unix::dbus_inhibit::{DbusInfo, Host, Service};

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
    fn g_variant_new(format: *const c_char, ...) -> *mut c_void;
    fn g_variant_new_uint32(value: u32) -> *mut c_void;
    fn g_variant_get(value: *mut c_void, format: *const c_char, ...);
    fn g_variant_unref(value: *mut c_void);
    fn g_object_unref(object: *mut c_void);
    fn g_error_free(error: *mut GError);
    fn blog(log_level: c_int, format: *const c_char, ...);
}

/// The session bus connection is owned by this inhibitor and dropped once.
pub struct Connection(NonNull<c_void>);

impl Drop for Connection {
    fn drop(&mut self) {
        // SAFETY: `self.0` came from `g_bus_get_sync` and owns one reference.
        unsafe { g_object_unref(self.0.as_ptr()) };
    }
}

fn service_name(service: Service) -> &'static CStr {
    match service {
        Service::ScreenSaver => c"org.freedesktop.ScreenSaver",
        Service::PowerManagement => c"org.freedesktop.PowerManagement.Inhibit",
        Service::MateSession => c"org.mate.SessionManager",
        Service::GnomeSession => c"org.gnome.SessionManager",
    }
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

/// One call through GIO, with the C's infinite timeout and no cancellable.
fn call(
    connection: &Connection,
    service: Service,
    method: &CStr,
    params: *mut c_void,
    flags: c_int,
    error: *mut *mut GError,
) -> *mut c_void {
    // SAFETY: the connection, static names and GVariant parameters are live.
    // GIO consumes the floating reference to `params`.
    unsafe {
        g_dbus_connection_call_sync(
            connection.0.as_ptr(),
            service_name(service).as_ptr(),
            service.path().as_ptr(),
            service.interface().as_ptr(),
            method.as_ptr(),
            params,
            ptr::null(),
            flags,
            -1,
            ptr::null_mut(),
            error,
        )
    }
}

pub struct GioHost;

impl Host for GioHost {
    type Connection = Connection;

    fn connect(&self) -> Result<Connection, String> {
        let mut error = ptr::null_mut();
        // G_BUS_TYPE_SESSION = 2.
        // SAFETY: out-pointer and NULL cancellable follow the GIO API.
        let connection = unsafe { g_bus_get_sync(2, ptr::null_mut(), &raw mut error) };
        match NonNull::new(connection) {
            Some(connection) => Ok(Connection(connection)),
            // SAFETY: GIO owns the error returned on failure.
            None => Err(unsafe { take_error(error) }),
        }
    }

    fn has_owner(&self, connection: &Connection, service: Service) -> bool {
        // SAFETY: the format and one NUL-terminated string match `(s)`.
        let params = unsafe { g_variant_new(c"(s)".as_ptr(), service_name(service).as_ptr()) };
        // The C calls GetNameOwner on the bus without auto-starting services.
        // SAFETY: all names and the connection are live; GIO consumes params.
        let reply = unsafe {
            g_dbus_connection_call_sync(
                connection.0.as_ptr(),
                c"org.freedesktop.DBus".as_ptr(),
                c"/org/freedesktop/DBus".as_ptr(),
                c"org.freedesktop.DBus".as_ptr(),
                c"GetNameOwner".as_ptr(),
                params,
                ptr::null(),
                1, // G_DBUS_CALL_FLAGS_NO_AUTO_START
                -1,
                ptr::null_mut(),
                ptr::null_mut(),
            )
        };
        if reply.is_null() {
            false
        } else {
            // SAFETY: non-NULL GIO reply has one owned reference.
            unsafe { g_variant_unref(reply) };
            true
        }
    }

    fn inhibit(
        &self,
        connection: &Connection,
        service: Service,
        reason: &CStr,
    ) -> Result<u32, String> {
        let params = match service {
            Service::MateSession | Service::GnomeSession => {
                // SAFETY: `(s@usu)` consumes one `(u)` variant, then a string
                // and a guint32. The C code passes XID 0 and flags 0xC.
                unsafe {
                    g_variant_new(
                        c"(s@usu)".as_ptr(),
                        c"libobs".as_ptr(),
                        g_variant_new_uint32(0),
                        reason.as_ptr(),
                        0xCu32,
                    )
                }
            }
            _ => {
                // SAFETY: the two C strings match `(ss)`.
                unsafe { g_variant_new(c"(ss)".as_ptr(), c"libobs".as_ptr(), reason.as_ptr()) }
            }
        };
        let mut error = ptr::null_mut();
        let reply = call(connection, service, c"Inhibit", params, 0, &raw mut error);
        if !error.is_null() {
            // SAFETY: GIO returned this error to us.
            return Err(unsafe { take_error(error) });
        }
        let mut cookie = 0u32;
        // SAFETY: successful Inhibit returns `(u)`; the reply is owned.
        unsafe {
            g_variant_get(reply, c"(u)".as_ptr(), &raw mut cookie);
            g_variant_unref(reply);
        }
        Ok(cookie)
    }

    fn uninhibit(
        &self,
        connection: &Connection,
        service: Service,
        cookie: u32,
    ) -> Result<(), String> {
        // SAFETY: one guint32 matches `(u)`.
        let params = unsafe { g_variant_new(c"(u)".as_ptr(), cookie) };
        let mut error = ptr::null_mut();
        let reply = call(
            connection,
            service,
            service.uninhibit_method(),
            params,
            0,
            &raw mut error,
        );
        if !error.is_null() {
            // SAFETY: GIO returned this error to us.
            return Err(unsafe { take_error(error) });
        }
        // SAFETY: GIO returns one owned variant on success.
        unsafe { g_variant_unref(reply) };
        Ok(())
    }

    fn log(&self, level: i32, message: &str) {
        let message = CString::new(message).unwrap_or_default();
        // SAFETY: the format contains one `%s`, with one C string argument.
        unsafe { blog(level, c"%s".as_ptr(), message.as_ptr()) };
    }
}

/// # Safety
/// The result is destroyed once by `dbus_sleep_info_destroy`.
#[unsafe(no_mangle)]
pub extern "C" fn dbus_sleep_info_create() -> *mut c_void {
    DbusInfo::create(GioHost).map_or(ptr::null_mut(), |info| Box::into_raw(Box::new(info)).cast())
}

/// # Safety
/// `info` is a live pointer returned by `dbus_sleep_info_create`, and
/// `reason` is a NUL-terminated C string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dbus_inhibit_sleep(
    info: *mut c_void,
    reason: *const c_char,
    active: bool,
) {
    // SAFETY: the caller owns the live pointer and valid reason.
    let Some(info) = (unsafe { info.cast::<DbusInfo<GioHost>>().as_mut() }) else {
        return;
    };
    let reason = if reason.is_null() {
        c""
    } else {
        // SAFETY: the caller passes a NUL-terminated reason.
        unsafe { CStr::from_ptr(reason) }
    };
    info.set_active(reason, active);
}

/// # Safety
/// `info` is null or a live pointer returned by `dbus_sleep_info_create`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn dbus_sleep_info_destroy(info: *mut c_void) {
    if !info.is_null() {
        // SAFETY: this takes back the unique allocation once.
        drop(unsafe { Box::from_raw(info.cast::<DbusInfo<GioHost>>()) });
    }
}
