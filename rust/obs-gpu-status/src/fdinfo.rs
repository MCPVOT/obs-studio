//! Per-process GPU utilization from DRM fdinfo (obs-rust/obs-studio#1023):
//! the `drm-*` keys the kernel adds to `/proc/<pid>/fdinfo/<fd>` of a DRM
//! file (docs.kernel.org/gpu/drm-usage-stats.html). Covers i915, xe and
//! amdgpu where the kernel exposes engine times.

use std::collections::BTreeMap;
use std::path::Path;

use crate::Utilization;

/// One engine's counters in one fdinfo.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Engine {
    /// `drm-engine-<e>` in ns, or xe's `drm-cycles-<e>`.
    pub busy: u64,
    /// xe's `drm-total-cycles-<e>`: the clock `busy` counts against. `None`
    /// for ns engines, which count against wall time.
    pub total: Option<u64>,
    /// `drm-engine-capacity-<e>`: how many engines of the class; 1 if absent.
    pub capacity: u64,
}

/// The DRM keys of one fdinfo file.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FdinfoClient {
    pub driver: String,
    /// `drm-pdev`: the PCI address of the device, if the driver gives one.
    pub pdev: Option<String>,
    /// `drm-client-id`: fds that share one (dup'ed or passed) are one client.
    pub client_id: u64,
    pub engines: BTreeMap<String, Engine>,
}

fn engine<'a>(engines: &'a mut BTreeMap<String, Engine>, name: &str) -> &'a mut Engine {
    engines.entry(name.to_owned()).or_insert(Engine {
        capacity: 1,
        ..Engine::default()
    })
}

/// Parse one fdinfo text; `None` if it is not a DRM client with an id.
pub fn parse_fdinfo(text: &str) -> Option<FdinfoClient> {
    let mut driver = None;
    let mut pdev = None;
    let mut client_id = None;
    let mut engines: BTreeMap<String, Engine> = BTreeMap::new();

    let mut times = BTreeMap::new();
    let mut cycles = BTreeMap::new();
    for line in text.lines() {
        let Some((key, value)) = line.split_once(':') else {
            continue;
        };
        let value = value.trim();
        let number = || {
            value
                .strip_suffix(" ns")
                .unwrap_or(value)
                .trim()
                .parse::<u64>()
                .ok()
        };
        if key == "drm-driver" {
            driver = Some(value.to_owned());
        } else if key == "drm-pdev" {
            pdev = Some(value.to_owned());
        } else if key == "drm-client-id" {
            client_id = number();
        } else if let Some(name) = key.strip_prefix("drm-engine-capacity-") {
            if let Some(n) = number().filter(|&n| n > 0) {
                engine(&mut engines, name).capacity = n;
            }
        } else if let Some(name) = key.strip_prefix("drm-engine-") {
            // Only "<n> ns" values are engine times.
            if let Some(n) = value
                .strip_suffix(" ns")
                .and_then(|v| v.trim().parse().ok())
            {
                times.insert(name.to_owned(), n);
                engine(&mut engines, name);
            }
        } else if let Some(name) = key.strip_prefix("drm-total-cycles-") {
            if let Some(n) = number() {
                engine(&mut engines, name).total = Some(n);
            }
        } else if let Some(name) = key.strip_prefix("drm-cycles-")
            && let Some(n) = number()
        {
            cycles.insert(name.to_owned(), n);
            engine(&mut engines, name);
        }
    }

    // Keep units paired: prefer cycles only with their total-cycle clock.
    // Otherwise fall back to an engine's time counter, never cycles / ns.
    engines.retain(|name, e| {
        if let Some(&busy) = cycles.get(name)
            && e.total.is_some()
        {
            e.busy = busy;
            true
        } else if let Some(&busy) = times.get(name) {
            e.busy = busy;
            e.total = None;
            true
        } else {
            false
        }
    });

    Some(FdinfoClient {
        driver: driver?,
        pdev,
        client_id: client_id?,
        engines,
    })
}

/// Every DRM client among the fdinfo files in `dir` (`/proc/self/fdinfo`).
pub fn read_fdinfo(dir: &Path) -> Vec<FdinfoClient> {
    let Ok(entries) = std::fs::read_dir(dir) else {
        return Vec::new();
    };
    entries
        .flatten()
        .filter_map(|e| std::fs::read_to_string(e.path()).ok())
        .filter_map(|text| parse_fdinfo(&text))
        .collect()
}

/// Each client on `pdev` once, by client id: fds of one client carry the
/// same counters.
fn clients_on(clients: &[FdinfoClient], pdev: &str) -> BTreeMap<u64, FdinfoClient> {
    let mut by_id: BTreeMap<u64, FdinfoClient> = BTreeMap::new();
    for c in clients.iter().filter(|c| c.pdev.as_deref() == Some(pdev)) {
        let client = by_id.entry(c.client_id).or_insert_with(|| c.clone());
        for (name, e) in &c.engines {
            let previous = client.engines.entry(name.clone()).or_insert(*e);
            // Duplicate descriptors are read at different instants. Keep
            // the furthest counter, with its matching clock-domain sample.
            if e.busy > previous.busy || (e.busy == previous.busy && e.total > previous.total) {
                *previous = *e;
            }
        }
    }
    by_id
}

/// The busiest engine's utilization on `pdev` between two samples taken
/// `elapsed_ns` apart, each client counted once.
pub fn fdinfo_utilization(
    before: &[FdinfoClient],
    after: &[FdinfoClient],
    pdev: &str,
    elapsed_ns: u64,
) -> Utilization {
    if elapsed_ns == 0 {
        return Utilization::Unavailable;
    }
    let before = clients_on(before, pdev);
    let after = clients_on(after, pdev);

    // Per engine name: busy delta summed over clients, and what it counts
    // against (wall time × capacity, or xe's total cycles).
    let mut busy: BTreeMap<&str, (u64, u64)> = BTreeMap::new();
    for (id, now) in &after {
        let was = before.get(id);
        for (name, e) in &now.engines {
            let old = was
                .and_then(|c| c.engines.get(name))
                .copied()
                .unwrap_or_default();
            let delta = e.busy.saturating_sub(old.busy);
            let span = match e.total {
                Some(total) => total.saturating_sub(old.total.unwrap_or(0)),
                None => elapsed_ns.saturating_mul(e.capacity),
            };
            let slot = busy.entry(name).or_default();
            slot.0 = slot.0.saturating_add(delta);
            slot.1 = slot.1.max(span);
        }
    }

    busy.values()
        .filter(|&&(_, span)| span > 0)
        .map(|&(delta, span)| (u128::from(delta) * 100 / u128::from(span)).min(100) as u8)
        .max()
        .map_or(Utilization::Unavailable, Utilization::Percent)
}
