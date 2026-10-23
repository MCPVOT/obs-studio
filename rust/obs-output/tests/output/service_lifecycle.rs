//! Tier 1 for obs_output::service, lifecycle, references, module lookup and
//! activation (#1920): the cases of test/cmocka/test_service_lifecycle.c
//! plus the output-side activate/deactivate/initialize, against services,
//! weak refs and modules in Rust. Every libobs call is traced, so the cases
//! pin the C's order of calls, not only the results.

use core::cell::RefCell;
use core::ffi::CStr;

use obs_output::service::{self, Lifecycle, Modules, NullLog};

#[derive(Default)]
struct Log(RefCell<Vec<String>>);

impl NullLog for Log {
    fn null_param(&self, func: &'static CStr, param: &'static CStr) {
        self.0.borrow_mut().push(format!(
            "{}:{}",
            func.to_string_lossy(),
            param.to_string_lossy()
        ));
    }
}

#[derive(Clone, Default)]
struct Svc {
    data: Option<u32>,
    active: bool,
    destroy: bool,
    output: bool,
    strong: i32,
    weak: i32,
    freed: bool,
    weak_freed: bool,
}

/// Services are indices; `create_returns` is what the next `create()` gives.
#[derive(Default)]
struct Fake {
    services: RefCell<Vec<Svc>>,
    trace: RefCell<Vec<String>>,
    known: Vec<&'static str>,
    context_fails: bool,
    create_returns: Option<u32>,
    has_activate: bool,
    has_deactivate: bool,
    initialize: Option<bool>,
}

impl Fake {
    fn new() -> Self {
        Fake {
            known: vec!["rtmp_custom"],
            create_returns: Some(7),
            has_activate: true,
            has_deactivate: true,
            ..Fake::default()
        }
    }
    fn ev(&self, e: impl Into<String>) {
        self.trace.borrow_mut().push(e.into());
    }
    fn take(&self) -> Vec<String> {
        core::mem::take(&mut *self.trace.borrow_mut())
    }
    fn svc(&self, s: usize) -> Svc {
        self.services.borrow()[s].clone()
    }
    fn with(&self, s: usize, f: impl FnOnce(&mut Svc)) {
        f(&mut self.services.borrow_mut()[s]);
    }
    /// A live service as `create` leaves it: one strong and one weak ref.
    fn live(&self, data: Option<u32>) -> usize {
        let mut v = self.services.borrow_mut();
        v.push(Svc {
            data,
            strong: 1,
            weak: 1,
            ..Svc::default()
        });
        v.len() - 1
    }
}

impl Lifecycle for Fake {
    type Svc = usize;
    type Weak = usize;
    type Output = &'static str;
    type Info = &'static str;
    type Str = &'static str;
    type Data = Option<&'static str>;
    type Ptr = u32;

    fn find(&self, id: &'static str) -> Option<&'static str> {
        self.ev(format!("find {id}"));
        self.known.contains(&id).then_some(id)
    }
    fn context_new(
        &self,
        name: &'static str,
        settings: Option<&'static str>,
        hotkey_data: Option<&'static str>,
        private: bool,
    ) -> Option<usize> {
        self.ev(format!(
            "context_new {name} {settings:?} {hotkey_data:?} private={private}"
        ));
        if self.context_fails {
            return None;
        }
        Some(self.live(None))
    }
    fn set_info(&self, service: usize, info: &'static str) {
        self.ev(format!("set_info {service} {info}"));
    }
    fn call_create(&self, service: usize) -> Option<u32> {
        self.ev(format!("create {service}"));
        self.create_returns
    }
    fn set_plugin_data(&self, service: usize, data: Option<u32>) {
        self.ev(format!("set_data {service} {data:?}"));
        self.with(service, |s| s.data = data);
    }
    fn plugin_data(&self, service: usize) -> Option<u32> {
        self.svc(service).data
    }
    fn context_insert(&self, service: usize) {
        self.ev(format!("insert {service}"));
    }
    fn context_remove(&self, service: usize) {
        self.ev(format!("remove {service}"));
    }
    fn context_free(&self, service: usize) {
        self.ev(format!("free {service}"));
        self.with(service, |s| s.freed = true);
    }
    fn set_destroy(&self, service: usize) {
        self.ev(format!("set_destroy {service}"));
        self.with(service, |s| s.destroy = true);
    }
    fn destroying(&self, service: usize) -> bool {
        self.svc(service).destroy
    }
    fn active(&self, service: usize) -> bool {
        self.svc(service).active
    }
    fn set_active(&self, service: usize, active: bool) {
        self.ev(format!("set_active {service} {active}"));
        self.with(service, |s| s.active = active);
    }
    fn has_output(&self, service: usize) -> bool {
        self.svc(service).output
    }
    fn clear_output(&self, service: usize) {
        self.ev(format!("clear_output {service}"));
        self.with(service, |s| s.output = false);
    }
    fn detach_from_output(&self, service: usize) {
        self.ev(format!("detach {service}"));
    }
    fn call_destroy(&self, service: usize, data: u32) {
        self.ev(format!("destroy_cb {service} {data}"));
    }
    fn call_activate(&self, service: usize) -> bool {
        if self.has_activate {
            self.ev(format!("activate_cb {service}"));
        }
        self.has_activate
    }
    fn call_deactivate(&self, service: usize) -> bool {
        if self.has_deactivate {
            self.ev(format!("deactivate_cb {service}"));
        }
        self.has_deactivate
    }
    fn call_initialize(&self, service: usize, output: &'static str) -> Option<bool> {
        if self.initialize.is_some() {
            self.ev(format!("initialize_cb {service} {output}"));
        }
        self.initialize
    }
    fn control(&self, service: usize) -> usize {
        service
    }
    fn ref_release(&self, weak: usize) -> bool {
        self.ev(format!("ref_release {weak}"));
        let mut last = false;
        self.with(weak, |s| {
            s.strong -= 1;
            last = s.strong == 0;
        });
        last
    }
    fn weak_addref(&self, weak: usize) {
        self.ev(format!("weak_addref {weak}"));
        self.with(weak, |s| s.weak += 1);
    }
    fn weak_release(&self, weak: usize) -> bool {
        self.ev(format!("weak_release {weak}"));
        let mut last = false;
        self.with(weak, |s| {
            s.weak -= 1;
            last = s.weak == 0;
        });
        last
    }
    fn weak_get_ref(&self, weak: usize) -> bool {
        let mut got = false;
        self.with(weak, |s| {
            if s.strong > 0 {
                s.strong += 1;
                got = true;
            }
        });
        got
    }
    fn weak_target(&self, weak: usize) -> usize {
        weak
    }
    fn weak_free(&self, weak: usize) {
        self.ev(format!("weak_free {weak}"));
        self.with(weak, |s| s.weak_freed = true);
    }
    fn log_not_found(&self, id: &'static str) {
        self.ev(format!("log not_found {id}"));
    }
    fn log_create_failed(&self, name: &'static str) {
        self.ev(format!("log create_failed {name}"));
    }
    fn log_created(&self, name: &'static str, id: &'static str) {
        self.ev(format!("log created {name} {id}"));
    }
    fn log_destroyed(&self, service: usize) {
        self.ev(format!("log destroyed {service}"));
    }
    fn log_no_output(&self, service: usize) {
        self.ev(format!("log no_output {service}"));
    }
}

#[test]
fn create_unknown_id_logs_and_returns_none() {
    let f = Fake::new();
    let s = service::create(&f, "missing", "x", None, None, false);
    assert_eq!(s, None);
    assert_eq!(f.take(), ["find missing", "log not_found missing"]);
}

#[test]
fn create_stops_when_the_context_fails() {
    let f = Fake {
        context_fails: true,
        ..Fake::new()
    };
    let s = service::create(&f, "rtmp_custom", "x", Some("s"), Some("h"), false);
    assert_eq!(s, None);
    assert_eq!(
        f.take(),
        [
            "find rtmp_custom",
            "context_new x Some(\"s\") Some(\"h\") private=false"
        ]
    );
}

#[test]
fn create_runs_the_c_sequence() {
    let f = Fake::new();
    let s = service::create(&f, "rtmp_custom", "main", None, None, true).unwrap();
    assert_eq!(
        f.take(),
        [
            "find rtmp_custom",
            "context_new main None None private=true",
            "set_info 0 rtmp_custom",
            "create 0",
            "set_data 0 Some(7)",
            "insert 0",
            "log created main rtmp_custom",
        ]
    );
    assert_eq!(f.svc(s).data, Some(7));
}

#[test]
fn create_keeps_a_service_whose_plugin_create_failed() {
    let f = Fake {
        create_returns: None,
        ..Fake::new()
    };
    let s = service::create(&f, "rtmp_custom", "main", None, None, false);
    assert_eq!(s, Some(0));
    assert_eq!(
        f.take()[3..],
        [
            "create 0",
            "set_data 0 None",
            "log create_failed main",
            "insert 0",
            "log created main rtmp_custom",
        ]
    );
}

#[test]
fn destroy_frees_an_inactive_service_at_once() {
    let f = Fake::new();
    let s = f.live(Some(3));
    f.with(s, |s| s.output = true);
    service::destroy(&f, Some(s));
    assert_eq!(
        f.take(),
        [
            "remove 0",
            "set_destroy 0",
            "destroy_cb 0 3",
            "detach 0",
            "log destroyed 0",
            "free 0",
        ]
    );
    service::destroy(&f, None);
    assert!(f.take().is_empty());
}

#[test]
fn destroy_skips_the_plugin_without_data_or_output() {
    let f = Fake::new();
    let s = f.live(None);
    service::destroy(&f, Some(s));
    assert_eq!(
        f.take(),
        ["remove 0", "set_destroy 0", "log destroyed 0", "free 0"]
    );
}

#[test]
fn destroy_waits_for_an_active_service_and_deactivate_frees_it() {
    let f = Fake::new();
    let log = Log::default();
    let s = f.live(Some(3));
    f.with(s, |s| {
        s.output = true;
        s.active = true;
    });
    service::destroy(&f, Some(s));
    assert_eq!(f.take(), ["remove 0", "set_destroy 0"]);
    assert!(!f.svc(s).freed);

    // remove is ignored: the service is freed instead.
    service::deactivate(&f, &log, Some(s), true);
    assert_eq!(
        f.take(),
        [
            "deactivate_cb 0",
            "set_active 0 false",
            "destroy_cb 0 3",
            "detach 0",
            "log destroyed 0",
            "free 0",
        ]
    );
    assert!(log.0.borrow().is_empty());
}

#[test]
fn activate_checks_service_output_and_state() {
    let f = Fake::new();
    let log = Log::default();
    service::activate(&f, &log, None);
    assert_eq!(*log.0.borrow(), ["obs_service_activate:service"]);

    let s = f.live(Some(1));
    service::activate(&f, &log, Some(s));
    assert_eq!(f.take(), ["log no_output 0"]);

    f.with(s, |s| s.output = true);
    service::activate(&f, &log, Some(s));
    assert_eq!(f.take(), ["activate_cb 0", "set_active 0 true"]);

    service::activate(&f, &log, Some(s));
    assert!(f.take().is_empty());
}

#[test]
fn activate_without_a_callback_still_activates() {
    let f = Fake {
        has_activate: false,
        has_deactivate: false,
        ..Fake::new()
    };
    let log = Log::default();
    let s = f.live(None);
    f.with(s, |s| s.output = true);
    service::activate(&f, &log, Some(s));
    assert_eq!(f.take(), ["set_active 0 true"]);
    service::deactivate(&f, &log, Some(s), false);
    assert_eq!(f.take(), ["set_active 0 false"]);
}

#[test]
fn deactivate_checks_and_removes_the_output() {
    let f = Fake::new();
    let log = Log::default();
    service::deactivate(&f, &log, None, false);
    assert_eq!(*log.0.borrow(), ["obs_service_deactivate:service"]);

    let s = f.live(None);
    service::deactivate(&f, &log, Some(s), true);
    assert_eq!(f.take(), ["log no_output 0"]);

    f.with(s, |s| s.output = true);
    service::deactivate(&f, &log, Some(s), true);
    assert!(f.take().is_empty(), "inactive: nothing to do");

    f.with(s, |s| s.active = true);
    service::deactivate(&f, &log, Some(s), false);
    assert_eq!(f.take(), ["deactivate_cb 0", "set_active 0 false"]);
    assert!(f.svc(s).output);

    f.with(s, |s| s.active = true);
    service::deactivate(&f, &log, Some(s), true);
    assert_eq!(
        f.take(),
        ["deactivate_cb 0", "set_active 0 false", "clear_output 0"]
    );
}

#[test]
fn initialize_checks_both_and_defaults_to_success() {
    let f = Fake::new();
    let log = Log::default();
    assert!(!service::initialize(&f, &log, None, Some("out")));
    let s = f.live(Some(1));
    assert!(!service::initialize(&f, &log, Some(s), None));
    assert_eq!(
        *log.0.borrow(),
        [
            "obs_service_initialize:service",
            "obs_service_initialize:output"
        ]
    );
    assert!(service::initialize(&f, &log, Some(s), Some("out")));
    assert!(f.take().is_empty());

    let f = Fake {
        initialize: Some(false),
        ..Fake::new()
    };
    let s = f.live(Some(1));
    assert!(!service::initialize(&f, &log, Some(s), Some("out")));
    assert_eq!(f.take(), ["initialize_cb 0 out"]);
}

#[test]
fn release_destroys_on_the_last_ref_then_drops_the_weak_ref() {
    let f = Fake::new();
    service::release(&f, None);
    let s = f.live(Some(2));
    let r = service::get_ref(&f, Some(s));
    assert_eq!(r, Some(s));
    service::release(&f, Some(s));
    assert_eq!(f.take(), ["ref_release 0"]);

    service::release(&f, Some(s));
    assert_eq!(
        f.take(),
        [
            "ref_release 0",
            "remove 0",
            "set_destroy 0",
            "destroy_cb 0 2",
            "log destroyed 0",
            "free 0",
            "weak_release 0",
            "weak_free 0",
        ]
    );
}

#[test]
fn weak_refs_outlive_the_service() {
    let f = Fake::new();
    let s = f.live(Some(2));
    let w = service::get_weak_service(&f, Some(s)).unwrap();
    assert_eq!(f.take(), ["weak_addref 0"]);
    assert!(service::weak_references(&f, Some(w), Some(s)));
    assert!(!service::weak_references(&f, Some(w), Some(9)));
    assert!(!service::weak_references(&f, None, Some(s)));
    assert!(!service::weak_references(&f, Some(w), None));

    let strong = service::weak_get_service(&f, Some(w));
    assert_eq!(strong, Some(s));
    assert_eq!(f.svc(s).strong, 2);
    service::release(&f, strong);

    service::release(&f, Some(s));
    f.take();
    assert!(f.svc(s).freed);
    assert!(!f.svc(s).weak_freed, "the caller's weak ref is still held");
    assert_eq!(service::weak_get_service(&f, Some(w)), None);
    assert_eq!(service::get_ref(&f, Some(s)), None);

    service::weak_release(&f, Some(w));
    assert_eq!(f.take(), ["weak_release 0", "weak_free 0"]);

    service::weak_addref(&f, None);
    service::weak_release(&f, None);
    assert_eq!(service::weak_get_service(&f, None), None);
    assert_eq!(service::get_weak_service(&f, None), None);
    assert_eq!(service::get_ref(&f, None), None);
    assert!(f.take().is_empty());
}

/// Two lists of modules, each with the service ids it registered.
struct Mods {
    loaded: Vec<(&'static [&'static str], i32)>,
    disabled: Vec<(&'static [&'static str], i32)>,
}

impl Modules for Mods {
    /// (disabled, index)
    type Module = (bool, usize);
    type Str = &'static str;

    fn first(&self, disabled: bool) -> Option<(bool, usize)> {
        self.list(disabled).first().map(|_| (disabled, 0))
    }
    fn next(&self, (disabled, i): (bool, usize)) -> Option<(bool, usize)> {
        (i + 1 < self.list(disabled).len()).then_some((disabled, i + 1))
    }
    fn service_count(&self, (disabled, i): (bool, usize)) -> usize {
        self.list(disabled)[i].0.len()
    }
    fn service_is(&self, (disabled, i): (bool, usize), n: usize, id: &'static str) -> bool {
        self.list(disabled)[i].0[n] == id
    }
    fn load_state(&self, (disabled, i): (bool, usize)) -> i32 {
        self.list(disabled)[i].1
    }
}

impl Mods {
    fn list(&self, disabled: bool) -> &[(&'static [&'static str], i32)] {
        if disabled {
            &self.disabled
        } else {
            &self.loaded
        }
    }
}

#[test]
fn module_lookup_prefers_loaded_then_disabled() {
    const ENABLED: i32 = 1;
    const DISABLED: i32 = 3;
    let m = Mods {
        loaded: vec![(&[], ENABLED), (&["a", "both"], ENABLED)],
        disabled: vec![(&["b"], DISABLED), (&["both"], DISABLED)],
    };
    assert_eq!(service::module(&m, "a"), Some((false, 1)));
    assert_eq!(service::module(&m, "b"), Some((true, 0)));
    assert_eq!(service::module(&m, "both"), Some((false, 1)));
    assert_eq!(service::module(&m, "none"), None);
    assert_eq!(service::load_state(&m, "a"), ENABLED);
    assert_eq!(service::load_state(&m, "b"), DISABLED);
    assert_eq!(service::load_state(&m, "none"), service::MODULE_MISSING);

    let empty = Mods {
        loaded: vec![],
        disabled: vec![],
    };
    assert_eq!(service::load_state(&empty, "a"), service::MODULE_MISSING);
}
