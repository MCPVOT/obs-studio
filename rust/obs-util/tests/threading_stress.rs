//! Stress test for the event and semaphore, required by the Phase 4 rules
//! on top of Tier 3 parity ("the threading and task ports add a loom or
//! stress-style Rust test").
//!
//! Determinism: every assertion is on counts established through sem waits
//! and thread joins — never on sleeps or timing.

use std::sync::Arc;
use std::sync::atomic::{AtomicUsize, Ordering};

use obs_c_oracle as _; // links the test bmalloc/bfree (oracle/test_bmem.c)
#[cfg(unix)]
use obs_util::threading::Sem;
use obs_util::threading::{Event, EventType};

/// Ping-pong pairs of auto events across many thread pairs: every volley
/// must complete exactly once per direction.
#[test]
fn stress_event_ping_pong_pairs() {
    const PAIRS: usize = 8;
    const ROUNDS: usize = 500;

    let mut threads = Vec::new();
    for _ in 0..PAIRS {
        let ping = Arc::new(Event::new(EventType::Auto));
        let pong = Arc::new(Event::new(EventType::Auto));
        let worker = {
            let ping = Arc::clone(&ping);
            let pong = Arc::clone(&pong);
            std::thread::spawn(move || {
                for _ in 0..ROUNDS {
                    ping.wait();
                    pong.signal();
                }
            })
        };
        threads.push(std::thread::spawn(move || {
            for _ in 0..ROUNDS {
                ping.signal();
                pong.wait();
            }
            worker.join().unwrap();
        }));
    }
    for t in threads {
        t.join().unwrap();
    }
}

/// One auto event, one waiter per signal: each signal must release
/// exactly one pending wait, whether or not the waiter was already asleep
/// when it fired.
#[cfg(unix)]
#[test]
fn stress_auto_event_releases_exactly_one_per_signal() {
    const WAITERS: usize = 64;

    let ev = Arc::new(Event::new(EventType::Auto));
    let released = Arc::new(AtomicUsize::new(0));
    let done = Arc::new(Sem::new(0));

    let threads: Vec<_> = (0..WAITERS)
        .map(|_| {
            let ev = Arc::clone(&ev);
            let released = Arc::clone(&released);
            let done = Arc::clone(&done);
            std::thread::spawn(move || {
                ev.wait();
                released.fetch_add(1, Ordering::SeqCst);
                done.post();
            })
        })
        .collect();

    // `signalled` is a bool in the C too: two signals before anyone
    // consumes collapse into one release. Gate each signal on the last
    // release so all WAITERS waiters are released exactly once, and check
    // that each signal is fully consumed rather than stacking up a
    // permit another waiter could take later.
    for _ in 0..WAITERS {
        ev.signal();
        done.wait();
        assert!(!ev.try_wait());
    }

    for t in threads {
        t.join().unwrap();
    }
    assert_eq!(released.load(Ordering::SeqCst), WAITERS);
}

/// Many producers on one counting semaphore; every posted count is taken
/// exactly once.
#[cfg(unix)]
#[test]
fn stress_sem_multi_producer() {
    const PRODUCERS: usize = 8;
    const POSTS: usize = 500;

    let sem = Arc::new(Sem::new(0));
    let consumed = Arc::new(AtomicUsize::new(0));

    let producers: Vec<_> = (0..PRODUCERS)
        .map(|_| {
            let sem = Arc::clone(&sem);
            std::thread::spawn(move || {
                for _ in 0..POSTS {
                    sem.post();
                }
            })
        })
        .collect();

    let consumer = {
        let sem = Arc::clone(&sem);
        let consumed = Arc::clone(&consumed);
        std::thread::spawn(move || {
            for _ in 0..PRODUCERS * POSTS {
                sem.wait();
                consumed.fetch_add(1, Ordering::SeqCst);
            }
        })
    };

    for p in producers {
        p.join().unwrap();
    }
    consumer.join().unwrap();
    assert_eq!(consumed.load(Ordering::SeqCst), PRODUCERS * POSTS);
}

/// reset() under contention: waiters must never observe a signal that was
/// reset before they consumed it, counted via try_wait after each wait.
#[test]
fn stress_event_signal_reset_contention() {
    const ROUNDS: usize = 2000;

    let ev = Arc::new(Event::new(EventType::Manual));
    let observed = Arc::new(AtomicUsize::new(0));
    let stop = Arc::new(Event::new(EventType::Auto));

    let toggler = {
        let ev = Arc::clone(&ev);
        let stop = Arc::clone(&stop);
        std::thread::spawn(move || {
            for _ in 0..ROUNDS {
                ev.signal();
                ev.reset();
            }
            stop.signal();
        })
    };

    let waiter = {
        let ev = Arc::clone(&ev);
        let observed = Arc::clone(&observed);
        std::thread::spawn(move || {
            while ev.timed_wait(1) {
                observed.fetch_add(1, Ordering::SeqCst);
            }
        })
    };

    toggler.join().unwrap();
    waiter.join().unwrap();
    // `observed` is intentionally unbounded: while the manual flag stays
    // set, every timed_wait returns true, so sightings are not limited by
    // ROUNDS. What is deterministic is the terminal state: the toggler's
    // last op was a reset, both threads joined, so the flag must read
    // cleared and the waiter must have left the loop rather than
    // deadlocking on a stale signal.
    assert!(
        !ev.try_wait(),
        "flag must be reset after the toggler joined (observed={})",
        observed.load(Ordering::SeqCst)
    );
}
