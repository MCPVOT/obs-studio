fn main() {
    let windows = std::env::var_os("CARGO_CFG_WINDOWS").is_some();
    let msvc = std::env::var("CARGO_CFG_TARGET_ENV").ok().as_deref() == Some("msvc");
    if windows && msvc {
        cc::Build::new()
            .file("src/ffi/thread_name_seh.c")
            .compile("thread_name_seh");
    }
}
