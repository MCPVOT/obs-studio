//! Original WHIP helper tests: real sources, no libobs or transport stubs.
use obs_build::{NativeBuild, validation};
use std::{path::PathBuf, process::Command};

#[test]
fn original_whip_clock_and_host_tests_pass_without_a_core_or_sdk() {
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../..");
    let directory = tempfile::tempdir().unwrap();
    let build = NativeBuild {
        host: env!("OBS_BUILD_HOST").into(),
        target: env!("OBS_BUILD_TARGET").into(),
        output: directory.path().join("native"),
    };
    let tests = validation::build_webrtc_tests(&build, &root).unwrap();
    assert_eq!(tests.len(), 2);
    for test in tests {
        let output = Command::new("timeout")
            .args(["--kill-after=5s", "60s"])
            .arg(&test.path)
            .current_dir(directory.path())
            .env("CMOCKA_MESSAGE_OUTPUT", "STDOUT")
            .env_remove("CMOCKA_TEST_FILTER")
            .env_remove("CMOCKA_SKIP_FILTER")
            .output()
            .unwrap();
        let stdout = String::from_utf8_lossy(&output.stdout);
        let stderr = String::from_utf8_lossy(&output.stderr);
        println!("{}\n{stdout}\n{stderr}", test.name);
        assert!(output.status.success(), "{}: {}", test.name, output.status);
        assert!(!stdout.contains("SKIPPED") && !stderr.contains("SKIPPED"));
        let cases = match test.name.as_str() {
            "test_whip_rtp_timestamp" => 6,
            "test_whip_host_os_name" => 2,
            other => panic!("unexpected WHIP target {other}"),
        };
        assert!(
            stderr.contains(&format!("{cases} test(s)."))
                || stdout.contains(&format!("{cases} test(s)."))
        );
    }
}
