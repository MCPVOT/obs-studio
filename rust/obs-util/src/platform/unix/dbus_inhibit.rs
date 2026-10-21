//! Session D-Bus sleep inhibitor from `util/platform-nix-dbus.c.h`.

use std::ffi::CStr;

use crate::base::{LOG_DEBUG, LOG_ERROR};

/// The four services, in the same preference order as the C implementation.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Service {
    ScreenSaver,
    PowerManagement,
    MateSession,
    GnomeSession,
}

pub const SERVICES: [Service; 4] = [
    Service::ScreenSaver,
    Service::PowerManagement,
    Service::MateSession,
    Service::GnomeSession,
];

impl Service {
    pub const fn name(self) -> &'static str {
        match self {
            Self::ScreenSaver => "org.freedesktop.ScreenSaver",
            Self::PowerManagement => "org.freedesktop.PowerManagement.Inhibit",
            Self::MateSession => "org.mate.SessionManager",
            Self::GnomeSession => "org.gnome.SessionManager",
        }
    }

    pub const fn path(self) -> &'static CStr {
        match self {
            Self::ScreenSaver => c"/ScreenSaver",
            Self::PowerManagement => c"/org/freedesktop/PowerManagement",
            Self::MateSession => c"/org/mate/SessionManager",
            Self::GnomeSession => c"/org/gnome/SessionManager",
        }
    }

    pub const fn interface(self) -> &'static CStr {
        match self {
            Self::ScreenSaver => c"org.freedesktop.ScreenSaver",
            Self::PowerManagement => c"org.freedesktop.PowerManagement.Inhibit",
            Self::MateSession => c"org.mate.SessionManager",
            Self::GnomeSession => c"org.gnome.SessionManager",
        }
    }

    pub const fn uninhibit_method(self) -> &'static CStr {
        match self {
            Self::ScreenSaver | Self::PowerManagement => c"UnInhibit",
            Self::MateSession | Self::GnomeSession => c"Uninhibit",
        }
    }
}

/// Host calls used by the safe state machine. GIO lives in the C ABI shim.
pub trait Host {
    type Connection;

    fn connect(&self) -> Result<Self::Connection, String>;
    fn has_owner(&self, connection: &Self::Connection, service: Service) -> bool;
    fn inhibit(
        &self,
        connection: &Self::Connection,
        service: Service,
        reason: &CStr,
    ) -> Result<u32, String>;
    fn uninhibit(
        &self,
        connection: &Self::Connection,
        service: Service,
        cookie: u32,
    ) -> Result<(), String>;
    fn log(&self, level: i32, message: &str);
}

/// Opaque `struct dbus_sleep_info` state.
pub struct DbusInfo<H: Host> {
    host: H,
    connection: H::Connection,
    service: Service,
    cookie: u32,
}

impl<H: Host> DbusInfo<H> {
    /// `dbus_sleep_info_create`.
    pub fn create(host: H) -> Option<Self> {
        let connection = match host.connect() {
            Ok(connection) => connection,
            Err(error) => {
                host.log(
                    LOG_ERROR,
                    &format!("Could not create dbus connection: {error}"),
                );
                return None;
            }
        };
        for service in SERVICES {
            if host.has_owner(&connection, service) {
                host.log(
                    LOG_DEBUG,
                    &format!("Found dbus service: {}", service.name()),
                );
                return Some(Self {
                    host,
                    connection,
                    service,
                    cookie: 0,
                });
            }
        }
        None
    }

    pub fn service(&self) -> Service {
        self.service
    }

    pub fn cookie(&self) -> u32 {
        self.cookie
    }

    /// `dbus_inhibit_sleep`.
    pub fn set_active(&mut self, reason: &CStr, active: bool) {
        if active == (self.cookie != 0) {
            return;
        }
        let method = if active {
            "Inhibit"
        } else {
            self.service
                .uninhibit_method()
                .to_str()
                .expect("ASCII method")
        };
        let result = if active {
            self.host
                .inhibit(&self.connection, self.service, reason)
                .map(Some)
        } else {
            self.host
                .uninhibit(&self.connection, self.service, self.cookie)
                .map(|()| None)
        };
        match result {
            Ok(Some(cookie)) => self.cookie = cookie,
            Ok(None) => self.cookie = 0,
            Err(error) => self
                .host
                .log(LOG_ERROR, &format!("Failed to call {method}: {error}")),
        }
    }
}
