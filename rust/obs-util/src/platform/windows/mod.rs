//! Windows side of the [platform boundary](super). [`pipe`] and [`utf8`]
//! call Win32 directly, declared against kernel32.lib.

pub mod pipe;
pub mod utf8;

pub use pipe::Pipe;

use std::path::PathBuf;
use std::pin::Pin;
use std::sync::{Condvar, Mutex};

/// libobs `os_fopen` converts with `utf8_to_wchar` (MultiByteToWideChar,
/// flags 0), which substitutes U+FFFD for invalid UTF-8 and then attempts
/// the substituted path.
pub fn path_from_bytes(bytes: &[u8]) -> PathBuf {
    PathBuf::from(String::from_utf8_lossy(bytes).into_owned())
}

/// Counting semaphore standing in for the native one: `os_sem_*` is not
/// exported on Windows yet (`threading-windows.c` still owns it), so there
/// is nothing shared to reuse. When it is ported, this is replaced by the
/// same `Sem` the export shim uses.
pub struct Sem {
    count: Mutex<usize>,
    avail: Condvar,
}

impl Sem {
    /// `os_sem_init`; cannot fail here (std `Mutex` + `Condvar`).
    pub fn try_new(value: u32) -> Option<Pin<Box<Self>>> {
        Some(Box::pin(Sem {
            count: Mutex::new(value as usize),
            avail: Condvar::new(),
        }))
    }

    /// `os_sem_post`.
    pub fn post(&self) -> i32 {
        *self.count.lock().unwrap() += 1;
        self.avail.notify_one();
        0
    }

    /// `os_sem_wait`.
    pub fn wait(&self) -> i32 {
        let mut count = self.count.lock().unwrap();
        while *count == 0 {
            count = self.avail.wait(count).unwrap();
        }
        *count -= 1;
        0
    }
}
