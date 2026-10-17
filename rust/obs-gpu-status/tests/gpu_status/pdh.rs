//! Tier 1 tests for obs-rust/obs-studio#1022: Windows GPU utilization from
//! the PDH `GPU Engine` counters. New code with no C original, so Tiers 2 and
//! 3 do not apply. Instance names follow those captured from Windows 11
//! (`typeperf "\GPU Engine(*)\Utilization Percentage"`).

use obs_gpu_status::Utilization;
use obs_gpu_status::pdh::{PdhInstance, parse_pdh_instance, pdh_utilization};

const DGPU: u64 = 0x0000_0000_0000_C6E1;
const IGPU: u64 = 0x0000_0001_0000_D2A4;

fn s(samples: &[(&str, f64)]) -> Vec<(String, f64)> {
    samples.iter().map(|(n, v)| ((*n).to_owned(), *v)).collect()
}

fn capture() -> Vec<(String, f64)> {
    s(&[
        (
            "pid_1234_luid_0x00000000_0x0000C6E1_phys_0_eng_0_engtype_3D",
            20.0,
        ),
        (
            "pid_5678_luid_0x00000000_0x0000C6E1_phys_0_eng_0_engtype_3D",
            15.4,
        ),
        (
            "pid_1234_luid_0x00000000_0x0000C6E1_phys_0_eng_3_engtype_VideoEncode",
            30.0,
        ),
        (
            "pid_1234_luid_0x00000000_0x0000C6E1_phys_0_eng_1_engtype_Copy",
            2.0,
        ),
        (
            "pid_5678_luid_0x00000000_0x0000C6E1_phys_0_eng_5_engtype_Compute_0",
            4.0,
        ),
        (
            "pid_1234_luid_0x00000001_0x0000D2A4_phys_0_eng_0_engtype_3D",
            80.0,
        ),
        (
            "pid_999_luid_0x00000001_0x0000d2a4_phys_0_eng_2_engtype_VideoDecode",
            5.0,
        ),
        ("_Total", 99.0),
        ("garbage", 50.0),
    ])
}

#[test]
fn parses_3d_instance() {
    assert_eq!(
        parse_pdh_instance("pid_1234_luid_0x00000000_0x0000C6E1_phys_0_eng_0_engtype_3D"),
        Some(PdhInstance {
            pid: 1234,
            luid: DGPU,
            phys: 0,
            eng: 0,
            engtype: "3D".into()
        })
    );
}

#[test]
fn parses_engtype_with_underscores_and_lowercase_hex() {
    assert_eq!(
        parse_pdh_instance("pid_7_luid_0x00000001_0x0000d2a4_phys_1_eng_12_engtype_Compute_0"),
        Some(PdhInstance {
            pid: 7,
            luid: IGPU,
            phys: 1,
            eng: 12,
            engtype: "Compute_0".into()
        })
    );
}

#[test]
fn rejects_garbage() {
    for name in [
        "",
        "_Total",
        "pid_x_luid_0x0_0x1_phys_0_eng_0_engtype_3D",
        "pid_1_luid_0x00000000_phys_0_eng_0_engtype_3D",
        "pid_1_luid_0xZZ_0x1_phys_0_eng_0_engtype_3D",
        "pid_1_luid_0x0_0x1_phys_0_eng_0",
        "pid_1_luid_0x0_0x1_phys_0_eng_0_engtype_",
    ] {
        assert_eq!(parse_pdh_instance(name), None, "{name:?}");
    }
}

#[test]
fn whole_adapter_is_busiest_engine_summed_over_processes() {
    // 3D: 20 + 15.4 = 35.4 beats VideoEncode 30.
    assert_eq!(
        pdh_utilization(&capture(), DGPU, None),
        Utilization::Percent(35)
    );
}

#[test]
fn second_adapter_is_separate() {
    assert_eq!(
        pdh_utilization(&capture(), IGPU, None),
        Utilization::Percent(80)
    );
}

#[test]
fn per_pid_share() {
    assert_eq!(
        pdh_utilization(&capture(), DGPU, Some(1234)),
        Utilization::Percent(30)
    );
    assert_eq!(
        pdh_utilization(&capture(), DGPU, Some(5678)),
        Utilization::Percent(15)
    );
}

#[test]
fn rounds_to_nearest() {
    let v = s(&[(
        "pid_1_luid_0x00000000_0x0000C6E1_phys_0_eng_0_engtype_3D",
        41.5,
    )]);
    assert_eq!(pdh_utilization(&v, DGPU, None), Utilization::Percent(42));
}

#[test]
fn clamps_above_100() {
    let v = s(&[
        (
            "pid_1_luid_0x00000000_0x0000C6E1_phys_0_eng_0_engtype_3D",
            70.0,
        ),
        (
            "pid_2_luid_0x00000000_0x0000C6E1_phys_0_eng_0_engtype_3D",
            60.0,
        ),
    ]);
    assert_eq!(pdh_utilization(&v, DGPU, None), Utilization::Percent(100));
}

#[test]
fn idle_adapter_is_zero() {
    let v = s(&[(
        "pid_1_luid_0x00000000_0x0000C6E1_phys_0_eng_0_engtype_3D",
        0.0,
    )]);
    assert_eq!(pdh_utilization(&v, DGPU, None), Utilization::Percent(0));
}

#[test]
fn unavailable_without_match() {
    assert_eq!(
        pdh_utilization(&capture(), 0xDEAD, None),
        Utilization::Unavailable
    );
    assert_eq!(
        pdh_utilization(&capture(), DGPU, Some(42)),
        Utilization::Unavailable
    );
    assert_eq!(
        pdh_utilization(&s(&[("garbage", 5.0)]), DGPU, None),
        Utilization::Unavailable
    );
    assert_eq!(pdh_utilization(&[], DGPU, None), Utilization::Unavailable);
}
