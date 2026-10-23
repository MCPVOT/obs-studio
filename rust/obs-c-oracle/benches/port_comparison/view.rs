//! #1933: obs-view, the obs-core shims against the C, on the oracle's view
//! stand-ins (no trace hook, so each step is a plain call).

use super::pair;
use criterion::Criterion;
use obs_c_oracle::view::{self as c, OracleViewSource};
use obs_core::ffi::view as rust;
use std::ffi::c_void;
use std::hint::black_box;
use std::ptr;

pub fn benchmarks(criterion: &mut Criterion) {
    let mut sources: Vec<OracleViewSource> = [c"a", c"b"]
        .iter()
        .map(|n| OracleViewSource {
            name: n.as_ptr(),
            removed: false,
            dying: false,
        })
        .collect();
    let a: *mut c_void = ptr::from_mut(&mut sources[0]).cast();
    let b: *mut c_void = ptr::from_mut(&mut sources[1]).cast();

    // SAFETY (whole block): views from each side's create, the oracle's
    // stand-in sources, no trace hook; destroyed at the end.
    unsafe {
        c::oracle_view_set_trace(None);
        c::oracle_view_reset(false, ptr::null_mut());
        let rv = rust::obs_view_create();
        let cv = c::oracle_obs_view_create();
        for ch in 0..16 {
            rust::obs_view_set_source(rv, ch, a);
            c::oracle_obs_view_set_source(cv, ch, a);
        }

        pair(
            criterion,
            "view/set_source swap",
            || {
                rust::obs_view_set_source(black_box(rv), 3, black_box(b));
                rust::obs_view_set_source(black_box(rv), 3, black_box(a));
            },
            || {
                c::oracle_obs_view_set_source(black_box(cv), 3, black_box(b));
                c::oracle_obs_view_set_source(black_box(cv), 3, black_box(a));
            },
        );
        pair(
            criterion,
            "view/get_source",
            || rust::obs_view_get_source(black_box(rv), black_box(5)).addr(),
            || c::oracle_obs_view_get_source(black_box(cv), black_box(5)).addr(),
        );
        pair(
            criterion,
            "view/render 16 of 64",
            || rust::obs_view_render(black_box(rv)),
            || c::oracle_obs_view_render(black_box(cv)),
        );
        pair(
            criterion,
            "view/create + destroy",
            || rust::obs_view_destroy(black_box(rust::obs_view_create())),
            || c::oracle_obs_view_destroy(black_box(c::oracle_obs_view_create())),
        );

        rust::obs_view_destroy(rv);
        c::oracle_obs_view_destroy(cv);
    }
}
