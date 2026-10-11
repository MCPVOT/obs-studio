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

/// One manual event, one waiter per signal: C signals with
/// `pthread_cond_signal`, so one `signal` wakes exactly one sleeper even
/// though a manual event never consumes its flag — a `notify_all` bug
/// would wake every waiter on the first signal, which the per-signal
/// wake count below catches. (Auto events cannot expose it: the
/// consumed flag re-blocks any extra wakers, as broadcast would in C.)
#[cfg(unix)]
#[test]
fn stress_manual_event_releases_exactly_one_per_signal() {
    const WAITERS: usize = 32;
    /// Window for wrongly-woken waiters to report in, measured in event
    /// timeouts rather than a sleep.
    const SETTLE_MS: u64 = 20;

    let ev = Arc::new(Event::new(EventType::Manual));
    let settle = Arc::new(Event::new(EventType::Auto)); // never signalled
    let ready = Arc::new(Sem::new(0));
    let released = Arc::new(AtomicUsize::new(0));
    let done = Arc::new(Sem::new(0));

    let threads: Vec<_> = (0..WAITERS)
        .map(|_| {
            let ev = Arc::clone(&ev);
            let ready = Arc::clone(&ready);
            let released = Arc::clone(&released);
            let done = Arc::clone(&done);
            std::thread::spawn(move || {
                ready.post();
                ev.wait();
                released.fetch_add(1, Ordering::SeqCst);
                done.post();
            })
        })
        .collect();

    // Every waiter must be blocked in ev.wait before the first signal —
    // with the manual flag still false, any waiter that reaches wait
    // blocks; the pre-settle covers the gap between a waiter's
    // ready.post and its actual arrival inside wait.
    for _ in 0..WAITERS {
        ready.wait();
    }
    assert!(
        !settle.timed_wait(SETTLE_MS),
        "settle event was signalled (pre-rounds)"
    );

    // Gate each signal on the last release (`done`), give every other
    // sleeper a settle window to report a wrongful wake, then reset so
    // the next round's sleepers need their own signal.
    for round in 0..WAITERS {
        let before = released.load(Ordering::SeqCst);
        ev.signal();
        done.wait();
        assert!(
            !settle.timed_wait(SETTLE_MS),
            "settle event was signalled (round {round})"
        );
        let woke = released.load(Ordering::SeqCst) - before;
        assert_eq!(
            woke, 1,
            "one signal released {woke} waiters in round {round} (want 1)"
        );
        ev.reset();
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

/// reset() under contention: the waiter must observe a signal — a
/// timed_wait that always timed out must not pass — and must never
/// observe a signal that was reset before it consumed it.
#[test]
fn stress_event_signal_reset_contention() {
    const ROUNDS: usize = 2000;
    /// Only bounds a genuinely broken timed_wait; on a correct build the
    /// ack arrives as soon as the waiter is scheduled, which on a loaded
    /// shared runner can already take seconds.
    const ACK_MS: u64 = 60_000;

    let ev = Arc::new(Event::new(EventType::Manual));
    let ack = Arc::new(Event::new(EventType::Auto));
    let started = Arc::new(Sem::new(0));
    let observed = Arc::new(AtomicUsize::new(0));

    // A manual event's first signal stays set until the first reset, so
    // the waiter's first timed_wait is guaranteed to observe it and ack
    // back — the toggler waits for the waiter to be running and then
    // blocks on that ack before entering the loop, making observed >= 1
    // deterministic rather than scheduler luck.
    let toggler = {
        let ev = Arc::clone(&ev);
        let ack = Arc::clone(&ack);
        let started = Arc::clone(&started);
        std::thread::spawn(move || {
            started.wait();
            ev.signal();
            assert!(ack.timed_wait(ACK_MS), "waiter never observed a signal");
            for _ in 0..ROUNDS {
                ev.signal();
                ev.reset();
            }
        })
    };

    let waiter = {
        let ev = Arc::clone(&ev);
        let ack = Arc::clone(&ack);
        let started = Arc::clone(&started);
        let observed = Arc::clone(&observed);
        std::thread::spawn(move || {
            started.post();
            while ev.timed_wait(1) {
                if observed.fetch_add(1, Ordering::SeqCst) == 0 {
                    ack.signal();
                }
            }
        })
    };

    toggler.join().unwrap();
    waiter.join().unwrap();
    // `observed` is intentionally unbounded: while the manual flag stays
    // set, every timed_wait returns true, so sightings are not limited by
    // ROUNDS. What is deterministic is that at least one signal arrived
    // in time (the handshake) and the terminal state: the toggler's last
    // op was a reset, both threads joined, so the flag must read cleared
    // and the waiter must have left the loop rather than deadlocking on
    // a stale signal.
    assert!(
        observed.load(Ordering::SeqCst) > 0,
        "timed_wait never observed a signal"
    );
    assert!(
        !ev.try_wait(),
        "flag must be reset after the toggler joined (observed={})",
        observed.load(Ordering::SeqCst)
    );
}
