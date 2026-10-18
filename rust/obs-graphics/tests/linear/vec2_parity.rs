//! Tier 3: the Rust C ABI shims and safe core behave exactly like the
//! original C, compiled as an oracle.
//!
//! No intentional differences from C.
//!
//! Reminder: the mutation check (break the core, see this file fail) is done
//! once per port, not on every change.

use obs_c_oracle::vec2::{self as c, OracleVec2};
use obs_graphics::ffi::vec2::{self as rs, vec2};
use obs_graphics::vec2::Vec2;
use proptest::prelude::*;

/// Bit-exact equality, except that any NaN equals any NaN.
fn same(a: f32, b: f32) -> bool {
    a.to_bits() == b.to_bits() || (a.is_nan() && b.is_nan())
}

fn arb_f32() -> impl Strategy<Value = f32> {
    any::<u32>().prop_map(f32::from_bits)
}

const SENTINEL: (f32, f32) = (7.0, 9.0);

proptest! {
    #[test]
    fn unary_ops_match_c_oracle(x in arb_f32(), y in arb_f32()) {
        let rv = vec2 { x, y };
        let cv = OracleVec2 { x, y };
        let core = Vec2::new(x, y);
        let mut rd = vec2::default();
        let mut cd = OracleVec2::default();

        // SAFETY: all pointers refer to live, properly aligned locals.
        unsafe {
            rs::vec2_abs(&mut rd, &rv);
            c::oracle_vec2_abs(&mut cd, &cv);
        }
        prop_assert!(same(rd.x, cd.x) && same(rd.y, cd.y));
        prop_assert!(same(core.abs().x, cd.x) && same(core.abs().y, cd.y));

        // SAFETY: as above.
        unsafe {
            rs::vec2_floor(&mut rd, &rv);
            c::oracle_vec2_floor(&mut cd, &cv);
        }
        prop_assert!(same(rd.x, cd.x) && same(rd.y, cd.y));
        prop_assert!(same(core.floor().x, cd.x) && same(core.floor().y, cd.y));

        // SAFETY: as above.
        unsafe {
            rs::vec2_ceil(&mut rd, &rv);
            c::oracle_vec2_ceil(&mut cd, &cv);
        }
        prop_assert!(same(rd.x, cd.x) && same(rd.y, cd.y));
        prop_assert!(same(core.ceil().x, cd.x) && same(core.ceil().y, cd.y));
    }

    #[test]
    fn norm_matches_c_oracle(x in arb_f32(), y in arb_f32()) {
        let rv = vec2 { x, y };
        let cv = OracleVec2 { x, y };
        let mut rd = vec2 { x: SENTINEL.0, y: SENTINEL.1 };
        let mut cd = OracleVec2 { x: SENTINEL.0, y: SENTINEL.1 };

        // SAFETY: all pointers refer to live, properly aligned locals.
        unsafe {
            rs::vec2_norm(&mut rd, &rv);
            c::oracle_vec2_norm(&mut cd, &cv);
        }
        prop_assert!(same(rd.x, cd.x) && same(rd.y, cd.y));

        // The safe core agrees; None means dst keeps the sentinel.
        let (ex, ey) = Vec2::new(x, y)
            .norm()
            .map_or(SENTINEL, |n| (n.x, n.y));
        prop_assert!(same(ex, cd.x) && same(ey, cd.y));

        // dst aliasing the source, on both sides.
        let mut ra = rv;
        let mut ca = cv;
        let ra_ptr: *mut vec2 = &mut ra;
        let ca_ptr: *mut OracleVec2 = &mut ca;
        // SAFETY: the pointers refer to live, aligned locals; aliasing
        // dst and src is allowed by both implementations.
        unsafe {
            rs::vec2_norm(ra_ptr, ra_ptr);
            c::oracle_vec2_norm(ca_ptr, ca_ptr);
        }
        prop_assert!(same(ra.x, ca.x) && same(ra.y, ca.y));
    }

    #[test]
    fn close_matches_c_oracle(
        x1 in arb_f32(), y1 in arb_f32(),
        x2 in arb_f32(), y2 in arb_f32(),
        epsilon in arb_f32(),
    ) {
        let r1 = vec2 { x: x1, y: y1 };
        let r2 = vec2 { x: x2, y: y2 };
        let c1 = OracleVec2 { x: x1, y: y1 };
        let c2 = OracleVec2 { x: x2, y: y2 };

        // SAFETY: all pointers refer to live, properly aligned locals.
        let (ours, theirs) = unsafe {
            (
                rs::vec2_close(&r1, &r2, epsilon),
                c::oracle_vec2_close(&c1, &c2, epsilon),
            )
        };
        prop_assert_eq!(ours != 0, theirs != 0);
        prop_assert_eq!(
            Vec2::new(x1, y1).close(Vec2::new(x2, y2), epsilon),
            theirs != 0
        );
    }
}

/// Random bits almost never land on the `<=` boundary, so check the case
/// where the difference equals epsilon exactly.
#[test]
fn close_boundary_matches_c_oracle() {
    for (x1, x2) in [(1.0_f32, 1.5_f32), (0.0, 0.25), (-3.0, -2.0), (2.0, 2.0)] {
        let epsilon = (x1 - x2).abs();
        let r1 = vec2 { x: x1, y: 0.0 };
        let r2 = vec2 { x: x2, y: 0.0 };
        let c1 = OracleVec2 { x: x1, y: 0.0 };
        let c2 = OracleVec2 { x: x2, y: 0.0 };

        // SAFETY: all pointers refer to live, properly aligned locals.
        let (ours, theirs) = unsafe {
            (
                rs::vec2_close(&r1, &r2, epsilon),
                c::oracle_vec2_close(&c1, &c2, epsilon),
            )
        };
        assert_ne!(theirs, 0, "C treats a difference equal to epsilon as close");
        assert_eq!(ours != 0, theirs != 0);
        assert_eq!(
            Vec2::new(x1, 0.0).close(Vec2::new(x2, 0.0), epsilon),
            theirs != 0
        );
    }
}

/// Edge values for the inline ceil/floor (#1298): signed zeros, subnormals,
/// halves, the 2^23 boundary, i32 limits, infinities and NaNs (quiet and
/// signaling, both signs). NaN bits must match the C exactly.
#[test]
fn ceil_floor_edge_values_match_c_oracle() {
    let bits: [u32; 22] = [
        0x0000_0000,
        0x8000_0000,
        0x0000_0001,
        0x8000_0001,
        0x007f_ffff,
        0x3f00_0000,
        0xbf00_0000,
        0x3f7f_ffff,
        0xbf7f_ffff,
        0x3fc0_0000,
        0xbfc0_0000,
        0x4aff_ffff,
        0xcaff_ffff,
        0x4b00_0000,
        0xcb00_0000,
        0x4f00_0000,
        0xcf00_0000,
        0x7f80_0000,
        0xff80_0000,
        0x7fc0_0000,
        0x7f80_0001,
        0xffa0_0123,
    ];
    for &b in &bits {
        let x = f32::from_bits(b);
        let v = vec2 { x, y: -x };
        let cv = OracleVec2 { x, y: -x };
        let (mut rd, mut cd) = (v, cv);
        // SAFETY: all pointers refer to live, properly aligned locals.
        unsafe {
            rs::vec2_ceil(&mut rd, &v);
            c::oracle_vec2_ceil(&mut cd, &cv);
        }
        assert_eq!(
            (rd.x.to_bits(), rd.y.to_bits()),
            (cd.x.to_bits(), cd.y.to_bits()),
            "ceil {b:#010x}"
        );
        // SAFETY: as above.
        unsafe {
            rs::vec2_floor(&mut rd, &v);
            c::oracle_vec2_floor(&mut cd, &cv);
        }
        assert_eq!(
            (rd.x.to_bits(), rd.y.to_bits()),
            (cd.x.to_bits(), cd.y.to_bits()),
            "floor {b:#010x}"
        );
    }
}
