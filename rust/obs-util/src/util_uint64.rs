//! Port of the `static inline` `util_mul_div64` in
//! `libobs/util/util_uint64.h`. Header-inline, so nothing is exported or
//! swapped.

/// `util_mul_div64`: `num * mul / div`, truncated.
///
/// Matches the C on each platform. On MSVC x64 the C uses
/// `_umul128`/`_udiv128`, exact 128-bit math. Everywhere else it computes
/// `(num / div) * mul + (num % div * mul) / div` in wrapping `u64`, which
/// differs from the exact result once `num % div * mul` overflows.
///
/// # Panics
///
/// If `div` is 0 (UB in C), and on MSVC x64 if the quotient does not fit
/// in 64 bits (`_udiv128` faults).
pub fn mul_div64(num: u64, mul: u64, div: u64) -> u64 {
    let _ = (num, mul, div);
    todo!()
}
