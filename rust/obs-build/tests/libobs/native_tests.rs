//! Runs unchanged native tests with isolated paths and verified core selection.
use obs_build::{NativeBuild, validation::Executable};
use std::path::Path;

pub fn exercise(
    build: &NativeBuild,
    root: &Path,
    core: &Path,
    rust_core: &Path,
    tests: &[Executable],
    suite: &str,
) {
    assert_ne!(
        core, rust_core,
        "compatibility runs require distinct C and Rust libraries"
    );
    let mut results = Vec::new();
    let mut failures = Vec::new();
    for selected in [core, rust_core] {
        let selected = dunce::canonicalize(selected).unwrap();
        let variant = selected
            .parent()
            .unwrap()
            .file_name()
            .unwrap()
            .to_str()
            .unwrap();
        let search = std::env::join_paths(
            std::iter::once(selected.parent().unwrap().to_owned()).chain(std::env::split_paths(
                &std::env::var_os("LD_LIBRARY_PATH").unwrap_or_default(),
            )),
        )
        .unwrap();
        for test in tests {
            let directory = build
                .output
                .join(format!("{suite}-c-test-runs"))
                .join(variant)
                .join(&test.name);
            std::fs::create_dir_all(directory.join("tmp")).unwrap();
            let directory = dunce::canonicalize(directory).unwrap();
            let executable = dunce::canonicalize(&test.path).unwrap();
            for name in ["config", "data", "cache"] {
                std::fs::create_dir_all(directory.join(name)).unwrap();
            }
            super::application::copy_tree(
                &root.join("libobs/data"),
                &directory.join("share/obs/libobs"),
            )
            .unwrap();
            let resolution = std::process::Command::new("ldd")
                .arg(&executable)
                .env("LD_LIBRARY_PATH", &search)
                .output()
                .unwrap();
            assert!(resolution.status.success());
            assert!(
                String::from_utf8_lossy(&resolution.stdout).contains(selected.to_str().unwrap()),
                "{} did not resolve the selected core",
                test.name
            );
            std::fs::write(directory.join("dependencies.log"), &resolution.stdout).unwrap();
            let deadline = match test.name.as_str() {
                "test_vlc_audio_track"
                | "test_vlc_audio_pacing"
                | "test_vlc_playlist_gap"
                | "test_group_hide_transition_audio" => "120s",
                _ => "60s",
            };
            let started = std::time::Instant::now();
            let status = std::process::Command::new("timeout")
                .args(["--kill-after=5s", deadline])
                .arg(&executable)
                .current_dir(&directory)
                .env("LD_LIBRARY_PATH", &search)
                .env("TMPDIR", directory.join("tmp"))
                .env("XDG_CONFIG_HOME", directory.join("config"))
                .env("XDG_DATA_HOME", directory.join("data"))
                .env("XDG_CACHE_HOME", directory.join("cache"))
                .env("CMOCKA_MESSAGE_OUTPUT", "STDOUT")
                .env_remove("CMOCKA_TEST_FILTER")
                .env_remove("CMOCKA_SKIP_FILTER")
                .stdout(std::fs::File::create(directory.join("stdout.log")).unwrap())
                .stderr(std::fs::File::create(directory.join("stderr.log")).unwrap())
                .status()
                .unwrap();
            let stdout = std::fs::read_to_string(directory.join("stdout.log")).unwrap();
            let stderr = std::fs::read_to_string(directory.join("stderr.log")).unwrap();
            let skipped = status.code() == Some(77)
                || stdout.contains("SKIPPED")
                || stderr.contains("SKIPPED");
            let passed = status.success() && !skipped;
            println!(
                "cc::Build {suite} C test: {variant}/{} exit {status}, skipped={skipped}",
                test.name
            );
            results.push(serde_json::json!({"variant":variant,"name":test.name,
                "status":status.code(),"skipped":skipped,"passed":passed,
                "seconds":started.elapsed().as_secs_f64(),"directory":directory}));
            std::fs::write(
                build.output.join(format!("{suite}-c-tests.json")),
                serde_json::to_vec_pretty(&results).unwrap(),
            )
            .unwrap();
            if !passed {
                failures.push(format!("{variant}/{} ({})", test.name, directory.display()));
            }
        }
    }
    assert!(
        failures.is_empty(),
        "{suite} compatibility failures or skips: {}",
        failures.join(", ")
    );
}
