//! Preserve the C source's individual sinf/cosf operations.
//!
//! Apple's paired __sincosf_stret can differ by one ULP from separate calls.
//! LLVM combines Rust intrinsics at opt-level 3, whereas the unchanged C
//! oracle uses individual calls. Separate non-inlined scalar functions keep
//! that optimization from silently changing the source operation's precision.

#[inline(never)]
fn sin(x: f32) -> f32 {
    x.sin()
}

#[inline(never)]
fn cos(x: f32) -> f32 {
    x.cos()
}

pub(crate) fn sin_cos(x: f32) -> (f32, f32) {
    (sin(x), cos(x))
}
