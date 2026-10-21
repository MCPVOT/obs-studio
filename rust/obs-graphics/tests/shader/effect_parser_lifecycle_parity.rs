//! Layout and Tier 3 against the extracted original C. No intentional
//! differences; fixtures contain initialized, distinct owned allocations.
//! Header-inline destructors remain C on both paths, as in production.
use obs_c_oracle::effect_parser_lifecycle as c;
use obs_graphics::ffi::effect_parser_lifecycle::{effect_parser, ep_free};
use proptest::prelude::*;
use std::mem::{align_of, offset_of, size_of};

#[test]
fn parent_layout_matches_c() {
    let rust = [
        size_of::<effect_parser>(),
        align_of::<effect_parser>(),
        offset_of!(effect_parser, effect),
        offset_of!(effect_parser, params),
        offset_of!(effect_parser, structs),
        offset_of!(effect_parser, funcs),
        offset_of!(effect_parser, samplers),
        offset_of!(effect_parser, techniques),
        offset_of!(effect_parser, files),
        offset_of!(effect_parser, tokens),
        offset_of!(effect_parser, cur_pass),
        offset_of!(effect_parser, cfp),
    ];
    for (i, value) in rust.into_iter().enumerate() {
        // SAFETY: i indexes the C probe's fixed array.
        assert_eq!(
            value,
            unsafe { c::oracle_effect_parser_layout(i) },
            "layout {i}"
        );
    }
}

#[test]
fn owned_graphs_match_original_c() {
    const CHILD: &str = "OBS_EFFECT_LIFECYCLE_PARITY_CHILD";
    if std::env::var_os(CHILD).is_none() {
        // Allocation accounting is process-global: re-exec this exact case,
        // not a new test binary and not a lock that other suites don't share.
        let name = format!("{}::owned_graphs_match_original_c", module_path!());
        let name = name.split_once("::").unwrap().1;
        let output = std::process::Command::new(std::env::current_exe().unwrap())
            .args(["--exact", name, "--nocapture"])
            .env(CHILD, "1")
            .output()
            .unwrap();
        let stdout = String::from_utf8_lossy(&output.stdout);
        assert!(stdout.contains("running 1 test"), "{stdout}");
        assert!(
            output.status.success(),
            "{stdout}\n{}",
            String::from_utf8_lossy(&output.stderr)
        );
        return;
    }
    fn compare(seed: u32) {
        // SAFETY: the C fixture builds a valid exclusively owned graph whose
        // first member is effect_parser. Each path destroys it twice (valid
        // idempotent cleanup), then the C checker frees retained allocations.
        unsafe {
            let original = c::oracle_effect_parser_fixture(seed);
            c::oracle_ep_free(original);
            c::oracle_ep_free(original);
            let expected = c::oracle_effect_parser_check(original);
            assert_eq!(expected, 0, "original C fixture, seed {seed}");
            let rust = c::oracle_effect_parser_fixture(seed);
            ep_free(rust.cast());
            ep_free(rust.cast());
            assert_eq!(c::oracle_effect_parser_check(rust), expected, "seed {seed}");
        }
    }
    for seed in [0, 1, 4, u32::MAX, 0x0fff_ffff, 0xaaaa_aaaa, 0x5555_5555] {
        compare(seed);
    }
    proptest::test_runner::TestRunner::default()
        .run(&any::<u32>(), |seed| {
            compare(seed);
            Ok(())
        })
        .unwrap();
}
