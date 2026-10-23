//! Tier 1 for obs_output::service, settings, properties and identity
//! (#1913): the cases of test/cmocka/test_service_settings.c, against a
//! host, a type and a service in Rust.

use core::cell::RefCell;
use core::ffi::CStr;

use obs_output::service::{self, Host, NullLog, Service, Type};

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

/// Data objects are indices into `objects`; every call is traced.
#[derive(Default)]
struct Fake {
    objects: RefCell<Vec<Vec<(String, i64)>>>,
    trace: RefCell<Vec<String>>,
}

impl Fake {
    fn ev(&self, e: String) {
        self.trace.borrow_mut().push(e);
    }
    fn take(&self) -> Vec<String> {
        core::mem::take(&mut *self.trace.borrow_mut())
    }
    fn new_obj(&self, items: &[(&str, i64)]) -> usize {
        let mut o = self.objects.borrow_mut();
        o.push(items.iter().map(|(k, v)| ((*k).to_owned(), *v)).collect());
        o.len() - 1
    }
    fn get(&self, obj: usize, key: &str) -> Option<i64> {
        self.objects.borrow()[obj]
            .iter()
            .find(|(k, _)| k == key)
            .map(|(_, v)| *v)
    }
}

impl Host for Fake {
    type Data = usize;
    type Props = Option<&'static str>;

    fn data_create(&self) -> usize {
        let id = self.new_obj(&[]);
        self.ev(format!("create {id}"));
        id
    }
    fn data_apply(&self, target: usize, apply: Option<usize>) {
        self.ev(format!("apply {target} {apply:?}"));
        if let Some(apply) = apply {
            let src = self.objects.borrow()[apply].clone();
            let mut o = self.objects.borrow_mut();
            for (k, v) in src {
                o[target].retain(|(key, _)| *key != k);
                o[target].push((k, v));
            }
        }
    }
    fn data_addref(&self, data: usize) {
        self.ev(format!("addref {data}"));
    }
    fn data_release(&self, data: usize) {
        self.ev(format!("release {data}"));
    }
    fn properties_apply_settings(&self, props: Option<&'static str>, settings: usize) {
        self.ev(format!(
            "props_apply {props:?} bitrate={:?}",
            self.get(settings, "bitrate")
        ));
    }
}

/// "full" has every optional callback, "bare" only the required ones.
struct Ty<'a> {
    host: &'a Fake,
    full: bool,
}

impl Type<Fake> for Ty<'_> {
    type Str = &'static str;
    type Ptr = usize;

    fn id(&self) -> &'static str {
        if self.full { "full" } else { "bare" }
    }
    fn type_data(&self) -> usize {
        if self.full { 77 } else { 0 }
    }
    fn display_name(&self) -> &'static str {
        if self.full {
            "Settings full"
        } else {
            "Settings bare"
        }
    }
    fn defaults(&self, settings: usize) -> bool {
        if !self.full {
            return false;
        }
        self.host.ev(format!("defaults {settings}"));
        self.host
            .objects
            .borrow_mut()
            .get_mut(settings)
            .unwrap()
            .push(("bitrate".into(), 2500));
        true
    }
    fn has_properties(&self) -> bool {
        self.full
    }
    fn properties(&self, data: usize) -> Option<&'static str> {
        self.host.ev(format!("get_properties {data}"));
        Some("props")
    }
    fn update(&self, data: usize, settings: usize) -> bool {
        if !self.full {
            return false;
        }
        self.host.ev(format!(
            "update {data} bitrate={:?}",
            self.host.get(settings, "bitrate")
        ));
        true
    }
    fn has_apply_encoder_settings(&self) -> bool {
        self.full
    }
    fn apply_encoder_settings(&self, data: usize, video: Option<usize>, audio: Option<usize>) {
        self.host
            .ev(format!("apply_encoder {data} {video:?} {audio:?}"));
    }
}

struct Svc<'a> {
    ty: Ty<'a>,
    settings: usize,
}

impl<'a> Service<Fake> for Svc<'a> {
    type Type = Ty<'a>;

    fn info(&self) -> &Ty<'a> {
        &self.ty
    }
    fn data(&self) -> usize {
        42
    }
    fn name(&self) -> &'static str {
        if self.ty.full { "full" } else { "bare" }
    }
    fn settings(&self) -> usize {
        self.settings
    }
}

fn svc(host: &Fake, full: bool) -> Svc<'_> {
    let settings = host.new_obj(&[("bitrate", 2500)]);
    Svc {
        ty: Ty { host, full },
        settings,
    }
}

#[test]
fn display_name_by_id() {
    let h = Fake::default();
    let full = Ty {
        host: &h,
        full: true,
    };
    let bare = Ty {
        host: &h,
        full: false,
    };
    assert_eq!(service::display_name(Some(&full)), Some("Settings full"));
    assert_eq!(service::display_name(Some(&bare)), Some("Settings bare"));
    assert_eq!(service::display_name::<Fake, Ty>(None), None);
}

#[test]
fn defaults_by_id() {
    let h = Fake::default();
    let d = service::defaults(
        &h,
        Some(&Ty {
            host: &h,
            full: true,
        }),
    )
    .unwrap();
    assert_eq!(h.get(d, "bitrate"), Some(2500));
    assert_eq!(h.take(), [format!("create {d}"), format!("defaults {d}")]);

    // No get_defaults: an empty object, not NULL.
    let d = service::defaults(
        &h,
        Some(&Ty {
            host: &h,
            full: false,
        }),
    )
    .unwrap();
    assert!(h.objects.borrow()[d].is_empty());
    assert_eq!(service::defaults::<Fake, Ty>(&h, None), None);
}

#[test]
fn properties_by_id() {
    let h = Fake::default();
    let p = service::type_properties(
        &h,
        Some(&Ty {
            host: &h,
            full: true,
        }),
    );
    assert_eq!(p, Some(Some("props")));
    // Defaults first, then get_properties(NULL), apply, release.
    assert_eq!(
        h.take(),
        [
            "create 0",
            "defaults 0",
            "get_properties 0",
            "props_apply Some(\"props\") bitrate=Some(2500)",
            "release 0"
        ]
    );
    assert_eq!(
        service::type_properties(
            &h,
            Some(&Ty {
                host: &h,
                full: false
            })
        ),
        None
    );
    assert!(h.take().is_empty());
    assert_eq!(service::type_properties::<Fake, Ty>(&h, None), None);
}

#[test]
fn identity() {
    let h = Fake::default();
    let log = Log::default();
    let full = svc(&h, true);
    let bare = svc(&h, false);
    assert_eq!(service::name(Some(&full), &log), Some("full"));
    assert_eq!(service::type_id(Some(&full), &log), Some("full"));
    assert_eq!(service::id(Some(&full), &log), Some("full"));
    assert_eq!(service::type_data(Some(&full), &log), Some(77));
    assert_eq!(service::type_data(Some(&bare), &log), Some(0));
    assert!(log.0.borrow().is_empty());

    let none: Option<&Svc> = None;
    assert_eq!(service::name(none, &log), None);
    assert_eq!(service::type_id(none, &log), None);
    assert_eq!(service::id(none, &log), None);
    assert_eq!(service::type_data(none, &log), None);
    assert_eq!(
        *log.0.borrow(),
        [
            "obs_service_get_name:service",
            "obs_service_get_type:service",
            "obs_service_get_id:service",
            "obs_service_get_type_data:service",
        ]
    );
}

#[test]
fn properties_on_service() {
    let h = Fake::default();
    let log = Log::default();
    let full = svc(&h, true);
    assert_eq!(
        service::properties(&h, Some(&full), &log),
        Some(Some("props"))
    );
    assert_eq!(
        h.take(),
        [
            "get_properties 42",
            "props_apply Some(\"props\") bitrate=Some(2500)"
        ]
    );
    assert_eq!(service::properties(&h, Some(&svc(&h, false)), &log), None);
    assert_eq!(service::properties::<Fake, Svc>(&h, None, &log), None);
    assert_eq!(*log.0.borrow(), ["obs_service_properties:service"]);
}

#[test]
fn update_and_settings() {
    let h = Fake::default();
    let log = Log::default();
    let full = svc(&h, true);
    let new = h.new_obj(&[("bitrate", 4000)]);
    service::update(&h, Some(&full), &log, Some(new));
    assert_eq!(
        h.take(),
        [
            format!("apply {} Some({new})", full.settings),
            "update 42 bitrate=Some(4000)".to_owned()
        ]
    );

    assert_eq!(
        service::settings(&h, Some(&full), &log),
        Some(full.settings)
    );
    assert_eq!(h.take(), [format!("addref {}", full.settings)]);

    // No update callback: the settings still change. NULL settings: applied
    // as NULL.
    let bare = svc(&h, false);
    service::update(&h, Some(&bare), &log, None);
    assert_eq!(h.take(), [format!("apply {} None", bare.settings)]);

    service::update::<Fake, Svc>(&h, None, &log, Some(new));
    assert_eq!(service::settings::<Fake, Svc>(&h, None, &log), None);
    assert!(h.take().is_empty());
    assert_eq!(
        *log.0.borrow(),
        [
            "obs_service_update:service",
            "obs_service_get_settings:service"
        ]
    );
}

#[test]
fn apply_encoder_settings() {
    let h = Fake::default();
    let log = Log::default();
    let full = svc(&h, true);
    service::apply_encoder_settings(Some(&full), &log, Some(5), Some(6));
    service::apply_encoder_settings(Some(&full), &log, Some(5), None);
    service::apply_encoder_settings(Some(&full), &log, None, Some(6));
    // Both NULL: not called.
    service::apply_encoder_settings(Some(&full), &log, None, None);
    assert_eq!(
        h.take(),
        [
            "apply_encoder 42 Some(5) Some(6)",
            "apply_encoder 42 Some(5) None",
            "apply_encoder 42 None Some(6)"
        ]
    );
    service::apply_encoder_settings(Some(&svc(&h, false)), &log, Some(5), Some(6));
    service::apply_encoder_settings::<Fake, Svc>(None, &log, Some(5), Some(6));
    assert!(h.take().is_empty());
    assert_eq!(
        *log.0.borrow(),
        ["obs_service_apply_encoder_settings:service"]
    );
}
