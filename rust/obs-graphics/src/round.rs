//! Inline `ceilf`/`floorf` for the vector cores (obs-rust/obs-studio#1298).
//!
//! On the x86_64 baseline (SSE2, no SSE4.1 `roundss`) LLVM lowers
//! `f32::ceil`/`f32::floor` to a libm call. GCC, building the C original,
//! inlines `ceilf`/`floorf` as a truncate-and-adjust sequence instead:
//! `|x| < 2^23` ? `copysign(trunc(x) ± (x ≷ trunc(x)), x)` : `x`. These
//! helpers do the same in portable Rust (rounding with the `±2^23` add
//! trick in place of the truncating cast), so they compile to inline SSE2
//! code without raising the CPU baseline.
//!
//! Results equal `f32::ceil`/`f32::floor` bit for bit for every non-NaN
//! input (the exhaustive test below checks all 2^32 bit patterns). A NaN is
//! returned unchanged, as the GCC-inlined C does (GCC assumes no signaling
//! NaNs by default, so it does not quiet them the way libm does).

/// 2^23: every `f32` with at least this magnitude is already an integer.
const TWO_POW_23: f32 = 8_388_608.0;

/// Rounds `x` (with `|x| < 2^23`) to an integer, ties to even.
///
/// Adding then subtracting `±2^23` pushes the fraction bits out of the
/// mantissa; both steps are exact apart from that one rounding. This is
/// cheaper than `x as i32 as f32`, whose saturating Rust cast adds compares
/// and conditional moves on x86_64 that GCC's C `(int)` cast does not have.
#[inline]
fn round_small(x: f32) -> f32 {
    let s = TWO_POW_23.copysign(x);
    (x + s) - s
}

/// `ceilf(x)`, inline.
#[inline]
#[must_use]
pub(crate) fn ceil_f32(x: f32) -> f32 {
    // False for NaN: NaN, infinities and large values return unchanged.
    if x.abs() < TWO_POW_23 {
        let t = round_small(x);
        let up = if x > t { 1.0 } else { 0.0 };
        // Keeps -0.0 for inputs in (-1, -0.0].
        (t + up).copysign(x)
    } else {
        core::hint::cold_path();
        x
    }
}

/// `floorf(x)`, inline.
#[inline]
#[must_use]
pub(crate) fn floor_f32(x: f32) -> f32 {
    if x.abs() < TWO_POW_23 {
        let t = round_small(x);
        let down = if x < t { 1.0 } else { 0.0 };
        (t - down).copysign(x)
    } else {
        core::hint::cold_path();
        x
    }
}

#[cfg(test)]
mod tests {
    use super::{ceil_f32, floor_f32};

    /// Bitwise equal, or both NaN with the input's bits (the C behaviour).
    fn matches(got: f32, want: f32, x: f32) -> bool {
        if x.is_nan() {
            got.to_bits() == x.to_bits()
        } else {
            got.to_bits() == want.to_bits()
        }
    }

    /// Checks every `f32` bit pattern, split across threads. The crate's dev
    /// profile is optimized (workspace `Cargo.toml`), so this takes seconds.
    #[test]
    fn exhaustive_matches_libm() {
        const CHUNKS: u64 = 64;
        let span = (1u64 << 32) / CHUNKS;
        std::thread::scope(|s| {
            let handles: Vec<_> = (0..CHUNKS)
                .map(|c| {
                    s.spawn(move || {
                        for bits in c * span..(c + 1) * span {
                            #[allow(clippy::cast_possible_truncation)]
                            let x = f32::from_bits(bits as u32);
                            assert!(
                                matches(ceil_f32(x), x.ceil(), x),
                                "ceil({bits:#010x}) = {:#010x}, want {:#010x}",
                                ceil_f32(x).to_bits(),
                                x.ceil().to_bits()
                            );
                            assert!(
                                matches(floor_f32(x), x.floor(), x),
                                "floor({bits:#010x}) = {:#010x}, want {:#010x}",
                                floor_f32(x).to_bits(),
                                x.floor().to_bits()
                            );
                        }
                    })
                })
                .collect();
            for h in handles {
                h.join().expect("exhaustive chunk panicked");
            }
        });
    }

    #[test]
    fn edge_values() {
        let cases: [(f32, f32, f32); 12] = [
            (0.0, 0.0, 0.0),
            (-0.0, -0.0, -0.0),
            (0.5, 1.0, 0.0),
            (-0.5, -0.0, -1.0),
            (1.0, 1.0, 1.0),
            (-1.5, -1.0, -2.0),
            (f32::from_bits(1), 1.0, 0.0),
            (-f32::from_bits(1), -0.0, -1.0),
            (8_388_607.5, 8_388_608.0, 8_388_607.0),
            (2_147_483_648.0, 2_147_483_648.0, 2_147_483_648.0),
            (f32::INFINITY, f32::INFINITY, f32::INFINITY),
            (f32::NEG_INFINITY, f32::NEG_INFINITY, f32::NEG_INFINITY),
        ];
        for (x, c, f) in cases {
            assert_eq!(ceil_f32(x).to_bits(), c.to_bits(), "ceil({x})");
            assert_eq!(floor_f32(x).to_bits(), f.to_bits(), "floor({x})");
        }
        let snan = f32::from_bits(0x7f80_0001);
        assert_eq!(ceil_f32(snan).to_bits(), snan.to_bits());
        assert_eq!(floor_f32(snan).to_bits(), snan.to_bits());
    }
}
