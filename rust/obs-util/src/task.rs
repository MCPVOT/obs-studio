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
use std::sync::atomic::{AtomicI64, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::thread::JoinHandle;

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

/// Counting semaphore (`os_sem_t`): `post` increments, `wait` blocks for a
/// count.
struct Sem {
    count: Mutex<usize>,
    avail: Condvar,
}

impl Sem {
    fn new() -> Self {
        Self {
            count: Mutex::new(0),
            avail: Condvar::new(),
        }
    }

    fn post(&self) {
        *self.count.lock().unwrap() += 1;
        self.avail.notify_one();
    }

    fn wait(&self) {
        let mut count = self.count.lock().unwrap();
        while *count == 0 {
            count = self.avail.wait(count).unwrap();
        }
        *count -= 1;
    }
}

/// Auto-reset event (`os_event_t` with `OS_EVENT_TYPE_AUTO`): `wait` blocks
/// until signaled, then consumes the signal.
struct AutoEvent {
    set: Mutex<bool>,
    cond: Condvar,
}

impl AutoEvent {
    fn new() -> Self {
        Self {
            set: Mutex::new(false),
            cond: Condvar::new(),
        }
    }

    fn signal(&self) {
        *self.set.lock().unwrap() = true;
        self.cond.notify_one();
    }

    fn wait(&self) {
        let mut set = self.set.lock().unwrap();
        while !*set {
            set = self.cond.wait(set).unwrap();
        }
        *set = false;
    }
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
    sem: Sem,
    wait_event: AutoEvent,
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
        let inner = Arc::new(Inner {
            id: THREAD_ID_COUNTER.fetch_add(1, Ordering::SeqCst),
            sem: Sem::new(),
            wait_event: AutoEvent::new(),
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

    while !EXIT_THREAD.with(Cell::get) {
        inner.sem.wait();

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
