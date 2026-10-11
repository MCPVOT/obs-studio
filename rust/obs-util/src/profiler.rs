//! Safe core for `libobs/util/profiler.c`.
//!
//! The C ABI shim lives in [`crate::ffi::profiler`]. Names are keyed by
//! pointer identity, as in C. `profile_end` pulls a timestamp from the
//! caller's clock once on entry (and again for each mismatched frame it
//! closes), which is where `profiler.c` calls `os_gettime_ns`.

use core::cell::{Cell, RefCell};
use core::ffi::{CStr, c_char, c_void};
use std::sync::{Arc, Mutex, MutexGuard};

use crate::ffi::bmem::bfree;

/// `LOG_ERROR` in `util/base.h`.
const LOG_ERROR: i32 = 100;

unsafe extern "C" {
    fn blog(log_level: i32, format: *const c_char, ...);
}

/// A profile name. Equality is the pointer address, not the bytes.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Hash)]
pub struct ProfileName(usize);

impl ProfileName {
    /// The address of a NUL-terminated string that lives for the whole
    /// process. The profiler reads names back as C strings.
    pub fn from_static(name: &'static CStr) -> Self {
        Self(name.as_ptr() as usize)
    }

    /// # Safety
    ///
    /// `name` must stay valid for as long as the profiler stores it, which
    /// is what `profile_start` requires of its caller.
    pub fn new(name: *const c_char) -> Self {
        Self(name as usize)
    }

    pub fn as_ptr(self) -> *const c_char {
        self.0 as *const c_char
    }

    pub fn is_null(self) -> bool {
        self.0 == 0
    }
}

/// One `(time_delta, count)` bucket from a snapshot.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct TimeEntry {
    pub time_delta: u64,
    pub count: u64,
}

/// One node of a snapshot, in registration / first-seen order.
#[derive(Clone, Debug, Default)]
pub struct SnapshotEntry {
    pub name: ProfileName,
    pub times: Vec<TimeEntry>,
    pub min_time: u64,
    pub max_time: u64,
    pub overall_count: u64,
    pub times_between_calls: Vec<TimeEntry>,
    pub expected_time_between_calls: u64,
    pub min_time_between_calls: u64,
    pub max_time_between_calls: u64,
    pub overall_between_calls_count: u64,
    pub children: Vec<SnapshotEntry>,
}

/// Roots of a profiler snapshot.
#[derive(Clone, Debug, Default)]
pub struct Snapshot {
    pub roots: Vec<SnapshotEntry>,
}

/// Owned copies of formatted profile names.
///
/// Each store returns a distinct pointer, including for equal bytes. The
/// pointers stay valid until the store is dropped.
#[derive(Default)]
pub struct NameStore {
    names: Mutex<Vec<Vec<u8>>>,
}

impl NameStore {
    pub fn new() -> Self {
        Self::default()
    }

    /// Copies `text` and returns the stored C string.
    pub fn store(&self, text: &str) -> *const c_char {
        let mut bytes = Vec::with_capacity(text.len() + 1);
        bytes.extend_from_slice(text.as_bytes());
        bytes.push(0);
        self.keep(bytes)
    }

    /// Takes ownership of a `bmalloc`'d NUL-terminated buffer.
    ///
    /// # Safety
    ///
    /// `owned` is null or a `bmalloc` allocation this function frees.
    pub unsafe fn push_owned(&self, owned: *mut c_char) -> *const c_char {
        if owned.is_null() {
            return self.keep(Vec::new());
        }
        let mut len = 0usize;
        // SAFETY: `owned` is a NUL-terminated allocation.
        while unsafe { *owned.add(len) } != 0 {
            len += 1;
        }
        // SAFETY: the bytes through the NUL are readable.
        let bytes = unsafe { core::slice::from_raw_parts(owned.cast::<u8>(), len + 1) }.to_vec();
        // SAFETY: the caller transferred the `bmalloc` allocation.
        unsafe { bfree(owned.cast::<c_void>()) };
        self.keep(bytes)
    }

    fn keep(&self, bytes: Vec<u8>) -> *const c_char {
        let ptr = if bytes.is_empty() {
            core::ptr::null()
        } else {
            bytes.as_ptr().cast()
        };
        self.names.lock().unwrap().push(bytes);
        ptr
    }
}

/// Frees a store from the C ABI. Null is a no-op.
///
/// # Safety
///
/// `store` is null or a pointer returned by [`name_store_create`].
pub unsafe fn name_store_free(store: *mut NameStore) {
    if store.is_null() {
        return;
    }
    // SAFETY: the caller transferred a box from `name_store_create`.
    drop(unsafe { Box::from_raw(store) });
}

/// Boxes a store for the C ABI.
pub fn name_store_create() -> *mut NameStore {
    Box::into_raw(Box::new(NameStore::new()))
}

#[derive(Clone, Copy, Default)]
struct Slot {
    probes: usize,
    time_delta: u64,
    count: u64,
}

/// Open-addressed map from `profiler.c` (`add_hashmap_entry` and its grow).
struct TimesTable {
    size: usize,
    occupied: usize,
    entries: Vec<Slot>,
    old_start_index: usize,
    old_occupied: usize,
    old_entries: Option<Vec<Slot>>,
}

impl TimesTable {
    fn new(size: usize) -> Self {
        Self {
            size,
            occupied: 0,
            entries: vec![Slot::default(); size],
            old_start_index: 0,
            old_occupied: 0,
            old_entries: None,
        }
    }

    fn add(&mut self, mut usec: u64, mut count: u64) {
        let mut probes = 1usize;
        let mut start = (usec % self.size as u64) as usize;
        loop {
            let idx = (start + probes) % self.size;
            if self.entries[idx].probes == 0 {
                self.entries[idx] = Slot {
                    probes,
                    time_delta: usec,
                    count,
                };
                self.occupied += 1;
                return;
            }
            if self.entries[idx].time_delta == usec {
                self.entries[idx].count += count;
                return;
            }
            if self.entries[idx].probes >= probes {
                probes += 1;
                continue;
            }
            if (self.occupied as f64) / (self.size as f64) > 0.7 {
                self.grow(usec, count);
                return;
            }
            let old = self.entries[idx];
            self.entries[idx] = Slot {
                probes,
                time_delta: usec,
                count,
            };
            probes = old.probes;
            count = old.count;
            usec = old.time_delta;
            start = (usec % self.size as u64) as usize;
            probes += 1;
        }
    }

    /// Moves at most eight old entries when `limit_items` is set, otherwise
    /// all of them. An empty leftover is freed on the next call, as in C.
    fn migrate(&mut self, limit_items: bool) {
        if self.old_entries.is_none() {
            return;
        }
        if self.old_occupied == 0 {
            self.old_entries = None;
            return;
        }
        let mut moved = 0usize;
        loop {
            if limit_items && moved >= 8 {
                return;
            }
            if self.old_occupied == 0 {
                return;
            }
            let idx = self.old_start_index;
            let Some(slot) = self.old_entries.as_ref().and_then(|e| e.get(idx).copied()) else {
                return;
            };
            self.old_start_index += 1;
            moved += 1;
            if slot.probes == 0 {
                continue;
            }
            self.old_occupied -= 1;
            self.add(slot.time_delta, slot.count);
        }
    }

    fn grow(&mut self, usec: u64, count: u64) {
        self.migrate(false);
        let old_occupied = self.occupied;
        let old_entries = core::mem::take(&mut self.entries);
        let doubled = self.size.wrapping_mul(2);
        let new_size = if doubled < 16 { 16 } else { doubled };
        *self = Self::new(new_size);
        self.old_occupied = old_occupied;
        self.old_entries = Some(old_entries);
        self.add(usec, count);
    }

    fn copy_times(&mut self) -> (Vec<TimeEntry>, u64, u64, u64) {
        self.migrate(false);
        let mut min = u64::MAX;
        let mut max = 0u64;
        let mut calls = 0u64;
        let mut items = Vec::new();
        for slot in &self.entries {
            if slot.probes == 0 {
                continue;
            }
            items.push(TimeEntry {
                time_delta: slot.time_delta,
                count: slot.count,
            });
            calls += slot.count;
            if min >= slot.time_delta {
                min = slot.time_delta;
            }
            if max <= slot.time_delta {
                max = slot.time_delta;
            }
        }
        // `profiler_time_entry_compare` sorts `time_delta` descending. Keys
        // are unique, so the order does not depend on `qsort` being unstable.
        items.sort_by_key(|entry| core::cmp::Reverse(entry.time_delta));
        (items, min, max, calls)
    }
}

struct ProfileEntry {
    name: ProfileName,
    times: TimesTable,
    expected: u64,
    between: TimesTable,
    children: Vec<ProfileEntry>,
}

impl ProfileEntry {
    fn new(name: ProfileName) -> Self {
        Self {
            name,
            times: TimesTable::new(1),
            expected: 0,
            between: TimesTable::new(1),
            children: Vec::new(),
        }
    }

    fn child_index(&mut self, name: ProfileName) -> usize {
        if let Some(i) = self.children.iter().position(|c| c.name == name) {
            return i;
        }
        self.children.push(Self::new(name));
        self.children.len() - 1
    }

    fn snapshot_of(&mut self) -> SnapshotEntry {
        let (times, min_time, max_time, overall_count) = self.times.copy_times();
        let mut out = SnapshotEntry {
            name: self.name,
            times,
            min_time,
            max_time,
            overall_count,
            expected_time_between_calls: self.expected,
            ..SnapshotEntry::default()
        };
        if self.expected != 0 {
            let (between, min_b, max_b, overall_b) = self.between.copy_times();
            out.times_between_calls = between;
            out.min_time_between_calls = min_b;
            out.max_time_between_calls = max_b;
            out.overall_between_calls_count = overall_b;
        }
        out.children = self
            .children
            .iter_mut()
            .map(ProfileEntry::snapshot_of)
            .collect();
        out
    }
}

struct Node {
    name: ProfileName,
    start: u64,
    end: u64,
    parent: Option<usize>,
    children: Vec<usize>,
}

struct Active {
    nodes: Vec<Node>,
}

impl Active {
    fn root_start(&self) -> u64 {
        self.nodes.first().map(|n| n.start).unwrap_or(0)
    }
}

struct Root {
    name: ProfileName,
    entry: Mutex<ProfileEntry>,
    prev: Mutex<Option<Arc<Active>>>,
}

struct Globals {
    enabled: bool,
    roots: Vec<Arc<Root>>,
}

static GLOBAL: Mutex<Globals> = Mutex::new(Globals {
    enabled: false,
    roots: Vec::new(),
});

thread_local! {
    static THREAD_ENABLED: Cell<bool> = const { Cell::new(true) };
    static ACTIVE: RefCell<Option<Active>> = const { RefCell::new(None) };
    static STACK: RefCell<Vec<usize>> = const { RefCell::new(Vec::new()) };
}

pub fn thread_enabled() -> bool {
    THREAD_ENABLED.with(Cell::get)
}

fn set_thread_enabled(enabled: bool) {
    THREAD_ENABLED.with(|c| c.set(enabled));
}

fn diff_ns_to_usec(prev: u64, next: u64) -> u64 {
    next.wrapping_sub(prev).wrapping_add(500) / 1000
}

fn lock_globals() -> MutexGuard<'static, Globals> {
    GLOBAL.lock().unwrap()
}

fn get_root(globals: &mut Globals, name: ProfileName) -> Arc<Root> {
    if let Some(root) = globals.roots.iter().find(|r| r.name == name) {
        return Arc::clone(root);
    }
    let root = Arc::new(Root {
        name,
        entry: Mutex::new(ProfileEntry::new(name)),
        prev: Mutex::new(None),
    });
    globals.roots.push(Arc::clone(&root));
    root
}

/// Enables recording. The root mutex stays for the process; see
/// [`profiler_free`].
pub fn profiler_start() {
    lock_globals().enabled = true;
}

pub fn profiler_stop() {
    lock_globals().enabled = false;
}

/// Drops recorded roots and disables the profiler.
///
/// The C root mutex is destroyed here. This port keeps the process-lifetime
/// mutex, so a later [`profiler_start`] is defined instead of locking a
/// destroyed mutex.
pub fn profiler_free() {
    let mut globals = lock_globals();
    globals.enabled = false;
    globals.roots.clear();
}

/// Clears roots and this thread's call stack.
///
/// The parity harness calls this between cases. It is not a C export:
/// `profiler_free` leaves a thread-local frame alone, and the C mutex cannot
/// be reused after that call.
pub fn testing_reset() {
    profiler_free();
    ACTIVE.with(|slot| *slot.borrow_mut() = None);
    STACK.with(|slot| slot.borrow_mut().clear());
    set_thread_enabled(true);
}

pub fn profile_reenable_thread() {
    if thread_enabled() {
        return;
    }
    let enabled = lock_globals().enabled;
    set_thread_enabled(enabled);
}

pub fn profile_register_root(name: ProfileName, expected_time_between_calls_ns: u64) {
    let mut globals = lock_globals();
    if !globals.enabled {
        drop(globals);
        set_thread_enabled(false);
        return;
    }
    let root = get_root(&mut globals, name);
    let usec = expected_time_between_calls_ns.wrapping_add(500) / 1000;
    root.entry.lock().unwrap().expected = usec;
}

pub fn profile_start(name: ProfileName, now_ns: u64) {
    if !thread_enabled() {
        return;
    }
    ACTIVE.with(|active_slot| {
        STACK.with(|stack_slot| {
            let mut active_ref = active_slot.borrow_mut();
            let active = active_ref.get_or_insert_with(|| Active { nodes: Vec::new() });
            let mut stack = stack_slot.borrow_mut();
            let parent = stack.last().copied();
            let idx = active.nodes.len();
            active.nodes.push(Node {
                name,
                start: now_ns,
                end: 0,
                parent,
                children: Vec::new(),
            });
            if let Some(p) = parent {
                active.nodes[p].children.push(idx);
            }
            stack.push(idx);
        });
    });
}

fn log_no_active() {
    unsafe {
        blog(
            LOG_ERROR,
            c"Called profile end with no active profile".as_ptr(),
        );
    }
}

fn log_mismatch(started: ProfileName, ended: ProfileName) {
    unsafe {
        blog(
            LOG_ERROR,
            c"Called profile end with mismatching name: start(\"%s\"[%p]) <-> end(\"%s\"[%p])"
                .as_ptr(),
            started.as_ptr(),
            started.as_ptr().cast::<c_void>(),
            ended.as_ptr(),
            ended.as_ptr().cast::<c_void>(),
        );
    }
}

fn top_name() -> Option<ProfileName> {
    STACK.with(|stack_slot| {
        let stack = stack_slot.borrow();
        let idx = *stack.last()?;
        ACTIVE.with(|active_slot| {
            active_slot
                .borrow()
                .as_ref()
                .map(|active| active.nodes[idx].name)
        })
    })
}

/// Ends the active call.
///
/// `clock` is read immediately, including when this thread is disabled, and
/// once more for every inner frame a mismatched end closes.
pub fn profile_end(name: ProfileName, clock: &mut dyn FnMut() -> u64) {
    let now = clock();
    if !thread_enabled() {
        return;
    }
    let Some(mut active) = ACTIVE.with(|slot| slot.borrow_mut().take()) else {
        log_no_active();
        return;
    };
    let Some(cur) = STACK.with(|slot| slot.borrow().last().copied()) else {
        ACTIVE.with(|slot| *slot.borrow_mut() = Some(active));
        log_no_active();
        return;
    };
    if active.nodes[cur].name.is_null() {
        active.nodes[cur].name = name;
    }
    if active.nodes[cur].name != name {
        log_mismatch(active.nodes[cur].name, name);
        let mut parent = active.nodes[cur].parent;
        while let Some(p) = parent {
            if active.nodes[p].parent.is_some() && active.nodes[p].name != name {
                parent = active.nodes[p].parent;
            } else {
                break;
            }
        }
        let found = matches!(parent, Some(p) if active.nodes[p].name == name);
        ACTIVE.with(|slot| *slot.borrow_mut() = Some(active));
        if !found {
            return;
        }
        while top_name() != Some(name) {
            let Some(top) = top_name() else {
                return;
            };
            profile_end(top, clock);
        }
        active = ACTIVE
            .with(|slot| slot.borrow_mut().take())
            .expect("profile frame");
    }
    let idx = STACK
        .with(|slot| slot.borrow_mut().pop())
        .expect("profile frame");
    active.nodes[idx].end = now;
    let is_root = active.nodes[idx].parent.is_none();
    if !is_root {
        ACTIVE.with(|slot| *slot.borrow_mut() = Some(active));
        return;
    }
    merge_context(active);
}

fn merge_call(entry: &mut ProfileEntry, active: &Active, idx: usize, prev_start: Option<u64>) {
    let children = active.nodes[idx].children.clone();
    for child in children {
        let child_name = active.nodes[child].name;
        let ci = entry.child_index(child_name);
        merge_call(&mut entry.children[ci], active, child, None);
    }
    if entry.expected != 0
        && let Some(prev) = prev_start
    {
        entry.between.migrate(true);
        entry
            .between
            .add(diff_ns_to_usec(prev, active.nodes[idx].start), 1);
    }
    entry.times.migrate(true);
    entry.times.add(
        diff_ns_to_usec(active.nodes[idx].start, active.nodes[idx].end),
        1,
    );
}

fn merge_context(active: Active) {
    let active = Arc::new(active);
    let name = active.nodes[0].name;
    let mut globals = lock_globals();
    if !globals.enabled {
        drop(globals);
        set_thread_enabled(false);
        return;
    }
    let root = get_root(&mut globals, name);
    let old = root.prev.lock().unwrap().replace(Arc::clone(&active));
    let root = Arc::clone(&root);
    drop(globals);
    let mut entry = root.entry.lock().unwrap();
    let prev_start = old.as_ref().map(|a| a.root_start());
    merge_call(&mut entry, &active, 0, prev_start);
}

pub fn profile_snapshot_create() -> Snapshot {
    let globals = lock_globals();
    let mut roots = Vec::with_capacity(globals.roots.len());
    for root in &globals.roots {
        roots.push(root.entry.lock().unwrap().snapshot_of());
    }
    Snapshot { roots }
}
