//! Runs the unchanged VLC C tests with the selected native artifacts.
use obs_build::{NativeBuild, plugins::Module, validation};
use std::path::Path;

pub fn exercise(
    build: &NativeBuild,
    root: &Path,
    core: &Path,
    rust_core: &Path,
    vlc: &Module,
    graphics: &Path,
) {
    let tests = validation::build_vlc_tests(build, root, core, vlc, graphics).unwrap();
    assert_eq!(
        tests.len(),
        6,
        "all original Linux VLC test targets are required"
    );
    super::native_tests::exercise(build, root, core, rust_core, &tests, "vlc");
}
