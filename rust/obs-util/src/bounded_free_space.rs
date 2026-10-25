//! `os_get_free_disk_space` with a deadline (#1345).
//!
//! `statvfs` / `GetDiskFreeSpaceEx` on a hung network share can block for
//! minutes. obs-websocket's `GetStats` calls the export on a pool thread once
//! per request, so a client polling every second filled the pool and stopped
//! every request and event. Here each path has at most one query in flight on
//! a helper thread; a call waits for it up to the deadline, then returns the
//! path's last value (0 before the first one, which callers already read as
//! "unknown"). A responsive volume answers well inside the deadline, so its
//! result is unchanged.

use std::collections::HashMap;
use std::sync::{Arc, Condvar, Mutex, PoisonError};
use std::time::Duration;

/// How long a caller waits for a fresh answer.
pub const DEADLINE: Duration = Duration::from_millis(200);

/// The platform query: bytes free on the volume of a path.
pub type Query = Arc<dyn Fn(&[u8]) -> u64 + Send + Sync>;

/// One query's answer, set once by its helper thread.
#[derive(Default)]
struct Answer {
    value: Mutex<Option<u64>>,
    ready: Condvar,
}

#[derive(Default)]
struct PathState {
    last: u64,
    in_flight: Option<Arc<Answer>>,
}

pub struct BoundedFreeSpace {
    query: Query,
    deadline: Duration,
    paths: Mutex<HashMap<Vec<u8>, PathState>>,
}

impl BoundedFreeSpace {
    pub fn new(query: Query, deadline: Duration) -> Self {
        Self {
            query,
            deadline,
            paths: Mutex::new(HashMap::new()),
        }
    }

    /// Bytes free on the volume of `path`, within about the deadline.
    pub fn get(&self, path: &[u8]) -> u64 {
        let answer = {
            let mut paths = self.paths.lock().unwrap_or_else(PoisonError::into_inner);
            let state = paths.entry(path.to_vec()).or_default();
            if let Some(answer) = &state.in_flight {
                match *answer.value.lock().unwrap_or_else(PoisonError::into_inner) {
                    // The hung query still holds its helper: don't start another.
                    None => return state.last,
                    Some(value) => state.last = value,
                }
            }
            let answer = Arc::new(Answer::default());
            let query = self.query.clone();
            let owned = path.to_vec();
            let helper = answer.clone();
            let spawned = std::thread::Builder::new()
                .name("obs-free-space".into())
                .spawn(move || {
                    let value = query(&owned);
                    *helper.value.lock().unwrap_or_else(PoisonError::into_inner) = Some(value);
                    helper.ready.notify_all();
                });
            if spawned.is_err() {
                // No thread to spare: answer inline, as before #1345.
                state.in_flight = None;
                drop(paths);
                return (self.query)(path);
            }
            state.in_flight = Some(answer.clone());
            answer
        };
        let value = {
            let guard = answer.value.lock().unwrap_or_else(PoisonError::into_inner);
            let (guard, _) = answer
                .ready
                .wait_timeout_while(guard, self.deadline, |value| value.is_none())
                .unwrap_or_else(PoisonError::into_inner);
            *guard
        };
        let mut paths = self.paths.lock().unwrap_or_else(PoisonError::into_inner);
        let state = paths.entry(path.to_vec()).or_default();
        match value {
            Some(value) => {
                state.last = value;
                if state
                    .in_flight
                    .as_ref()
                    .is_some_and(|a| Arc::ptr_eq(a, &answer))
                {
                    state.in_flight = None;
                }
                value
            }
            None => state.last,
        }
    }
}

/// The process-wide instance behind the `os_get_free_disk_space` export,
/// built on first use around the host's query.
pub fn shared(query: fn(&[u8]) -> u64) -> &'static BoundedFreeSpace {
    static SHARED: std::sync::OnceLock<BoundedFreeSpace> = std::sync::OnceLock::new();
    SHARED.get_or_init(|| BoundedFreeSpace::new(Arc::new(query), DEADLINE))
}
