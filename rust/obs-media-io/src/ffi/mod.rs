//! C ABI shims replacing the original libobs `EXPORT` symbols.

pub mod audio_io;
// The audio-io.c engine is Unix-only for now; see its module docs.
#[cfg(unix)]
pub mod audio_output;
pub mod format_conversion;
pub mod frame_rate;
pub mod video_fourcc;
pub mod video_frame;
pub mod video_io;
pub mod video_matrices;
