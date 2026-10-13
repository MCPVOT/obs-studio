fn main() {
    let windows = std::env::var_os("CARGO_CFG_WINDOWS").is_some();
    let msvc = std::env::var("CARGO_CFG_TARGET_ENV").ok().as_deref() == Some("msvc");
    if windows && msvc {
        // Warnings are errors, like the oracle (obs-rust/obs-studio#516).
        cc::Build::new()
            .file("src/ffi/thread_name_seh.c")
            .warnings(true)
            .extra_warnings(true)
            .warnings_into_errors(true)
            .compile("thread_name_seh");
    }
}
