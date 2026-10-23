//! One amalgamated category harness; ordinary cases run in parallel.

#[path = "audio/main.rs"]
mod audio;
#[path = "remux/main.rs"]
mod remux;
#[path = "video/main.rs"]
mod video;
