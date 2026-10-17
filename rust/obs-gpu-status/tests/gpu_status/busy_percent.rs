//! Tier 1 tests for obs-rust/obs-studio#1024: amdgpu's `gpu_busy_percent`.
//! New code with no C original, so Tiers 2 and 3 do not apply.

#![cfg(unix)]

use std::fs;
use std::path::{Path, PathBuf};

use obs_gpu_status::{Utilization, amdgpu_busy_percent};

struct Tree(PathBuf);

impl Drop for Tree {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}

fn tree(name: &str) -> Tree {
    let p = std::env::temp_dir().join(format!("obs-gpu-busy-{name}-{}", std::process::id()));
    let _ = fs::remove_dir_all(&p);
    fs::create_dir_all(&p).unwrap();
    Tree(p)
}

/// A card at `pci` laid out as sysfs does; `busy` is its `gpu_busy_percent`
/// contents, or `None` for a driver without the file.
fn card(root: &Path, pci: &str, card: &str, busy: Option<&str>) {
    let dev = root.join("sys/devices/pci0000:00").join(pci);
    let dir = dev.join("drm").join(card);
    fs::create_dir_all(&dir).unwrap();
    if let Some(busy) = busy {
        fs::write(dev.join("gpu_busy_percent"), busy).unwrap();
    }
    std::os::unix::fs::symlink(format!("../../../{pci}"), dir.join("device")).unwrap();
    let class = root.join("sys/class/drm");
    fs::create_dir_all(&class).unwrap();
    std::os::unix::fs::symlink(
        format!("../../devices/pci0000:00/{pci}/drm/{card}"),
        class.join(card),
    )
    .unwrap();
}

#[test]
fn reads_the_render_adapters_card() {
    let t = tree("pick");
    card(&t.0, "0000:2b:00.0", "card0", Some("87\n"));
    card(&t.0, "0000:2c:00.0", "card1", Some("12\n"));
    assert_eq!(
        amdgpu_busy_percent(&t.0, "0000:2c:00.0"),
        Utilization::Percent(12)
    );
    assert_eq!(
        amdgpu_busy_percent(&t.0, "0000:2b:00.0"),
        Utilization::Percent(87)
    );
}

#[test]
fn missing_file_is_unavailable() {
    let t = tree("missing");
    card(&t.0, "0000:2b:00.0", "card0", None);
    assert_eq!(
        amdgpu_busy_percent(&t.0, "0000:2b:00.0"),
        Utilization::Unavailable
    );
}

#[test]
fn unknown_adapter_is_unavailable() {
    let t = tree("unknown");
    card(&t.0, "0000:2b:00.0", "card0", Some("50\n"));
    assert_eq!(
        amdgpu_busy_percent(&t.0, "0000:99:00.0"),
        Utilization::Unavailable
    );
}

#[test]
fn garbage_or_out_of_range_is_unavailable() {
    let t = tree("garbage");
    card(&t.0, "0000:2b:00.0", "card0", Some("busy\n"));
    card(&t.0, "0000:2c:00.0", "card1", Some("101\n"));
    assert_eq!(
        amdgpu_busy_percent(&t.0, "0000:2b:00.0"),
        Utilization::Unavailable
    );
    assert_eq!(
        amdgpu_busy_percent(&t.0, "0000:2c:00.0"),
        Utilization::Unavailable
    );
}
