//! Tier 3 for `libobs/util/platform-nix-portal.c.h` (#1631).
//! The oracle compiles that file with its GIO calls traced. The Rust core
//! runs the same steps against a host that writes the same lines.
//!
//! Intentional differences, also in `rust/README.md`:
//! - The handle is an `Arc` behind a `Box`. C uses `bzalloc` and
//!   `gatomicrefcount`. The trace does not count allocations.
//! - A NULL reason becomes an empty string. `g_variant_new_string` does not
//!   accept NULL. These cases pass a real string.
//! - Fields other than `destroying` sit behind one mutex. C leaves them
//!   unsynchronized. The calls below are on one thread.
//!
//! Kept, characterized, not endorsed: destroying an active inhibit does not
//! send `Request.Close`. Only `portal_inhibit(..., false)` does.

#![cfg(any(target_os = "linux", target_os = "freebsd"))]

use std::cell::RefCell;
use std::ffi::CString;
use std::rc::Rc;
use std::sync::Mutex;

use obs_c_oracle::portal_inhibit::Oracle;
use obs_util::platform::unix::portal_inhibit::{
    CALL_FLAGS_NONE, CALL_TIMEOUT_INFINITE, CallResult, Host, INHIBIT_IFACE, PORTAL_NAME,
    PORTAL_PATH, PROPERTIES_IFACE, Portal, REQUEST_IFACE, SESSION_BUS, SIGNAL_FLAGS_NO_MATCH_RULE,
};
use proptest::prelude::*;

static LOCK: Mutex<()> = Mutex::new(());

// `dyn Fn(u32)` desugars past clippy's type-complexity limit.
type ResponseFn = Rc<dyn Fn(u32)>;

struct Pending {
    done: Box<dyn Fn(CallResult)>,
    cancel: Option<Rc<std::cell::Cell<bool>>>,
}

struct Inner {
    bus_missing: bool,
    portal_missing: bool,
    next_id: u32,
    trace: String,
    response: Option<(u32, ResponseFn)>,
    calls: Vec<Pending>,
}

#[derive(Clone)]
struct TraceHost(Rc<RefCell<Inner>>);

impl TraceHost {
    fn new(bus_missing: bool, portal_missing: bool) -> Self {
        Self(Rc::new(RefCell::new(Inner {
            bus_missing,
            portal_missing,
            next_id: 7,
            trace: String::new(),
            response: None,
            calls: Vec::new(),
        })))
    }

    fn line(&self, text: &str) {
        self.0.borrow_mut().trace.push_str(text);
    }

    fn complete(&self, error: bool) {
        let call = self.0.borrow_mut().calls.remove(0);
        let cancelled = call.cancel.as_ref().is_some_and(|flag| flag.get());
        let result = if cancelled {
            CallResult::Cancelled
        } else if error {
            CallResult::Failed("method failed".into())
        } else {
            CallResult::Ok
        };
        (call.done)(result);
    }

    fn respond(&self, code: u32) {
        let response = self
            .0
            .borrow()
            .response
            .as_ref()
            .map(|slot| Rc::clone(&slot.1))
            .expect("response");
        response(code);
    }

    fn trace(&self) -> String {
        self.0.borrow().trace.clone()
    }
}

#[allow(clippy::too_many_arguments)]
impl Host for TraceHost {
    type Conn = ();

    fn bus_get(&self, bus_type: i32) -> Result<(), String> {
        self.line(&format!("bus {bus_type}\n"));
        if bus_type != SESSION_BUS {
            return Err("wrong bus".into());
        }
        if self.0.borrow().bus_missing {
            Err("no session bus".into())
        } else {
            Ok(())
        }
    }

    fn unique_name(&self, _: &()) -> String {
        self.line("unique :1.42\n");
        ":1.42".into()
    }

    fn properties_get(&self, _: &(), interface: &str, property: &str) -> bool {
        self.line(&format!(
            "get {PORTAL_NAME} {PORTAL_PATH} {PROPERTIES_IFACE} Get {interface} {property} {CALL_FLAGS_NONE} {CALL_TIMEOUT_INFINITE}\n"
        ));
        !self.0.borrow().portal_missing
    }

    fn request_id(&self) -> u32 {
        let mut inner = self.0.borrow_mut();
        let id = inner.next_id;
        inner.next_id = id.wrapping_add(1);
        inner.trace.push_str(&format!("rand {id}\n"));
        id
    }

    fn subscribe(
        &self,
        _: &(),
        sender: &str,
        interface: &str,
        member: &str,
        path: &str,
        flags: u32,
        on_response: Box<dyn Fn(u32)>,
    ) -> u32 {
        self.line(&format!(
            "subscribe {sender} {interface} {member} {path} {flags}\n"
        ));
        assert_eq!(interface, REQUEST_IFACE);
        assert_eq!(flags, SIGNAL_FLAGS_NO_MATCH_RULE);
        self.0.borrow_mut().response = Some((17, Rc::from(on_response)));
        17
    }

    fn unsubscribe(&self, _: &(), id: u32) {
        self.line(&format!("unsubscribe {id}\n"));
        let response = self.0.borrow_mut().response.take();
        assert!(response.is_some());
    }

    fn inhibit(
        &self,
        _: &(),
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
        self.line(&format!(
            "inhibit {destination} {path} {interface} {method} {call_flags} {timeout} [{window}] {inhibit_flags} [{token}] [{reason}]\n"
        ));
        assert_eq!(interface, INHIBIT_IFACE);
        let cancel = Rc::new(std::cell::Cell::new(false));
        self.0.borrow_mut().calls.push(Pending {
            done: on_done,
            cancel: Some(cancel),
        });
    }

    fn close(
        &self,
        _: &(),
        destination: &str,
        path: &str,
        interface: &str,
        method: &str,
        call_flags: i32,
        timeout: i32,
        on_done: Box<dyn Fn(CallResult)>,
    ) {
        self.line(&format!(
            "close {destination} {path} {interface} {method} {call_flags} {timeout}\n"
        ));
        self.0.borrow_mut().calls.push(Pending {
            done: on_done,
            cancel: None,
        });
    }

    fn cancel(&self) {
        self.line("cancel\n");
        let inner = self.0.borrow();
        let pending = inner.iter_cancellable();
        pending.set(true);
    }

    fn log(&self, level: i32, message: &str) {
        self.line(&format!("log {level} {message}\n"));
    }
}

impl Inner {
    fn iter_cancellable(&self) -> Rc<std::cell::Cell<bool>> {
        self.calls
            .iter()
            .rev()
            .find_map(|call| call.cancel.clone())
            .expect("cancellable")
    }
}

enum Step {
    Create,
    Set(&'static str, bool),
    Complete(bool),
    Respond(u32),
    Destroy,
}

fn rust_trace(bus_missing: bool, portal_missing: bool, steps: &[Step]) -> String {
    let host = TraceHost::new(bus_missing, portal_missing);
    let mut portal = None;
    for step in steps {
        match step {
            Step::Create => portal = Portal::create(host.clone()),
            Step::Set(reason, active) => {
                if let Some(portal) = &portal {
                    portal.set_active(reason, *active);
                }
            }
            Step::Complete(error) => host.complete(*error),
            Step::Respond(code) => host.respond(*code),
            Step::Destroy => Portal::destroy(portal.take()),
        }
    }
    host.trace()
}

fn c_trace(bus_missing: bool, portal_missing: bool, steps: &[Step]) -> String {
    let mut oracle = Oracle::start(bus_missing, portal_missing);
    for step in steps {
        match step {
            Step::Create => oracle.create(),
            Step::Set(reason, active) => {
                let reason = CString::new(*reason).expect("reason");
                oracle.inhibit(&reason, *active);
            }
            Step::Complete(error) => oracle.complete(*error),
            Step::Respond(code) => oracle.respond(*code),
            Step::Destroy => oracle.destroy(),
        }
    }
    oracle.trace()
}

fn assert_same(bus_missing: bool, portal_missing: bool, steps: &[Step]) {
    let _guard = LOCK.lock().expect("portal parity lock");
    let left = rust_trace(bus_missing, portal_missing, steps);
    let right = c_trace(bus_missing, portal_missing, steps);
    assert_eq!(left, right);
}

#[test]
fn discovery_matches_c() {
    assert_same(false, false, &[Step::Destroy]);
    assert_same(true, false, &[Step::Create, Step::Destroy]);
    assert_same(false, true, &[Step::Create, Step::Destroy]);
    assert_same(
        false,
        false,
        &[Step::Create, Step::Set("unused", false), Step::Destroy],
    );
}

#[test]
fn inhibit_and_release_matches_c() {
    assert_same(
        false,
        false,
        &[
            Step::Create,
            Step::Set("recording ☃", true),
            Step::Set("ignored", true),
            Step::Complete(false),
            Step::Respond(0),
            Step::Set("unused", false),
            Step::Set("unused", false),
            Step::Complete(false),
            Step::Destroy,
        ],
    );
}

#[test]
fn rejected_and_errors_match_c() {
    for code in [1, 2, 99] {
        assert_same(
            false,
            false,
            &[
                Step::Create,
                Step::Set("", true),
                Step::Complete(false),
                Step::Respond(code),
                Step::Set("unused", false),
                Step::Set("retry", true),
                Step::Complete(false),
                Step::Respond(0),
                Step::Destroy,
            ],
        );
    }
    assert_same(
        false,
        false,
        &[
            Step::Create,
            Step::Set("recording", true),
            Step::Complete(true),
            Step::Set("retry", true),
            Step::Destroy,
        ],
    );
}

#[test]
fn cancel_and_destroy_match_c() {
    assert_same(
        false,
        false,
        &[
            Step::Create,
            Step::Set("recording", true),
            Step::Set("unused", false),
            Step::Complete(true),
            Step::Destroy,
        ],
    );
    assert_same(
        false,
        false,
        &[
            Step::Create,
            Step::Set("recording", true),
            Step::Destroy,
            Step::Complete(true),
        ],
    );
    assert_same(
        false,
        false,
        &[
            Step::Create,
            Step::Set("recording", true),
            Step::Complete(false),
            Step::Respond(0),
            Step::Set("unused", false),
            Step::Destroy,
            Step::Complete(false),
        ],
    );
    assert_same(
        false,
        false,
        &[
            Step::Create,
            Step::Set("recording", true),
            Step::Complete(false),
            Step::Respond(0),
            Step::Destroy,
        ],
    );
}

#[test]
fn response_before_reply_matches_c() {
    for code in [0, 1, 2] {
        assert_same(
            false,
            false,
            &[
                Step::Create,
                Step::Set("recording", true),
                Step::Respond(code),
                Step::Complete(false),
                Step::Destroy,
            ],
        );
    }
}

proptest! {
    #[test]
    fn random_response_matches_oracle(code in 0u32..8, reply_first: bool) {
        let _guard = LOCK.lock().expect("portal parity lock");
        let mut steps = vec![Step::Create, Step::Set("recording", true)];
        if reply_first {
            steps.push(Step::Complete(false));
            steps.push(Step::Respond(code));
        } else {
            steps.push(Step::Respond(code));
            steps.push(Step::Complete(false));
        }
        steps.push(Step::Destroy);
        let left = rust_trace(false, false, &steps);
        let right = c_trace(false, false, &steps);
        prop_assert_eq!(left, right);
    }
}
