//! `obs_fader_t`. Opaque in the public header. The safe object is not `repr(C)`.

use std::sync::Mutex;

use crate::curves::{self, Curve};
use crate::{FADER_CUBIC, FADER_IEC, FADER_LOG, db_to_mul, mul_to_db};

/// `obs_fader_changed_t`, with the `void *` param carried as `usize`.
pub type FaderCb = fn(param: usize, db: f32);

struct State {
    curve: Curve,
    max_db: f32,
    min_db: f32,
    cur_db: f32,
    /// Characterized, not endorsed: `set_db` arms this even with no source.
    /// The next volume signal is dropped. Attach does not clear it.
    ignore_next: bool,
    attached: bool,
    volume: f32,
}

/// Cubic, IEC, or log fader. An unknown type is not created.
pub struct Fader {
    state: Mutex<State>,
    callbacks: Mutex<Vec<(FaderCb, usize)>>,
}

impl Fader {
    /// `obs_fader_create`. `None` for a type other than cubic, IEC, or log.
    pub fn create(ty: i32) -> Option<Self> {
        let curve = match ty {
            FADER_CUBIC => Curve::Cubic,
            FADER_IEC => Curve::Iec,
            FADER_LOG => Curve::Log,
            _ => return None,
        };
        Some(Self {
            state: Mutex::new(State {
                curve,
                max_db: 0.0,
                min_db: curves::min_db(curve),
                cur_db: 0.0,
                ignore_next: false,
                attached: false,
                volume: 0.0,
            }),
            callbacks: Mutex::new(Vec::new()),
        })
    }

    /// `obs_fader_get_db`.
    pub fn db(&self) -> f32 {
        self.lock().cur_db
    }

    /// The curve chosen at create, as `enum obs_fader_type`.
    ///
    /// The shim uses this to return the matching unexported `db_to_def`.
    pub(crate) fn kind(&self) -> i32 {
        match self.lock().curve {
            Curve::Cubic => FADER_CUBIC,
            Curve::Iec => FADER_IEC,
            Curve::Log => FADER_LOG,
        }
    }

    /// `obs_fader_get_deflection`.
    pub fn deflection(&self) -> f32 {
        let state = self.lock();
        curves::db_to_def(state.curve, state.cur_db)
    }

    /// `obs_fader_get_mul`.
    pub fn mul(&self) -> f32 {
        db_to_mul(self.lock().cur_db)
    }

    /// What `obs_fader_db_to_def` returns, applied to `db`.
    pub fn db_to_def(&self, db: f32) -> f32 {
        curves::db_to_def(self.lock().curve, db)
    }

    /// `obs_fader_set_db`.
    ///
    /// Clamps to the curve's range. Below `min_db` the stored value is
    /// -infinity, not the floor. Arms `ignore_next_signal`. When a source
    /// volume is attached, that volume becomes `db_to_mul` of the stored
    /// dB. This does not run fader callbacks. `obs_source_set_volume`
    /// signals `volume`, which is [`Self::on_volume`] and clears the flag.
    pub fn set_db(&self, db: f32) -> bool {
        let mut state = self.lock();
        let mut clamped = false;
        state.cur_db = db;
        if state.cur_db > state.max_db {
            state.cur_db = state.max_db;
            clamped = true;
        }
        if state.cur_db < state.min_db {
            state.cur_db = f32::NEG_INFINITY;
            clamped = true;
        }
        state.ignore_next = true;
        if state.attached {
            state.volume = db_to_mul(state.cur_db);
        }
        !clamped
    }

    /// `obs_fader_set_deflection`.
    pub fn set_deflection(&self, def: f32) -> bool {
        let db = curves::def_to_db(self.lock().curve, def);
        self.set_db(db)
    }

    /// `obs_fader_set_mul`.
    pub fn set_mul(&self, mul: f32) -> bool {
        self.set_db(mul_to_db(mul))
    }

    /// `obs_fader_attach_source`: drop any current source, then copy `volume`
    /// into the current dB. `ignore_next_signal` stays as it was.
    pub fn attach_volume(&self, volume: f32) {
        self.detach();
        let mut state = self.lock();
        state.attached = true;
        state.volume = volume;
        state.cur_db = mul_to_db(volume);
    }

    /// `obs_fader_detach_source`. The current dB stays.
    pub fn detach(&self) {
        self.lock().attached = false;
    }

    /// The attached source volume, if `attach_volume` is in effect.
    pub fn attached_volume(&self) -> Option<f32> {
        let state = self.lock();
        if state.attached {
            Some(state.volume)
        } else {
            None
        }
    }

    /// The fader's `volume` signal handler.
    ///
    /// A set ignore flag is cleared and the update is dropped. Otherwise
    /// the current dB becomes `mul_to_db(mul)` and callbacks run, last
    /// added first. The handler does not check that a source is still
    /// attached; disconnect is the caller's job.
    pub fn on_volume(&self, mul: f32) {
        let db = {
            let mut state = self.lock();
            if state.ignore_next {
                state.ignore_next = false;
                return;
            }
            let db = mul_to_db(mul);
            state.cur_db = db;
            if state.attached {
                state.volume = mul;
            }
            db
        };
        self.signal(db);
    }

    /// The fader's `destroy` signal handler. Detaches.
    pub fn on_destroy(&self) {
        self.detach();
    }

    /// `obs_fader_add_callback`.
    pub fn add_callback(&self, cb: FaderCb, param: usize) {
        self.callbacks
            .lock()
            .expect("fader callbacks")
            .push((cb, param));
    }

    /// `obs_fader_remove_callback`. One matching `(cb, param)` pair.
    pub fn remove_callback(&self, cb: FaderCb, param: usize) {
        let mut callbacks = self.callbacks.lock().expect("fader callbacks");
        if let Some(index) = callbacks.iter().position(|&(existing, existing_param)| {
            std::ptr::fn_addr_eq(existing, cb) && existing_param == param
        }) {
            callbacks.remove(index);
        }
    }

    fn lock(&self) -> std::sync::MutexGuard<'_, State> {
        self.state.lock().expect("fader")
    }

    fn signal(&self, db: f32) {
        let callbacks = self.callbacks.lock().expect("fader callbacks");
        for &(cb, param) in callbacks.iter().rev() {
            cb(param, db);
        }
    }
}
