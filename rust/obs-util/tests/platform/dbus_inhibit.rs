//! Tier 1 for the session D-Bus inhibitor (#1576). The ScreenSaver cases
//! mirror `test_inhibit_sleep_dbus.c` through its direct D-Bus path.

use std::cell::RefCell;
use std::ffi::CStr;
use std::rc::Rc;

use obs_util::platform::unix::dbus_inhibit::{DbusInfo, Host, Service};

#[derive(Default)]
struct State {
    connected: bool,
    owners: Vec<Service>,
    calls: Vec<String>,
    fail_call: bool,
    cookie: u32,
}

#[derive(Clone)]
struct Fake(Rc<RefCell<State>>);

impl Fake {
    fn new(owners: &[Service]) -> Self {
        Self(Rc::new(RefCell::new(State {
            connected: true,
            owners: owners.to_vec(),
            cookie: 4242,
            ..State::default()
        })))
    }

    fn calls(&self) -> Vec<String> {
        self.0.borrow().calls.clone()
    }
}

impl Host for Fake {
    type Connection = ();

    fn connect(&self) -> Result<(), String> {
        self.0.borrow_mut().calls.push("connect".into());
        self.0
            .borrow()
            .connected
            .then_some(())
            .ok_or_else(|| "no session bus".into())
    }

    fn has_owner(&self, _: &(), service: Service) -> bool {
        let mut state = self.0.borrow_mut();
        state.calls.push(format!("owner {service:?}"));
        state.owners.contains(&service)
    }

    fn inhibit(&self, _: &(), service: Service, reason: &CStr) -> Result<u32, String> {
        let mut state = self.0.borrow_mut();
        state
            .calls
            .push(format!("inhibit {service:?} {}", reason.to_string_lossy()));
        if state.fail_call {
            Err("call failed".into())
        } else {
            Ok(state.cookie)
        }
    }

    fn uninhibit(&self, _: &(), service: Service, cookie: u32) -> Result<(), String> {
        let mut state = self.0.borrow_mut();
        state.calls.push(format!("uninhibit {service:?} {cookie}"));
        if state.fail_call {
            Err("call failed".into())
        } else {
            Ok(())
        }
    }

    fn log(&self, level: i32, message: &str) {
        self.0
            .borrow_mut()
            .calls
            .push(format!("log {level} {message}"));
    }
}

#[test]
fn test_screensaver_inhibit_and_release() {
    let fake = Fake::new(&[Service::ScreenSaver]);
    let mut info = DbusInfo::create(fake.clone()).expect("screensaver service");
    assert_eq!(info.service(), Service::ScreenSaver);
    info.set_active(c"OBS Video/audio", true);
    assert_eq!(info.cookie(), 4242);
    info.set_active(c"OBS Video/audio", false);
    assert_eq!(info.cookie(), 0);
    assert_eq!(
        fake.calls(),
        [
            "connect",
            "owner ScreenSaver",
            "log 400 Found dbus service: org.freedesktop.ScreenSaver",
            "inhibit ScreenSaver OBS Video/audio",
            "uninhibit ScreenSaver 4242",
        ]
    );
}

#[test]
fn test_screensaver_repeated_set_active() {
    let fake = Fake::new(&[Service::ScreenSaver]);
    let mut info = DbusInfo::create(fake.clone()).unwrap();
    info.set_active(c"reason", true);
    info.set_active(c"reason", true);
    info.set_active(c"reason", false);
    info.set_active(c"reason", false);
    assert_eq!(
        fake.calls()
            .iter()
            .filter(|call| call.starts_with("inhibit ") || call.starts_with("uninhibit "))
            .count(),
        2
    );
}

#[test]
fn service_selection_uses_c_preference_order() {
    let fake = Fake::new(&[Service::GnomeSession, Service::PowerManagement]);
    let info = DbusInfo::create(fake.clone()).unwrap();
    assert_eq!(info.service(), Service::PowerManagement);
    assert_eq!(
        fake.calls(),
        [
            "connect",
            "owner ScreenSaver",
            "owner PowerManagement",
            "log 400 Found dbus service: org.freedesktop.PowerManagement.Inhibit",
        ]
    );
}

#[test]
fn no_bus_or_service_returns_none() {
    let fake = Fake::new(&[]);
    assert!(DbusInfo::create(fake.clone()).is_none());
    assert_eq!(fake.calls().len(), 5);

    let fake = Fake::new(&[]);
    fake.0.borrow_mut().connected = false;
    assert!(DbusInfo::create(fake.clone()).is_none());
    assert_eq!(
        fake.calls(),
        [
            "connect",
            "log 100 Could not create dbus connection: no session bus"
        ]
    );
}

#[test]
fn failed_calls_preserve_cookie_until_a_successful_reply() {
    let fake = Fake::new(&[Service::ScreenSaver]);
    let mut info = DbusInfo::create(fake.clone()).unwrap();
    fake.0.borrow_mut().fail_call = true;
    info.set_active(c"reason", true);
    assert_eq!(info.cookie(), 0);
    fake.0.borrow_mut().fail_call = false;
    info.set_active(c"reason", true);
    assert_eq!(info.cookie(), 4242);
    fake.0.borrow_mut().fail_call = true;
    info.set_active(c"reason", false);
    assert_eq!(info.cookie(), 4242);
    assert!(
        fake.calls()
            .contains(&"log 100 Failed to call Inhibit: call failed".into())
    );
    assert!(
        fake.calls()
            .contains(&"log 100 Failed to call UnInhibit: call failed".into())
    );
}
