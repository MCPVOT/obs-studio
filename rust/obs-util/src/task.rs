//! Safe core for `libobs/util/task.c`.
//!
//! The C ABI shim lives in [`crate::ffi::task`]. The queue owns one worker
//! thread that runs tasks FIFO; [`TaskQueue::wait`] queues a marker behind
//! the pending tasks and blocks the caller until the worker reaches it.
//!
//! Reproduced from the C, including its quirks: a wait or stop marker found
//! with tasks still behind it is re-queued once (it yields to exactly one
//! task, not all of them), and `wait` reports whether a non-marker task ran
//! while the `waiting` flag was set — a racy value callers may not rely on.

use std::cell::Cell;
use std::collections::VecDeque;
use std::pin::Pin;
use std::sync::atomic::{AtomicI64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread::JoinHandle;

#[cfg(unix)]
use crate::threading::Sem;
use crate::threading::{Event, EventType};

/// Boxed task, the safe equivalent of `os_task_t` + `void *param`.
pub type Task = Box<dyn FnOnce() + Send + 'static>;

// `static THREAD_LOCAL` in the C: one instance per thread, shared by every
// queue. `THREAD_ID` is only nonzero on a queue's worker thread;
// `EXIT_THREAD` is only ever set on a worker by its own stop marker.
thread_local! {
    static EXIT_THREAD: Cell<bool> = const { Cell::new(false) };
    static THREAD_ID: Cell<i64> = const { Cell::new(0) };
}

static THREAD_ID_COUNTER: AtomicI64 = AtomicI64::new(1);

/// `os_sem_init(&tq->sem, 0)`. On Unix this is the same native semaphore
/// behind the exported `os_sem_*` calls ([`crate::threading::Sem`], the
/// `libc::sem_t`/Mach port); on Windows `os_sem_*` is not ported yet
/// (`threading-windows.c` still owns it), so the condvar sem below fills
/// in until then.
#[cfg(unix)]
fn sem_init_zero() -> Option<Pin<Box<Sem>>> {
    let mut sem = Box::<Sem>::new_uninit();
    // SAFETY: `sem` is `size_of::<Sem>()` writable uninit bytes; a failed
    // init leaves it dead with nothing to drop — exactly the C's fail
    // path, which frees the queue and returns NULL.
    if unsafe { Sem::init_in(sem.as_mut_ptr(), 0) } != 0 {
        return None;
    }
    // SAFETY: `*sem` is a live `Sem`, pinned in its box so it cannot move.
    Some(unsafe { Box::into_pin(sem.assume_init()) })
}

/// Counting semaphore for non-Unix builds only: `os_sem_*` is not
/// exported there yet, so there is nothing shared to reuse. When
/// `threading-windows.c` lands, this is deleted in favour of the same
/// `Sem` the export shim uses.
#[cfg(not(unix))]
struct Sem {
    count: Mutex<usize>,
    avail: std::sync::Condvar,
}

#[cfg(not(unix))]
impl Sem {
    fn post(&self) {
        *self.count.lock().unwrap() += 1;
        self.avail.notify_one();
    }

    fn wait(&self) -> i32 {
        let mut count = self.count.lock().unwrap();
        while *count == 0 {
            count = self.avail.wait(count).unwrap();
        }
        *count -= 1;
        0
    }
}

#[cfg(not(unix))]
fn sem_init_zero() -> Option<Pin<Box<Sem>>> {
    Some(Box::pin(Sem {
        count: Mutex::new(0),
        avail: std::sync::Condvar::new(),
    }))
}

/// A queued entry. `wait` and `stop` are distinct markers so the worker can
/// apply the C reorder rule (`ti.task == wait_for_thread` / `stop_thread`).
enum Entry {
    Task(Task),
    WaitMarker,
    StopMarker,
}

struct State {
    tasks: VecDeque<Entry>,
    waiting: bool,
    tasks_processed: bool,
}

struct Inner {
    id: i64,
    sem: Pin<Box<Sem>>,
    wait_event: Event,
    state: Mutex<State>,
}

/// `struct os_task_queue`.
pub struct TaskQueue {
    inner: Arc<Inner>,
    worker: Option<JoinHandle<()>>,
}

impl TaskQueue {
    /// `os_task_queue_create`.
    pub fn new() -> Option<Self> {
        // The C returns NULL when os_sem_init fails; os_event_init and
        // pthread_create failures also unwind to NULL. Event::new cannot
        // fail (std Mutex+Condvar), matching the C only in that an event
        // init failure is a resource failure the process does not survive
        // either way. `init_in` preserves the semaphore failure channel.
        let inner = Arc::new(Inner {
            id: THREAD_ID_COUNTER.fetch_add(1, Ordering::SeqCst),
            sem: sem_init_zero()?,
            wait_event: Event::new(EventType::Auto),
            state: Mutex::new(State {
                tasks: VecDeque::new(),
                waiting: false,
                tasks_processed: false,
            }),
        });

        let worker_inner = Arc::clone(&inner);
        let worker = std::thread::Builder::new()
            .name("tiny_tubular_task_thread".to_owned())
            .spawn(move || worker_loop(&worker_inner))
            .ok()?;

        Some(Self {
            inner,
            worker: Some(worker),
        })
    }

    /// `os_task_queue_queue_task`.
    pub fn queue_task(&self, task: Task) -> bool {
        self.inner
            .state
            .lock()
            .unwrap()
            .tasks
            .push_back(Entry::Task(task));
        self.inner.sem.post();
        true
    }

    /// `os_task_queue_wait`.
    pub fn wait(&self) -> bool {
        {
            let mut state = self.inner.state.lock().unwrap();
            state.waiting = true;
            state.tasks_processed = false;
            state.tasks.push_back(Entry::WaitMarker);
        }

        self.inner.sem.post();
        self.inner.wait_event.wait();

        self.inner.state.lock().unwrap().tasks_processed
    }

    /// `os_task_queue_inside`.
    pub fn inside(&self) -> bool {
        THREAD_ID.with(|id| id.get() == self.inner.id)
    }
}

fn worker_loop(inner: &Inner) {
    THREAD_ID.with(|id| id.set(inner.id));

    // `while (!exit_thread && os_sem_wait(tq->sem) == 0)`: an EINTR or a
    // destroyed semaphore ends the worker, same as the C.
    while !EXIT_THREAD.with(Cell::get) && inner.sem.wait() == 0 {
        let entry = {
            let mut state = inner.state.lock().unwrap();
            let mut entry = state.tasks.pop_front();
            // A marker with tasks still behind it is re-queued once and the
            // next entry runs first (faithful to the C push/pop pair).
            if !state.tasks.is_empty() && matches!(entry, Some(Entry::WaitMarker)) {
                state.tasks.push_back(entry.take().unwrap());
                entry = state.tasks.pop_front();
            }
            if !state.tasks.is_empty() && matches!(entry, Some(Entry::StopMarker)) {
                state.tasks.push_back(entry.take().unwrap());
                entry = state.tasks.pop_front();
            }
            if state.waiting {
                match entry {
                    Some(Entry::WaitMarker) => state.waiting = false,
                    _ => state.tasks_processed = true,
                }
            }
            entry
        };

        match entry {
            Some(Entry::Task(task)) => task(),
            Some(Entry::WaitMarker) => inner.wait_event.signal(),
            Some(Entry::StopMarker) => EXIT_THREAD.with(|exit| exit.set(true)),
            // C indexes an empty deque here; unreachable: sem count matches
            // pushes. Treat as a lost wakeup and keep looping.
            None => {}
        }
    }
}

impl Drop for TaskQueue {
    /// `os_task_queue_destroy`: queue the stop marker behind any pending
    /// tasks, then join the worker. Pending tasks run to completion first.
    fn drop(&mut self) {
        self.inner
            .state
            .lock()
            .unwrap()
            .tasks
            .push_back(Entry::StopMarker);
        self.inner.sem.post();
        if let Some(worker) = self.worker.take() {
            // Joining self would deadlock; the C API has the same constraint
            // (pthread_join on the caller's own thread).
            let _ = worker.join();
        }
    }
}
