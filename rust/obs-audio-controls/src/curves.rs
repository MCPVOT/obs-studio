//! Fader curves from `obs-audio-controls.c.h`.
//!
//! `obs_mul_to_db` and `obs_db_to_mul` are the header inlines. The three
//! curves are file-local in C and selected when the fader is created.

use crate::{db_to_mul, mul_to_db};

#[derive(Clone, Copy)]
pub(crate) enum Curve {
    Cubic,
    Iec,
    Log,
}

pub(crate) fn def_to_db(curve: Curve, def: f32) -> f32 {
    match curve {
        Curve::Cubic => cubic_def_to_db(def),
        Curve::Iec => iec_def_to_db(def),
        Curve::Log => log_def_to_db(def),
    }
}

pub(crate) fn db_to_def(curve: Curve, db: f32) -> f32 {
    match curve {
        Curve::Cubic => cubic_db_to_def(db),
        Curve::Iec => iec_db_to_def(db),
        Curve::Log => log_db_to_def(db),
    }
}

/// `min_db` for the curve. Cubic and IEC use -infinity. Log uses -96.
pub(crate) fn min_db(curve: Curve) -> f32 {
    match curve {
        Curve::Log => -96.0,
        Curve::Cubic | Curve::Iec => f32::NEG_INFINITY,
    }
}

fn cubic_def_to_db(def: f32) -> f32 {
    if def == 1.0 {
        0.0
    } else if def <= 0.0 {
        f32::NEG_INFINITY
    } else {
        mul_to_db(def * def * def)
    }
}

fn cubic_db_to_def(db: f32) -> f32 {
    if db == 0.0 {
        1.0
    } else if db == f32::NEG_INFINITY {
        0.0
    } else {
        db_to_mul(db).cbrt()
    }
}

fn iec_def_to_db(def: f32) -> f32 {
    if def == 1.0 {
        return 0.0;
    } else if def <= 0.0 {
        return f32::NEG_INFINITY;
    }

    if def >= 0.75 {
        (def - 1.0) / 0.25 * 9.0
    } else if def >= 0.5 {
        (def - 0.75) / 0.25 * 11.0 - 9.0
    } else if def >= 0.3 {
        (def - 0.5) / 0.2 * 10.0 - 20.0
    } else if def >= 0.15 {
        (def - 0.3) / 0.15 * 10.0 - 30.0
    } else if def >= 0.075 {
        (def - 0.15) / 0.075 * 10.0 - 40.0
    } else if def >= 0.025 {
        (def - 0.075) / 0.05 * 10.0 - 50.0
    } else if def >= 0.001 {
        // Characterized, not endorsed: 0.001 lands at -146.4, and the
        // reverse map treats anything below -114 as deflection 0.
        (def - 0.025) / 0.025 * 90.0 - 60.0
    } else {
        f32::NEG_INFINITY
    }
}

fn iec_db_to_def(db: f32) -> f32 {
    if db == 0.0 {
        1.0
    } else if db == f32::NEG_INFINITY {
        0.0
    } else if db >= -9.0 {
        (db + 9.0) / 9.0 * 0.25 + 0.75
    } else if db >= -20.0 {
        (db + 20.0) / 11.0 * 0.25 + 0.5
    } else if db >= -30.0 {
        (db + 30.0) / 10.0 * 0.2 + 0.3
    } else if db >= -40.0 {
        (db + 40.0) / 10.0 * 0.15 + 0.15
    } else if db >= -50.0 {
        (db + 50.0) / 10.0 * 0.075 + 0.075
    } else if db >= -60.0 {
        (db + 60.0) / 10.0 * 0.05 + 0.025
    } else if db >= -114.0 {
        (db + 150.0) / 90.0 * 0.025
    } else {
        0.0
    }
}

const LOG_OFFSET_DB: f32 = 6.0;
const LOG_RANGE_DB: f32 = 96.0;
/// `-log10f(LOG_OFFSET_DB)`, the f32 value of the C literal.
const LOG_OFFSET_VAL: f32 = -0.778_151_3;
/// `-log10f(-LOG_RANGE_DB + LOG_OFFSET_DB)`, the f32 value of the C literal.
const LOG_RANGE_VAL: f32 = -2.008_600_2;

fn log_def_to_db(def: f32) -> f32 {
    if def >= 1.0 {
        0.0
    } else if def <= 0.0 {
        f32::NEG_INFINITY
    } else {
        let span = LOG_RANGE_DB + LOG_OFFSET_DB;
        -span * (span / LOG_OFFSET_DB).powf(-def) + LOG_OFFSET_DB
    }
}

fn log_db_to_def(db: f32) -> f32 {
    if db >= 0.0 {
        1.0
    } else if db <= -96.0 {
        0.0
    } else {
        (-(-db + LOG_OFFSET_DB).log10() - LOG_RANGE_VAL) / (LOG_OFFSET_VAL - LOG_RANGE_VAL)
    }
}
