//! Tier 1 tests for obs-rust/obs-studio#1023: DRM fdinfo parsers. New code
//! with no C original, so Tiers 2 and 3 do not apply. Fixture texts follow
//! docs.kernel.org/gpu/drm-usage-stats.html and each driver's own keys.

use std::fs;
use std::path::PathBuf;

use obs_gpu_status::Utilization;
use obs_gpu_status::fdinfo::{fdinfo_utilization, parse_fdinfo, read_fdinfo};

const I915_PDEV: &str = "0000:00:02.0";
const AMD_PDEV: &str = "0000:2b:00.0";

fn i915(client: u64, render_ns: u64, video_ns: u64) -> String {
    format!(
        "pos:\t0\nflags:\t02100002\nmnt_id:\t26\nino:\t1063\n\
         drm-driver:\ti915\ndrm-pdev:\t{I915_PDEV}\ndrm-client-id:\t{client}\n\
         drm-engine-render:\t{render_ns} ns\ndrm-engine-copy:\t0 ns\n\
         drm-engine-video:\t{video_ns} ns\ndrm-engine-capacity-video:\t2\n\
         drm-engine-video-enhance:\t0 ns\n"
    )
}

fn xe(client: u64, rcs: u64, rcs_total: u64) -> String {
    format!(
        "drm-driver:\txe\ndrm-pdev:\t0000:03:00.0\ndrm-client-id:\t{client}\n\
         drm-total-system:\t0\ndrm-cycles-rcs:\t{rcs}\ndrm-total-cycles-rcs:\t{rcs_total}\n\
         drm-cycles-bcs:\t0\ndrm-total-cycles-bcs:\t{rcs_total}\n"
    )
}

fn amdgpu(client: u64, gfx_ns: u64) -> String {
    format!(
        "pos:\t0\nflags:\t02100002\ndrm-driver:\tamdgpu\ndrm-pdev:\t{AMD_PDEV}\n\
         drm-client-id:\t{client}\ndrm-memory-vram:\t4096 KiB\n\
         drm-engine-gfx:\t{gfx_ns} ns\ndrm-engine-compute:\t0 ns\n"
    )
}

#[test]
fn parses_i915_keys() {
    let c = parse_fdinfo(&i915(7, 100, 50)).unwrap();
    assert_eq!(c.driver, "i915");
    assert_eq!(c.pdev.as_deref(), Some(I915_PDEV));
    assert_eq!(c.client_id, 7);
    assert_eq!(c.engines["render"].busy, 100);
    assert_eq!(c.engines["render"].capacity, 1);
    assert_eq!(c.engines["video"].capacity, 2);
    assert_eq!(c.engines["video"].total, None);
    assert!(c.engines.contains_key("video-enhance"));
}

#[test]
fn parses_xe_cycles() {
    let c = parse_fdinfo(&xe(3, 500, 1000)).unwrap();
    assert_eq!(c.driver, "xe");
    assert_eq!(c.engines["rcs"].busy, 500);
    assert_eq!(c.engines["rcs"].total, Some(1000));
}

#[test]
fn parses_amdgpu_and_ignores_memory_keys() {
    let c = parse_fdinfo(&amdgpu(9, 42)).unwrap();
    assert_eq!(c.driver, "amdgpu");
    assert_eq!(c.engines["gfx"].busy, 42);
    assert_eq!(c.engines.len(), 2);
}

#[test]
fn non_drm_or_idless_fdinfo_is_none() {
    assert_eq!(parse_fdinfo("pos:\t0\nflags:\t02\nmnt_id:\t26\n"), None);
    assert_eq!(
        parse_fdinfo("drm-driver:\ti915\ndrm-engine-render:\t1 ns\n"),
        None
    );
}

#[test]
fn busiest_engine_over_elapsed_time() {
    let before = [parse_fdinfo(&i915(7, 0, 0)).unwrap()];
    // render busy 250 ms of 1 s; video 1 s on a two-engine class: 50 %.
    let after = [parse_fdinfo(&i915(7, 250_000_000, 1_000_000_000)).unwrap()];
    assert_eq!(
        fdinfo_utilization(&before, &after, I915_PDEV, 1_000_000_000),
        Utilization::Percent(50)
    );
}

#[test]
fn two_fds_of_one_client_count_once() {
    let before = [
        parse_fdinfo(&amdgpu(9, 0)).unwrap(),
        parse_fdinfo(&amdgpu(9, 0)).unwrap(),
    ];
    let after = [
        parse_fdinfo(&amdgpu(9, 300_000_000)).unwrap(),
        parse_fdinfo(&amdgpu(9, 300_000_000)).unwrap(),
    ];
    assert_eq!(
        fdinfo_utilization(&before, &after, AMD_PDEV, 1_000_000_000),
        Utilization::Percent(30)
    );
}

#[test]
fn clients_add_up_and_cap_at_100() {
    let before = [
        parse_fdinfo(&amdgpu(1, 0)).unwrap(),
        parse_fdinfo(&amdgpu(2, 0)).unwrap(),
    ];
    let after = [
        parse_fdinfo(&amdgpu(1, 200_000_000)).unwrap(),
        parse_fdinfo(&amdgpu(2, 100_000_000)).unwrap(),
    ];
    assert_eq!(
        fdinfo_utilization(&before, &after, AMD_PDEV, 1_000_000_000),
        Utilization::Percent(30)
    );
    let after = [parse_fdinfo(&amdgpu(1, 2_000_000_000)).unwrap()];
    assert_eq!(
        fdinfo_utilization(&before[..1], &after, AMD_PDEV, 1_000_000_000),
        Utilization::Percent(100)
    );
}

#[test]
fn xe_uses_cycles_against_total_cycles() {
    let before = [parse_fdinfo(&xe(3, 1000, 10_000)).unwrap()];
    let after = [parse_fdinfo(&xe(3, 1750, 11_000)).unwrap()];
    assert_eq!(
        fdinfo_utilization(&before, &after, "0000:03:00.0", 1_000_000_000),
        Utilization::Percent(75)
    );
}

#[test]
fn other_device_or_no_clients_is_unavailable() {
    let before = [parse_fdinfo(&amdgpu(9, 0)).unwrap()];
    let after = [parse_fdinfo(&amdgpu(9, 500_000_000)).unwrap()];
    assert_eq!(
        fdinfo_utilization(&before, &after, I915_PDEV, 1_000_000_000),
        Utilization::Unavailable
    );
    assert_eq!(
        fdinfo_utilization(&[], &[], AMD_PDEV, 1_000_000_000),
        Utilization::Unavailable
    );
}

#[test]
fn new_client_counts_from_zero_and_zero_elapsed_is_unavailable() {
    let after = [parse_fdinfo(&amdgpu(4, 100_000_000)).unwrap()];
    assert_eq!(
        fdinfo_utilization(&[], &after, AMD_PDEV, 1_000_000_000),
        Utilization::Percent(10)
    );
    assert_eq!(
        fdinfo_utilization(&after, &after, AMD_PDEV, 0),
        Utilization::Unavailable
    );
}

#[test]
fn reads_drm_clients_from_a_directory() {
    let dir = std::env::temp_dir().join(format!("obs-gpu-fdinfo-{}", std::process::id()));
    let _ = fs::remove_dir_all(&dir);
    fs::create_dir_all(&dir).unwrap();
    fs::write(dir.join("0"), "pos:\t0\nflags:\t02\n").unwrap();
    fs::write(dir.join("5"), i915(7, 1, 0)).unwrap();
    fs::write(dir.join("6"), i915(7, 1, 0)).unwrap();
    fs::write(dir.join("9"), amdgpu(2, 1)).unwrap();
    let mut ids: Vec<u64> = read_fdinfo(&dir).iter().map(|c| c.client_id).collect();
    ids.sort_unstable();
    let _ = fs::remove_dir_all(PathBuf::from(&dir));
    assert_eq!(ids, [2, 7, 7]);
}
