//! Port of the `static inline` helpers in `libobs/media-io/audio-math.h`.
//! Header-inline, so nothing is exported or swapped.

/// `mul_to_db`: a linear multiplier in decibels, `20 * log10(mul)`.
///
/// 0 (either sign) gives -inf. A negative multiplier or NaN gives NaN.
pub fn mul_to_db(mul: f32) -> f32 {
    let _ = mul;
    todo!("port mul_to_db")
}

/// `db_to_mul`: decibels as a linear multiplier, `10^(db / 20)`.
///
/// Non-finite input, +inf included, gives 0.
pub fn db_to_mul(db: f32) -> f32 {
    let _ = db;
    todo!("port db_to_mul")
}
