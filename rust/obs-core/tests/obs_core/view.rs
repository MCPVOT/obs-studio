//! Tier 1: the view core (`obs_core::view`) against a recording host.
//!
//! The host keeps a view's channels, type and lock state and a source's
//! removed flag, and logs every step, so each case checks both the result
//! and the C's order of locks, references and activations.

use std::cell::{Cell, RefCell};
use std::collections::HashMap;

use obs_core::view::{
    AUX_VIEW, Handle, Host, MAIN_VIEW, MAX_CHANNELS, add, add2, create, destroy, enum_video_info,
    free, get_source, init, remove, render, set_source,
};

#[derive(Default)]
struct Fake {
    log: RefCell<Vec<String>>,
    channels: RefCell<HashMap<(Handle, usize), Handle>>,
    types: RefCell<HashMap<Handle, u32>>,
    locked: RefCell<HashMap<Handle, bool>>,
    removed: RefCell<Vec<Handle>>,
    dying: RefCell<Vec<Handle>>,
    mutex_fails: Cell<bool>,
    next_view: Cell<Handle>,
    main_info: Cell<Handle>,
    next_video: Cell<Handle>,
    mixes: RefCell<Vec<(Handle, Handle)>>,
    mixes_locked: Cell<bool>,
}

impl Fake {
    fn new() -> Self {
        let f = Self::default();
        f.next_view.set(0x1000);
        f.next_video.set(0x9000);
        f
    }

    fn push(&self, s: impl Into<String>) {
        self.log.borrow_mut().push(s.into());
    }

    fn take(&self) -> Vec<String> {
        std::mem::take(&mut self.log.borrow_mut())
    }

    fn get(&self, v: Handle, ch: usize) -> Handle {
        *self.channels.borrow().get(&(v, ch)).unwrap_or(&0)
    }

    fn view(&self) -> Handle {
        let v = create(self);
        assert_ne!(v, 0);
        self.take();
        v
    }
}

impl Host for Fake {
    fn alloc_view(&self) -> Handle {
        let v = self.next_view.get();
        self.next_view.set(v + 0x100);
        self.push(format!("alloc {v:#x}"));
        v
    }
    fn free_view(&self, view: Handle) {
        self.push(format!("free {view:#x}"));
    }
    fn mutex_init(&self, view: Handle) -> bool {
        self.push(format!("mutex_init {view:#x}"));
        !self.mutex_fails.get()
    }
    fn mutex_destroy(&self, view: Handle) {
        self.push(format!("mutex_destroy {view:#x}"));
    }
    fn lock(&self, view: Handle) {
        assert!(!self.locked.borrow().get(&view).copied().unwrap_or(false));
        self.locked.borrow_mut().insert(view, true);
        self.push("lock");
    }
    fn unlock(&self, view: Handle) {
        assert!(self.locked.borrow().get(&view).copied().unwrap_or(false));
        self.locked.borrow_mut().insert(view, false);
        self.push("unlock");
    }
    fn view_type(&self, view: Handle) -> u32 {
        *self.types.borrow().get(&view).unwrap_or(&0)
    }
    fn set_view_type(&self, view: Handle, view_type: u32) {
        self.types.borrow_mut().insert(view, view_type);
        self.push(format!("type {view_type}"));
    }
    fn channel(&self, view: Handle, channel: usize) -> Handle {
        self.get(view, channel)
    }
    fn set_channel(&self, view: Handle, channel: usize, source: Handle) {
        self.channels.borrow_mut().insert((view, channel), source);
        self.push(format!("set {channel} {source:#x}"));
    }
    fn clear_channels(&self, view: Handle) {
        self.channels.borrow_mut().retain(|k, _| k.0 != view);
        self.push("clear");
    }
    fn log_mutex_failed(&self) {
        self.push("log mutex");
    }
    fn source_get_ref(&self, source: Handle) -> Handle {
        self.push(format!("get_ref {source:#x}"));
        if source == 0 || self.dying.borrow().contains(&source) {
            0
        } else {
            source
        }
    }
    fn source_release(&self, source: Handle) {
        self.push(format!("release {source:#x}"));
    }
    fn source_activate(&self, source: Handle, view_type: u32) {
        self.push(format!("activate {source:#x} {view_type}"));
    }
    fn source_deactivate(&self, source: Handle, view_type: u32) {
        self.push(format!("deactivate {source:#x} {view_type}"));
    }
    fn source_video_render(&self, source: Handle) {
        self.push(format!("render {source:#x}"));
    }
    fn source_removed(&self, source: Handle) -> bool {
        self.removed.borrow().contains(&source)
    }
    fn main_mix_info(&self) -> Handle {
        self.main_info.get()
    }
    fn mix_add(&self, view: Handle, ovi: Handle) -> Handle {
        self.push(format!("mix_add {view:#x} {ovi:#x}"));
        let video = self.next_video.get();
        self.next_video.set(video + 1);
        self.mixes.borrow_mut().push((view, ovi));
        video
    }
    fn mixes_lock(&self) {
        assert!(!self.mixes_locked.get());
        self.mixes_locked.set(true);
        self.push("mixes_lock");
    }
    fn mixes_unlock(&self) {
        assert!(self.mixes_locked.get());
        self.mixes_locked.set(false);
        self.push("mixes_unlock");
    }
    fn mix_count(&self) -> usize {
        self.mixes.borrow().len()
    }
    fn mix_view(&self, index: usize) -> Handle {
        self.mixes.borrow()[index].0
    }
    fn mix_clear_view(&self, index: usize) {
        self.mixes.borrow_mut()[index].0 = 0;
        self.push(format!("mix_clear {index}"));
    }
    fn mix_info(&self, index: usize) -> Handle {
        self.mixes.borrow()[index].1
    }
}

#[test]
fn view_create_is_an_aux_view() {
    let h = Fake::new();
    let v = create(&h);
    assert_eq!(v, 0x1000);
    assert_eq!(h.take(), ["alloc 0x1000", "mutex_init 0x1000", "type 2"]);
    assert_eq!(h.view_type(v), AUX_VIEW);
}

#[test]
fn view_create_frees_when_the_mutex_fails() {
    let h = Fake::new();
    h.mutex_fails.set(true);
    assert_eq!(create(&h), 0);
    assert_eq!(
        h.take(),
        [
            "alloc 0x1000",
            "mutex_init 0x1000",
            "log mutex",
            "free 0x1000"
        ]
    );
}

#[test]
fn view_init_null_and_main() {
    let h = Fake::new();
    assert!(!init(&h, 0, MAIN_VIEW));
    assert!(h.take().is_empty());
    assert!(init(&h, 0x42, MAIN_VIEW));
    assert_eq!(h.take(), ["mutex_init 0x42", "type 1"]);
}

#[test]
fn view_set_source_order() {
    let h = Fake::new();
    let v = h.view();
    set_source(&h, v, 3, 0xa0);
    assert_eq!(
        h.take(),
        [
            "lock",
            "get_ref 0xa0",
            "set 3 0xa0",
            "unlock",
            "activate 0xa0 2"
        ]
    );
    set_source(&h, v, 3, 0xb0);
    assert_eq!(
        h.take(),
        [
            "lock",
            "get_ref 0xb0",
            "set 3 0xb0",
            "unlock",
            "activate 0xb0 2",
            "deactivate 0xa0 2",
            "release 0xa0",
        ]
    );
    set_source(&h, v, 3, 0);
    assert_eq!(
        h.take(),
        [
            "lock",
            "get_ref 0x0",
            "set 3 0x0",
            "unlock",
            "deactivate 0xb0 2",
            "release 0xb0",
        ]
    );
}

#[test]
fn view_set_source_that_is_being_destroyed_clears_the_channel() {
    let h = Fake::new();
    let v = h.view();
    set_source(&h, v, 0, 0xa0);
    h.take();
    h.dying.borrow_mut().push(0xb0);
    set_source(&h, v, 0, 0xb0);
    assert_eq!(h.get(v, 0), 0);
    assert_eq!(
        h.take(),
        [
            "lock",
            "get_ref 0xb0",
            "set 0 0x0",
            "unlock",
            "deactivate 0xa0 2",
            "release 0xa0",
        ]
    );
}

#[test]
fn view_channel_bounds_and_null() {
    let h = Fake::new();
    let v = h.view();
    set_source(&h, v, MAX_CHANNELS as u32, 0xa0);
    set_source(&h, 0, 0, 0xa0);
    assert_eq!(get_source(&h, v, MAX_CHANNELS as u32), 0);
    assert_eq!(get_source(&h, v, u32::MAX), 0);
    assert_eq!(get_source(&h, 0, 0), 0);
    render(&h, 0);
    remove(&h, 0);
    assert_eq!(add2(&h, 0, 0x5), 0);
    assert_eq!(add2(&h, v, 0), 0);
    free(&h, 0);
    destroy(&h, 0);
    assert!(h.take().is_empty());
    set_source(&h, v, MAX_CHANNELS as u32 - 1, 0xa0);
    assert_eq!(h.get(v, MAX_CHANNELS - 1), 0xa0);
}

#[test]
fn view_get_source_takes_a_reference_under_the_lock() {
    let h = Fake::new();
    let v = h.view();
    set_source(&h, v, 7, 0xa0);
    h.take();
    assert_eq!(get_source(&h, v, 7), 0xa0);
    assert_eq!(h.take(), ["lock", "get_ref 0xa0", "unlock"]);
    assert_eq!(get_source(&h, v, 8), 0);
    assert_eq!(h.take(), ["lock", "get_ref 0x0", "unlock"]);
}

#[test]
fn view_render_drops_removed_sources_without_deactivating() {
    let h = Fake::new();
    let v = h.view();
    set_source(&h, v, 1, 0xa0);
    set_source(&h, v, 4, 0xb0);
    h.take();
    h.removed.borrow_mut().push(0xa0);
    render(&h, v);
    assert_eq!(
        h.take(),
        ["lock", "release 0xa0", "set 1 0x0", "render 0xb0", "unlock"]
    );
    assert_eq!(h.get(v, 1), 0);
}

#[test]
fn view_free_and_destroy() {
    let h = Fake::new();
    let v = h.view();
    set_source(&h, v, 2, 0xa0);
    set_source(&h, v, 9, 0xb0);
    h.take();
    destroy(&h, v);
    assert_eq!(
        h.take(),
        [
            "deactivate 0xa0 2",
            "release 0xa0",
            "deactivate 0xb0 2",
            "release 0xb0",
            "clear",
            "mutex_destroy 0x1000",
            "free 0x1000",
        ]
    );
}

#[test]
fn view_free_uses_the_view_type() {
    let h = Fake::new();
    assert!(init(&h, 0x77, MAIN_VIEW));
    set_source(&h, 0x77, 0, 0xa0);
    h.take();
    free(&h, 0x77);
    assert_eq!(
        h.take(),
        [
            "deactivate 0xa0 1",
            "release 0xa0",
            "clear",
            "mutex_destroy 0x77"
        ]
    );
}

#[test]
fn view_add_uses_the_main_mix() {
    let h = Fake::new();
    let v = h.view();
    assert_eq!(add(&h, v), 0);
    assert!(h.take().is_empty());
    h.main_info.set(0x55);
    assert_eq!(add(&h, v), 0x9000);
    assert_eq!(h.take(), [format!("mix_add {v:#x} 0x55")]);
    // A NULL view with a main mix: add2 refuses it.
    assert_eq!(add(&h, 0), 0);
    assert!(h.take().is_empty());
}

#[test]
fn view_remove_and_enum() {
    let h = Fake::new();
    let a = h.view();
    let b = h.view();
    add2(&h, a, 0x1);
    add2(&h, b, 0x2);
    add2(&h, a, 0x3);
    h.take();

    let mut seen = Vec::new();
    enum_video_info(&h, a, |ovi| {
        seen.push(ovi);
        true
    });
    assert_eq!(seen, [0x1, 0x3]);
    assert_eq!(h.take(), ["mixes_lock", "mixes_unlock"]);

    let mut seen = Vec::new();
    enum_video_info(&h, a, |ovi| {
        seen.push(ovi);
        false
    });
    assert_eq!(seen, [0x1]);
    h.take();

    remove(&h, a);
    assert_eq!(
        h.take(),
        ["mixes_lock", "mix_clear 0", "mix_clear 2", "mixes_unlock"]
    );
    let mut seen = Vec::new();
    enum_video_info(&h, a, |ovi| {
        seen.push(ovi);
        true
    });
    assert!(seen.is_empty());

    // NULL matches the mixes whose view was removed (no NULL check).
    let mut seen = Vec::new();
    enum_video_info(&h, 0, |ovi| {
        seen.push(ovi);
        true
    });
    assert_eq!(seen, [0x1, 0x3]);
}
