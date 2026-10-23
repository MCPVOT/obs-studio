//! Callback-time state must match the unchanged C properties implementation.

use std::ffi::c_void;

use obs_c_oracle::obs_properties as c;
use obs_properties::ffi::obs_properties as rs;

type GetParam = unsafe extern "C" fn(*mut c_void) -> *mut c_void;
type SetParam =
    unsafe extern "C" fn(*mut c_void, *mut c_void, Option<unsafe extern "C" fn(*mut c_void)>);
type SetFlags = unsafe extern "C" fn(*mut c_void, u32);

struct Param {
    props: *mut c_void,
    get: GetParam,
    flags: SetFlags,
    saw_self: bool,
    calls: u32,
}

unsafe extern "C" fn destroy(param: *mut c_void) {
    // SAFETY: the test keeps this Param and its properties alive through the callback.
    unsafe {
        let state = &mut *param.cast::<Param>();
        state.saw_self = (state.get)(state.props) == param;
        state.calls += 1;
        (state.flags)(state.props, 42);
    }
}

#[test]
fn replacement_destructor_observes_old_param_like_c() {
    for clear in [false, true] {
        for oracle in [true, false] {
            let (create, set, get, flags, get_flags, free) = if oracle {
                (
                    c::oracle_obs_properties_create as unsafe extern "C" fn() -> *mut c_void,
                    c::oracle_obs_properties_set_param as SetParam,
                    c::oracle_obs_properties_get_param as GetParam,
                    c::oracle_obs_properties_set_flags as SetFlags,
                    c::oracle_obs_properties_get_flags as unsafe extern "C" fn(_) -> u32,
                    c::oracle_obs_properties_destroy as unsafe extern "C" fn(_),
                )
            } else {
                (
                    rs::obs_properties_create as unsafe extern "C" fn() -> *mut c_void,
                    rs::obs_properties_set_param as SetParam,
                    rs::obs_properties_get_param as GetParam,
                    rs::obs_properties_set_flags as SetFlags,
                    rs::obs_properties_get_flags as unsafe extern "C" fn(_) -> u32,
                    rs::obs_properties_destroy as unsafe extern "C" fn(_),
                )
            };
            // SAFETY: all handles and callback parameters remain live until free returns.
            unsafe {
                let props = create();
                let mut old = Param {
                    props,
                    get,
                    flags,
                    saw_self: false,
                    calls: 0,
                };
                let mut new = Param {
                    props,
                    get,
                    flags,
                    saw_self: false,
                    calls: 0,
                };
                set(props, (&raw mut old).cast(), Some(destroy));
                let replacement = if clear {
                    std::ptr::null_mut()
                } else {
                    (&raw mut new).cast()
                };
                set(props, replacement, Some(destroy));
                let observed = (
                    old.saw_self,
                    old.calls,
                    get(props) == replacement,
                    get_flags(props),
                );
                free(props);
                assert_eq!(
                    observed,
                    (true, 1, true, 42),
                    "oracle={oracle}, clear={clear}"
                );
                assert_eq!(new.calls, u32::from(!clear));
                assert_eq!(new.saw_self, !clear);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// #1943: destroy callbacks that change the tree run like the C.

type CreateParam =
    unsafe extern "C" fn(*mut c_void, Option<unsafe extern "C" fn(*mut c_void)>) -> *mut c_void;
type Free = unsafe extern "C" fn(*mut c_void);
type Remove = unsafe extern "C" fn(*mut c_void, *const std::ffi::c_char);
type Get = unsafe extern "C" fn(*mut c_void, *const std::ffi::c_char) -> *mut c_void;
type AddBool = unsafe extern "C" fn(
    *mut c_void,
    *const std::ffi::c_char,
    *const std::ffi::c_char,
) -> *mut c_void;
type AddGroup = unsafe extern "C" fn(
    *mut c_void,
    *const std::ffi::c_char,
    *const std::ffi::c_char,
    i32,
    *mut c_void,
) -> *mut c_void;

/// One implementation's entry points.
#[derive(Clone, Copy)]
struct Api {
    create_param: CreateParam,
    free: Free,
    remove: Remove,
    get: Get,
    add_bool: AddBool,
    add_group: AddGroup,
}

const ORACLE: Api = Api {
    create_param: c::oracle_obs_properties_create_param,
    free: c::oracle_obs_properties_destroy,
    remove: c::oracle_obs_properties_remove_by_name,
    get: c::oracle_obs_properties_get,
    add_bool: c::oracle_obs_properties_add_bool,
    add_group: c::oracle_obs_properties_add_group,
};

const RUST: Api = Api {
    create_param: rs::obs_properties_create_param,
    free: rs::obs_properties_destroy,
    remove: rs::obs_properties_remove_by_name,
    get: rs::obs_properties_get,
    add_bool: rs::obs_properties_add_bool,
    add_group: rs::obs_properties_add_group,
};

/// `OBS_GROUP_NORMAL`.
const GROUP_NORMAL: i32 = 1;

/// What a destroy callback does, and what it saw.
struct Node {
    api: Api,
    tag: &'static str,
    log: *mut Vec<String>,
    /// The set this param belongs to (for reading its own content).
    own: *mut c_void,
    action: Action,
}

#[derive(Clone, Copy)]
enum Action {
    None,
    /// Remove the group named "g" from `own`.
    RemoveG,
    /// Add a group "h" whose content's param is `Node` `extra`.
    AddH(*mut Node),
    /// Look up "inner" in its own set and log whether it is there.
    ReadInner,
}

unsafe extern "C" fn node_destroy(param: *mut c_void) {
    // SAFETY: every Node outlives the sets in its test.
    unsafe {
        let n = &mut *param.cast::<Node>();
        let log = &mut *n.log;
        log.push(n.tag.to_string());
        match n.action {
            Action::None => {}
            Action::RemoveG => (n.api.remove)(n.own, c"g".as_ptr()),
            Action::AddH(extra) => {
                let content = (n.api.create_param)(extra.cast(), Some(node_destroy));
                (*extra).own = content;
                (n.api.add_group)(n.own, c"h".as_ptr(), c"H".as_ptr(), GROUP_NORMAL, content);
            }
            Action::ReadInner => {
                let found = !(n.api.get)(n.own, c"inner".as_ptr()).is_null();
                log.push(format!("{} sees inner: {found}", n.tag));
            }
        }
    }
}

fn node(api: Api, tag: &'static str, log: &mut Vec<String>, action: Action) -> Box<Node> {
    Box::new(Node {
        api,
        tag,
        log: &raw mut *log,
        own: std::ptr::null_mut(),
        action,
    })
}

/// A set with param `parent` holding group "g" whose content has param
/// `child` and a bool "inner".
unsafe fn tree(api: Api, parent: &mut Node, child: &mut Node) -> *mut c_void {
    // SAFETY: the caller keeps both nodes alive until the sets are freed.
    unsafe {
        let top = (api.create_param)((&raw mut *parent).cast(), Some(node_destroy));
        parent.own = top;
        let content = (api.create_param)((&raw mut *child).cast(), Some(node_destroy));
        child.own = content;
        (api.add_bool)(content, c"inner".as_ptr(), c"Inner".as_ptr());
        (api.add_group)(top, c"g".as_ptr(), c"G".as_ptr(), GROUP_NORMAL, content);
        top
    }
}

fn parent_removes_group(api: Api) -> Vec<String> {
    let mut log = Vec::new();
    let mut parent = node(api, "parent", &mut log, Action::RemoveG);
    let mut child = node(api, "child", &mut log, Action::None);
    // SAFETY: the nodes outlive the set.
    unsafe {
        let top = tree(api, &mut parent, &mut child);
        (api.free)(top);
    }
    log
}

fn parent_adds_group(api: Api) -> Vec<String> {
    let mut log = Vec::new();
    let mut added = node(api, "added", &mut log, Action::None);
    let mut parent = node(api, "parent", &mut log, Action::AddH(&raw mut *added));
    let mut child = node(api, "child", &mut log, Action::None);
    // SAFETY: the nodes outlive the set.
    unsafe {
        let top = tree(api, &mut parent, &mut child);
        (api.free)(top);
    }
    log
}

fn removed_group_reads_content(api: Api) -> Vec<String> {
    let mut log = Vec::new();
    let mut parent = node(api, "parent", &mut log, Action::None);
    let mut child = node(api, "child", &mut log, Action::ReadInner);
    // SAFETY: the nodes outlive the set.
    unsafe {
        let top = tree(api, &mut parent, &mut child);
        (api.remove)(top, c"g".as_ptr());
        (api.free)(top);
    }
    log
}

#[test]
fn parent_destructor_removing_a_group_destroys_it_once_like_c() {
    let c = parent_removes_group(ORACLE);
    assert_eq!(c, ["parent", "child"]);
    assert_eq!(parent_removes_group(RUST), c);
}

#[test]
fn parent_destructor_adding_a_group_destroys_it_like_c() {
    let c = parent_adds_group(ORACLE);
    assert_eq!(c, ["parent", "child", "added"]);
    assert_eq!(parent_adds_group(RUST), c);
}

#[test]
fn removed_group_content_is_alive_in_its_destructor_like_c() {
    let c = removed_group_reads_content(ORACLE);
    assert_eq!(c, ["child", "child sees inner: true", "parent"]);
    assert_eq!(removed_group_reads_content(RUST), c);
}
