//! Tier 1 mirror of `test/cmocka/test_audio_controls.c`.
//!
//! The live `obs_source_t` stays in that cmocka test. Here a source is a
//! volume plus a registration. `set_db` arms `ignore_next_signal`; the
//! helper below fires the `volume` signal `obs_source_set_volume` would
//! emit. The one `bzalloc` per object stays with the cmocka `bnum_allocs`
//! checks. The shim owns that count.
//!
//! Not called, because the C crashes or does not return:
//! `obs_volmeter_get_nr_channels(NULL)`,
//! `obs_volmeter_set_peak_meter_type(NULL, ...)`, and
//! `obs_fader_db_to_def(NULL)`.
//!
//! The "Null 'fader'" log line is `obs_ptr_valid` in the shim. Tier 2
//! still counts it.

mod parity;

use obs_audio_controls::{
    AudioPacket, FADER_CUBIC, FADER_IEC, FADER_LOG, Fader, MAX_AUDIO_CHANNELS, MeterCb,
    SAMPLE_PEAK_METER, SpeakerLayout, TRUE_PEAK_METER, Volmeter, db_to_mul, mul_to_db,
    speaker_layout_to_c,
};

const TONE_FRAMES: usize = 8;

fn speakers(layout: SpeakerLayout) -> i32 {
    speaker_layout_to_c(layout)
}

fn assert_db(got: f32, expect: f32) {
    if expect.is_nan() {
        assert!(got.is_nan(), "expected NaN, got {got}");
        return;
    }
    if expect.is_infinite() {
        assert!(
            got.is_infinite() && got.is_sign_negative() == expect.is_sign_negative(),
            "expected {expect}, got {got}"
        );
        return;
    }
    let diff = (got - expect).abs();
    assert!(diff <= 0.0001, "expected {expect}, got {got}");
}

/// `obs_source_set_volume` always signals. Swallow or apply that here.
fn push_db(fader: &Fader, db: f32) -> bool {
    let ok = fader.set_db(db);
    if let Some(vol) = fader.attached_volume() {
        fader.on_volume(vol);
    }
    ok
}

struct Wire {
    source: Option<u8>,
}

fn attach_fader(fader: &Fader, wire: &mut Wire, source: u8, volume: f32) {
    fader.attach_volume(volume);
    wire.source = Some(source);
}

fn detach_fader(fader: &Fader, wire: &mut Wire) {
    fader.detach();
    wire.source = None;
}

fn set_fader_volume(fader: &Fader, wire: &Wire, source: u8, volume: f32) {
    if wire.source == Some(source) {
        fader.on_volume(volume);
    }
}

fn attach_meter(meter: &Volmeter, wire: &mut Wire, source: u8, volume: f32) {
    meter.attach_volume(volume);
    wire.source = Some(source);
}

fn detach_meter(meter: &Volmeter, wire: &mut Wire) {
    meter.detach();
    wire.source = None;
}

fn set_meter_volume(meter: &Volmeter, wire: &Wire, source: u8, volume: f32) {
    if wire.source == Some(source) {
        meter.on_volume(volume);
    }
}

fn feed(meter: &Volmeter, wire: &Wire, source: u8, packet: &AudioPacket<'_>) {
    if wire.source == Some(source) {
        meter.on_data(packet);
    }
}

fn stereo<'a>(
    left: &'a [f32],
    right: &'a [f32],
    layout: SpeakerLayout,
    capture_muted: bool,
    user_muted: bool,
) -> AudioPacket<'a> {
    let mut planes = [None; MAX_AUDIO_CHANNELS];
    planes[0] = Some(left);
    planes[1] = Some(right);
    AudioPacket {
        planes,
        frames: left.len(),
        speakers: speakers(layout),
        aligned: [true; MAX_AUDIO_CHANNELS],
        capture_muted,
        user_muted,
    }
}

struct CallLog {
    seq: [i32; 8],
    n: usize,
}

struct FaderNote {
    id: i32,
    calls: i32,
    db: f32,
    log: *mut CallLog,
}

fn on_fader(param: usize, db: f32) {
    let note = unsafe { &mut *(param as *mut FaderNote) };
    if !note.log.is_null() {
        let log = unsafe { &mut *note.log };
        if log.n < 8 {
            log.seq[log.n] = note.id;
            log.n += 1;
        }
    }
    note.db = db;
    note.calls += 1;
}

struct MeterNote {
    id: i32,
    calls: i32,
    log: *mut CallLog,
    magnitude: [f32; MAX_AUDIO_CHANNELS],
    peak: [f32; MAX_AUDIO_CHANNELS],
    input_peak: [f32; MAX_AUDIO_CHANNELS],
}

fn on_meter(
    param: usize,
    magnitude: &[f32; MAX_AUDIO_CHANNELS],
    peak: &[f32; MAX_AUDIO_CHANNELS],
    input_peak: &[f32; MAX_AUDIO_CHANNELS],
) {
    let note = unsafe { &mut *(param as *mut MeterNote) };
    if !note.log.is_null() {
        let log = unsafe { &mut *note.log };
        if log.n < 8 {
            log.seq[log.n] = note.id;
            log.n += 1;
        }
    }
    note.magnitude = *magnitude;
    note.peak = *peak;
    note.input_peak = *input_peak;
    note.calls += 1;
}

fn meter_cb() -> MeterCb {
    on_meter
}

/// `null_pointers_test`.
///
/// A null fader or meter returns false from the setters and attach, 0 from
/// the getters, and destroy/detach are safe. That is the shim. The log line
/// stays in the cmocka test. `obs_mul_to_db` and `obs_db_to_mul` are here.
#[test]
fn null_pointers_test() {
    assert_db(mul_to_db(0.0), f32::NEG_INFINITY);
    assert_db(mul_to_db(1.0), 0.0);
    assert_db(db_to_mul(0.0), 1.0);
    assert_db(db_to_mul(f32::NEG_INFINITY), 0.0);
}

/// `fresh_fader_is_unity_test`. Allocation count stays in the cmocka test.
#[test]
fn fresh_fader_is_unity_test() {
    for ty in [FADER_CUBIC, FADER_IEC, FADER_LOG] {
        let fader = Fader::create(ty).expect("fader");
        assert_db(fader.db(), 0.0);
        assert_db(fader.deflection(), 1.0);
        assert_db(fader.mul(), 1.0);
        assert_db(fader.db_to_def(0.0), 1.0);
    }
}

/// `bad_type_test`. Characterized, not endorsed: an unknown fader type is
/// dropped. A volmeter stores any type and never reads it.
#[test]
fn bad_type_test() {
    assert!(Fader::create(99).is_none());
    // Create has to accept the type. The levels test shows it is not read.
    let _meter = Volmeter::create(99);
}

/// `cubic_curve_test`.
#[test]
fn cubic_curve_test() {
    let fader = Fader::create(FADER_CUBIC).expect("fader");

    assert!(fader.set_deflection(1.0));
    assert_db(fader.db(), 0.0);

    assert!(fader.set_deflection(0.0));
    assert_db(fader.db(), f32::NEG_INFINITY);
    assert_db(fader.mul(), 0.0);

    assert!(fader.set_deflection(0.5));
    assert_db(fader.db(), -18.0618);
    assert_db(fader.deflection(), 0.5);

    // Above 1 the curve is positive dB and the fader clamps to 0.
    assert!(!fader.set_deflection(1.5));
    assert_db(fader.db(), 0.0);
    assert_db(fader.deflection(), 1.0);

    // min_db is -infinity, so a finite value below 0 is stored as given.
    assert!(fader.set_db(-1000.0));
    assert_db(fader.db(), -1000.0);

    // Characterized, not endorsed: NaN is stored. Deflection of NaN is 0.
    assert!(fader.set_db(f32::NAN));
    assert_db(fader.db(), f32::NAN);
    assert_db(fader.deflection(), 0.0);

    assert!(fader.set_mul(0.5));
    assert_db(fader.db(), mul_to_db(0.5));
    assert_db(fader.mul(), 0.5);

    assert!(fader.set_mul(0.0));
    assert_db(fader.db(), f32::NEG_INFINITY);
    assert_db(fader.mul(), 0.0);

    // Above 0 dB the cubic fader clamps and reports it.
    assert!(!fader.set_mul(2.0));
    assert_db(fader.db(), 0.0);
    assert_db(fader.mul(), 1.0);
}

/// `iec_breakpoints_test`.
#[test]
fn iec_breakpoints_test() {
    let fader = Fader::create(FADER_IEC).expect("fader");
    let def = [1.0, 0.75, 0.5, 0.3, 0.15, 0.075, 0.025];
    let db = [0.0, -9.0, -20.0, -30.0, -40.0, -50.0, -60.0];

    for i in 0..def.len() {
        assert!(fader.set_deflection(def[i]));
        assert_db(fader.db(), db[i]);
        assert_db(fader.deflection(), def[i]);
    }

    assert!(fader.set_deflection(0.0));
    assert_db(fader.db(), f32::NEG_INFINITY);
    assert_db(fader.deflection(), 0.0);

    // Characterized, not endorsed: def 0.001 maps to -146.4 dB, and the
    // reverse map treats anything below -114 dB as deflection 0.
    assert!(fader.set_deflection(0.001));
    assert_db(fader.db(), -146.4);
    assert_db(fader.deflection(), 0.0);

    assert!(fader.set_deflection(0.01));
    assert_db(fader.db(), -114.0);
    assert_db(fader.deflection(), 0.01);
}

/// `log_curve_test`.
#[test]
fn log_curve_test() {
    let fader = Fader::create(FADER_LOG).expect("fader");

    assert!(fader.set_deflection(0.5));
    assert_db(fader.db(), -18.7386);
    assert_db(fader.deflection(), 0.5);

    assert!(fader.set_deflection(0.25));
    assert_db(fader.db(), -44.2329);

    // def >= 1 is already 0 dB, so set_deflection does not report a clamp.
    assert!(fader.set_deflection(1.5));
    assert_db(fader.db(), 0.0);
    assert_db(fader.deflection(), 1.0);

    assert!(fader.set_db(-96.0));
    assert_db(fader.db(), -96.0);
    assert_db(fader.deflection(), 0.0);

    // Characterized, not endorsed: below -96 the stored value is -infinity,
    // not the -96 floor. set_mul(0) takes the same path.
    assert!(!fader.set_db(-100.0));
    assert_db(fader.db(), f32::NEG_INFINITY);
    assert_db(fader.deflection(), 0.0);
    assert_db(fader.mul(), 0.0);

    assert!(!fader.set_mul(0.0));
    assert_db(fader.db(), f32::NEG_INFINITY);
    assert_db(fader.mul(), 0.0);
}

/// `callback_order_and_source_test`.
#[test]
fn callback_order_and_source_test() {
    let fader = Fader::create(FADER_CUBIC).expect("fader");
    let mut wire = Wire { source: None };
    let mut log = CallLog { seq: [0; 8], n: 0 };
    let mut a = FaderNote {
        id: 1,
        calls: 0,
        db: 0.0,
        log: &mut log,
    };
    let mut b = FaderNote {
        id: 2,
        calls: 0,
        db: 0.0,
        log: &mut log,
    };

    // obs_fader_attach_source(fader, NULL) is false. The shim checks that.
    attach_fader(&fader, &mut wire, 1, 1.0);
    assert_db(fader.db(), 0.0);

    fader.add_callback(on_fader, &mut a as *mut FaderNote as usize);
    fader.add_callback(on_fader, &mut b as *mut FaderNote as usize);

    // Setting through the fader swallows the volume signal it causes.
    assert!(push_db(&fader, -12.0));
    assert_eq!(a.calls, 0);
    assert_eq!(b.calls, 0);
    assert_db(fader.db(), -12.0);
    assert_db(fader.attached_volume().expect("volume"), db_to_mul(-12.0));

    set_fader_volume(&fader, &wire, 1, 0.5);
    assert_eq!(a.calls, 1);
    assert_eq!(b.calls, 1);
    // Walk is last-added first.
    assert_eq!(log.n, 2);
    assert_eq!(log.seq[0], 2);
    assert_eq!(log.seq[1], 1);
    assert_db(b.db, mul_to_db(0.5));
    assert_db(fader.db(), mul_to_db(0.5));

    log.n = 0;
    fader.remove_callback(on_fader, &mut b as *mut FaderNote as usize);
    set_fader_volume(&fader, &wire, 1, 0.25);
    assert_eq!(b.calls, 1);
    assert_eq!(a.calls, 2);
    assert_eq!(log.n, 1);
    assert_eq!(log.seq[0], 1);
    assert_db(fader.db(), mul_to_db(0.25));

    // The first source is detached. Its later volume change is ignored.
    attach_fader(&fader, &mut wire, 2, 1.0);
    assert_db(fader.db(), 0.0);
    a.calls = 0;
    set_fader_volume(&fader, &wire, 1, 0.1);
    assert_eq!(a.calls, 0);
    log.n = 0;
    set_fader_volume(&fader, &wire, 2, 0.8);
    assert_eq!(a.calls, 1);
    assert_eq!(log.n, 1);
    assert_eq!(log.seq[0], 1);
    assert_db(a.db, mul_to_db(0.8));

    // Detach leaves the last dB and ignores later volume changes.
    detach_fader(&fader, &mut wire);
    log.n = 0;
    a.calls = 0;
    set_fader_volume(&fader, &wire, 2, 0.4);
    assert_eq!(a.calls, 0);
    assert_eq!(log.n, 0);
    assert_db(fader.db(), mul_to_db(0.8));

    // The destroy signal detaches. The last dB stays.
    fader.on_destroy();
    assert_db(fader.db(), mul_to_db(0.8));
    assert!(fader.set_db(-3.0));
    assert_db(fader.db(), -3.0);
}

/// `ignore_sticks_across_attach_test`. Characterized, not endorsed:
/// `set_db` arms `ignore_next_signal` with no source attached. The next
/// external volume change is dropped, and the current dB stays at the
/// value attach copied.
#[test]
fn ignore_sticks_across_attach_test() {
    let fader = Fader::create(FADER_CUBIC).expect("fader");
    let mut wire = Wire { source: None };
    let mut note = FaderNote {
        id: 1,
        calls: 0,
        db: 0.0,
        log: std::ptr::null_mut(),
    };

    assert!(fader.set_db(-10.0));
    fader.add_callback(on_fader, &mut note as *mut FaderNote as usize);
    attach_fader(&fader, &mut wire, 1, 1.0);
    assert_db(fader.db(), 0.0);

    set_fader_volume(&fader, &wire, 1, 0.5);
    assert_eq!(note.calls, 0);
    assert_db(fader.db(), 0.0);

    set_fader_volume(&fader, &wire, 1, 0.25);
    assert_eq!(note.calls, 1);
    assert_db(fader.db(), mul_to_db(0.25));
}

fn tone(left: f32, right: f32, frames: usize) -> (Vec<f32>, Vec<f32>) {
    (vec![left; frames], vec![right; frames])
}

/// `volmeter_levels_test`.
#[test]
fn volmeter_levels_test() {
    let cubic = Volmeter::create(FADER_CUBIC);
    let logf = Volmeter::create(FADER_LOG);
    let mut cubic_wire = Wire { source: None };
    let mut log_wire = Wire { source: None };
    let mut a = MeterNote {
        id: 0,
        calls: 0,
        log: std::ptr::null_mut(),
        magnitude: [0.0; MAX_AUDIO_CHANNELS],
        peak: [0.0; MAX_AUDIO_CHANNELS],
        input_peak: [0.0; MAX_AUDIO_CHANNELS],
    };
    let mut b = MeterNote {
        id: 0,
        calls: 0,
        log: std::ptr::null_mut(),
        magnitude: [0.0; MAX_AUDIO_CHANNELS],
        peak: [0.0; MAX_AUDIO_CHANNELS],
        input_peak: [0.0; MAX_AUDIO_CHANNELS],
    };
    let mix = Some(speakers(SpeakerLayout::Stereo));

    assert_eq!(cubic.nr_channels(mix), 0);
    // obs_volmeter_attach_source(meter, NULL) is false. The shim checks that.
    attach_meter(&cubic, &mut cubic_wire, 1, 1.0);
    attach_meter(&logf, &mut log_wire, 1, 1.0);
    // No packet yet, so the speaker layout is still unknown.
    assert_eq!(cubic.nr_channels(mix), 0);

    cubic.add_callback(meter_cb(), &mut a as *mut MeterNote as usize);
    logf.add_callback(meter_cb(), &mut b as *mut MeterNote as usize);

    let (left, right) = tone(0.5, 0.25, TONE_FRAMES);
    let packet = stereo(&left, &right, SpeakerLayout::Stereo, false, false);
    feed(&cubic, &cubic_wire, 1, &packet);
    feed(&logf, &log_wire, 1, &packet);

    assert_eq!(a.calls, 1);
    assert_eq!(b.calls, 1);
    assert_eq!(cubic.nr_channels(mix), 2);
    // Characterized, not endorsed: the fader type is stored and not used.
    for channel in 0..MAX_AUDIO_CHANNELS {
        assert_db(a.magnitude[channel], b.magnitude[channel]);
        assert_db(a.peak[channel], b.peak[channel]);
        assert_db(a.input_peak[channel], b.input_peak[channel]);
    }
    assert_db(a.magnitude[0], mul_to_db(0.5));
    assert_db(a.magnitude[1], mul_to_db(0.25));
    assert_db(a.peak[0], mul_to_db(0.5));
    assert_db(a.peak[1], mul_to_db(0.25));
    assert_db(a.input_peak[0], mul_to_db(0.5));
    for channel in 2..MAX_AUDIO_CHANNELS {
        assert_db(a.magnitude[channel], f32::NEG_INFINITY);
        assert_db(a.peak[channel], f32::NEG_INFINITY);
    }

    // The volume signal updates dB and does not emit levels.
    set_meter_volume(&cubic, &cubic_wire, 1, 0.5);
    set_meter_volume(&logf, &log_wire, 1, 0.5);
    assert_eq!(a.calls, 1);

    feed(&cubic, &cubic_wire, 1, &packet);
    feed(&logf, &log_wire, 1, &packet);
    assert_eq!(a.calls, 2);
    assert_db(a.magnitude[0], mul_to_db(0.25));
    assert_db(a.peak[0], mul_to_db(0.25));
    // Input peak is the sample peak, before volume.
    assert_db(a.input_peak[0], mul_to_db(0.5));
    assert_db(a.input_peak[1], mul_to_db(0.25));

    cubic.remove_callback(meter_cb(), &mut a as *mut MeterNote as usize);
    feed(&cubic, &cubic_wire, 1, &packet);
    feed(&logf, &log_wire, 1, &packet);
    assert_eq!(a.calls, 2);
    assert_eq!(b.calls, 3);

    // The destroy signal detaches. The channel count is then 0.
    cubic.on_destroy();
    assert_eq!(cubic.nr_channels(mix), 0);
}

/// `mute_and_disable_test`. Characterized, not endorsed: user mute does not
/// zero the meter. The meter zeros only when the capture muted flag and
/// `obs_source_muted` disagree, which a disabled source does.
#[test]
fn mute_and_disable_test() {
    let meter = Volmeter::create(FADER_CUBIC);
    let mut wire = Wire { source: None };
    let mut note = MeterNote {
        id: 0,
        calls: 0,
        log: std::ptr::null_mut(),
        magnitude: [0.0; MAX_AUDIO_CHANNELS],
        peak: [0.0; MAX_AUDIO_CHANNELS],
        input_peak: [0.0; MAX_AUDIO_CHANNELS],
    };

    attach_meter(&meter, &mut wire, 1, 1.0);
    meter.add_callback(meter_cb(), &mut note as *mut MeterNote as usize);

    let (left, right) = tone(0.5, 0.5, TONE_FRAMES);
    // User mute: both flags set. The meter keeps the samples.
    let muted = stereo(&left, &right, SpeakerLayout::Stereo, true, true);
    feed(&meter, &wire, 1, &muted);
    assert_eq!(note.calls, 1);
    assert_db(note.magnitude[0], mul_to_db(0.5));

    // Disabled: capture muted, user mute clear. Magnitude and peak go silent.
    let disabled = stereo(&left, &right, SpeakerLayout::Stereo, true, false);
    feed(&meter, &wire, 1, &disabled);
    assert_eq!(note.calls, 2);
    assert_db(note.magnitude[0], f32::NEG_INFINITY);
    assert_db(note.peak[0], f32::NEG_INFINITY);
    assert_db(note.input_peak[0], mul_to_db(0.5));
}

/// `short_packet_peak_test`. Characterized, not endorsed: a packet shorter
/// than 4 samples does not enter the peak loop. Magnitude still uses every
/// sample. Those samples then sit in the peak history.
#[test]
fn short_packet_peak_test() {
    let meter = Volmeter::create(FADER_CUBIC);
    let mut wire = Wire { source: None };
    let mut note = MeterNote {
        id: 0,
        calls: 0,
        log: std::ptr::null_mut(),
        magnitude: [0.0; MAX_AUDIO_CHANNELS],
        peak: [0.0; MAX_AUDIO_CHANNELS],
        input_peak: [0.0; MAX_AUDIO_CHANNELS],
    };

    attach_meter(&meter, &mut wire, 1, 1.0);
    meter.add_callback(meter_cb(), &mut note as *mut MeterNote as usize);

    let (left, right) = tone(1.0, 1.0, 2);
    let packet = stereo(&left, &right, SpeakerLayout::Stereo, false, false);
    feed(&meter, &wire, 1, &packet);
    assert_eq!(note.calls, 1);
    assert_db(note.magnitude[0], 0.0);
    assert_db(note.peak[0], f32::NEG_INFINITY);
    assert_db(note.input_peak[0], f32::NEG_INFINITY);

    let (zleft, zright) = tone(0.0, 0.0, TONE_FRAMES);
    let silence = stereo(&zleft, &zright, SpeakerLayout::Stereo, false, false);
    feed(&meter, &wire, 1, &silence);
    assert_eq!(note.calls, 2);
    assert_db(note.magnitude[0], f32::NEG_INFINITY);
    assert_db(note.peak[0], 0.0);
    assert_db(note.input_peak[0], 0.0);

    // The silence packet replaces that history.
    feed(&meter, &wire, 1, &silence);
    assert_eq!(note.calls, 3);
    assert_db(note.peak[0], f32::NEG_INFINITY);
    assert_db(note.input_peak[0], f32::NEG_INFINITY);
}

/// `sample_and_true_peak_test`.
#[test]
fn sample_and_true_peak_test() {
    let sample = Volmeter::create(FADER_CUBIC);
    let truth = Volmeter::create(FADER_CUBIC);
    let mut sample_wire = Wire { source: None };
    let mut true_wire = Wire { source: None };
    let mut sample_note = MeterNote {
        id: 0,
        calls: 0,
        log: std::ptr::null_mut(),
        magnitude: [0.0; MAX_AUDIO_CHANNELS],
        peak: [0.0; MAX_AUDIO_CHANNELS],
        input_peak: [0.0; MAX_AUDIO_CHANNELS],
    };
    let mut true_note = MeterNote {
        id: 0,
        calls: 0,
        log: std::ptr::null_mut(),
        magnitude: [0.0; MAX_AUDIO_CHANNELS],
        peak: [0.0; MAX_AUDIO_CHANNELS],
        input_peak: [0.0; MAX_AUDIO_CHANNELS],
    };

    sample.set_peak_meter_type(SAMPLE_PEAK_METER);
    truth.set_peak_meter_type(TRUE_PEAK_METER);
    attach_meter(&sample, &mut sample_wire, 1, 1.0);
    attach_meter(&truth, &mut true_wire, 1, 1.0);
    sample.add_callback(meter_cb(), &mut sample_note as *mut MeterNote as usize);
    truth.add_callback(meter_cb(), &mut true_note as *mut MeterNote as usize);

    let left = [0.0, 1.0, 0.0, -1.0, 0.0, 1.0, 0.0, -1.0];
    let right = [0.0; 8];
    let packet = stereo(&left, &right, SpeakerLayout::Stereo, false, false);
    feed(&sample, &sample_wire, 1, &packet);
    feed(&truth, &true_wire, 1, &packet);

    assert_eq!(sample_note.calls, 1);
    assert_eq!(true_note.calls, 1);
    assert_db(sample_note.peak[0], 0.0);
    assert_db(sample_note.input_peak[0], 0.0);
    assert_db(sample_note.peak[1], f32::NEG_INFINITY);
    // Magnitude is RMS. Four of the eight samples are ±1, so this is about -3.0103 dB.
    assert_db(sample_note.magnitude[0], mul_to_db(0.5f32.sqrt()));
    assert_db(true_note.magnitude[0], mul_to_db(0.5f32.sqrt()));
    assert_db(sample_note.magnitude[1], f32::NEG_INFINITY);
    // Characterized, not endorsed: this buffer overshoots 0 dB.
    // The C meter reported 0x1.57fbf4p-2f (bits 0x3eabfdfa).
    let true_peak = f32::from_bits(0x3eab_fdfa);
    assert_db(true_note.peak[0], true_peak);
    assert_db(true_note.input_peak[0], true_peak);
    assert_db(true_note.peak[1], f32::NEG_INFINITY);
}

/// `meter_callback_order_test`. Last callback added runs first.
#[test]
fn meter_callback_order_test() {
    let meter = Volmeter::create(FADER_CUBIC);
    let mut wire = Wire { source: None };
    let mut log = CallLog { seq: [0; 8], n: 0 };
    let mut a = MeterNote {
        id: 1,
        calls: 0,
        log: &mut log,
        magnitude: [0.0; MAX_AUDIO_CHANNELS],
        peak: [0.0; MAX_AUDIO_CHANNELS],
        input_peak: [0.0; MAX_AUDIO_CHANNELS],
    };
    let mut b = MeterNote {
        id: 2,
        calls: 0,
        log: &mut log,
        magnitude: [0.0; MAX_AUDIO_CHANNELS],
        peak: [0.0; MAX_AUDIO_CHANNELS],
        input_peak: [0.0; MAX_AUDIO_CHANNELS],
    };

    attach_meter(&meter, &mut wire, 1, 1.0);
    meter.add_callback(meter_cb(), &mut a as *mut MeterNote as usize);
    meter.add_callback(meter_cb(), &mut b as *mut MeterNote as usize);

    let (left, right) = tone(0.5, 0.5, TONE_FRAMES);
    let packet = stereo(&left, &right, SpeakerLayout::Stereo, false, false);
    feed(&meter, &wire, 1, &packet);

    assert_eq!(log.n, 2);
    assert_eq!(log.seq[0], 2);
    assert_eq!(log.seq[1], 1);
}

/// `meter_reattach_and_detach_test`.
#[test]
fn meter_reattach_and_detach_test() {
    let meter = Volmeter::create(FADER_CUBIC);
    let mut wire = Wire { source: None };
    let mut note = MeterNote {
        id: 0,
        calls: 0,
        log: std::ptr::null_mut(),
        magnitude: [0.0; MAX_AUDIO_CHANNELS],
        peak: [0.0; MAX_AUDIO_CHANNELS],
        input_peak: [0.0; MAX_AUDIO_CHANNELS],
    };

    attach_meter(&meter, &mut wire, 1, 1.0);
    meter.add_callback(meter_cb(), &mut note as *mut MeterNote as usize);
    set_meter_volume(&meter, &wire, 1, 0.5);

    let (left, right) = tone(1.0, 1.0, TONE_FRAMES);
    let packet = stereo(&left, &right, SpeakerLayout::Stereo, false, false);
    feed(&meter, &wire, 1, &packet);
    assert_eq!(note.calls, 1);
    assert_db(note.magnitude[0], mul_to_db(0.5));

    attach_meter(&meter, &mut wire, 2, 1.0);
    // The first source is no longer registered.
    feed(&meter, &wire, 1, &packet);
    assert_eq!(note.calls, 1);

    feed(&meter, &wire, 2, &packet);
    assert_eq!(note.calls, 2);
    // Attach copies the new source's volume, which is unity.
    assert_db(note.magnitude[0], 0.0);

    detach_meter(&meter, &mut wire);
    feed(&meter, &wire, 2, &packet);
    assert_eq!(note.calls, 2);
}

/// `channel_count_clamps_test`. 7.1 into a stereo mix is clamped to the mix.
#[test]
fn channel_count_clamps_test() {
    let meter = Volmeter::create(FADER_CUBIC);
    let mut wire = Wire { source: None };
    attach_meter(&meter, &mut wire, 1, 1.0);

    let planes = [[0.2f32; 8]; 8];
    let mut packet_planes = [None; MAX_AUDIO_CHANNELS];
    for i in 0..MAX_AUDIO_CHANNELS {
        packet_planes[i] = Some(&planes[i][..]);
    }
    let packet = AudioPacket {
        planes: packet_planes,
        frames: 8,
        speakers: speakers(SpeakerLayout::SevenPointOne),
        aligned: [true; MAX_AUDIO_CHANNELS],
        capture_muted: false,
        user_muted: false,
    };
    feed(&meter, &wire, 1, &packet);
    assert_eq!(meter.nr_channels(Some(speakers(SpeakerLayout::Stereo))), 2);

    // obs_get_audio_info failing caps the same source at 2.
    assert_eq!(meter.nr_channels(None), 2);
}

/// Edge beyond the cmocka file: `get_audio_channels` for each layout, read
/// back through the meter with a mix wide enough not to clamp.
#[test]
fn speaker_channel_map_test() {
    let meter = Volmeter::create(FADER_CUBIC);
    let mut wire = Wire { source: None };
    let wide = Some(speakers(SpeakerLayout::SevenPointOne));
    let layouts = [
        (SpeakerLayout::Unknown, 0),
        (SpeakerLayout::Mono, 1),
        (SpeakerLayout::Stereo, 2),
        (SpeakerLayout::TwoPointOne, 3),
        (SpeakerLayout::FourPointZero, 4),
        (SpeakerLayout::FourPointOne, 5),
        (SpeakerLayout::FivePointOne, 6),
        (SpeakerLayout::SevenPointOne, 8),
    ];
    let buf = [0.0f32; 4];

    for (layout, want) in layouts {
        attach_meter(&meter, &mut wire, 1, 1.0);
        let mut planes = [None; MAX_AUDIO_CHANNELS];
        planes[0] = Some(&buf[..]);
        let packet = AudioPacket {
            planes,
            frames: buf.len(),
            speakers: speakers(layout),
            aligned: [true; MAX_AUDIO_CHANNELS],
            capture_muted: false,
            user_muted: false,
        };
        feed(&meter, &wire, 1, &packet);
        assert_eq!(meter.nr_channels(wide), want, "{layout:?}");
    }

    detach_meter(&meter, &mut wire);
    assert_eq!(meter.nr_channels(wide), 0);
}

/// The unaligned plane path is not reached through `obs_source_output_audio`.
/// The copy the source keeps is aligned. The branch still forces peak to 1.
#[test]
fn unaligned_plane_peak_test() {
    let meter = Volmeter::create(FADER_CUBIC);
    let mut wire = Wire { source: None };
    let mut note = MeterNote {
        id: 0,
        calls: 0,
        log: std::ptr::null_mut(),
        magnitude: [0.0; MAX_AUDIO_CHANNELS],
        peak: [0.0; MAX_AUDIO_CHANNELS],
        input_peak: [0.0; MAX_AUDIO_CHANNELS],
    };

    attach_meter(&meter, &mut wire, 1, 1.0);
    meter.add_callback(meter_cb(), &mut note as *mut MeterNote as usize);

    let (left, right) = tone(0.5, 0.25, TONE_FRAMES);
    let mut packet = stereo(&left, &right, SpeakerLayout::Stereo, false, false);
    packet.aligned = [false; MAX_AUDIO_CHANNELS];
    feed(&meter, &wire, 1, &packet);

    assert_eq!(note.calls, 1);
    assert_db(note.magnitude[0], mul_to_db(0.5));
    assert_db(note.magnitude[1], mul_to_db(0.25));
    assert_db(note.peak[0], 0.0);
    assert_db(note.input_peak[0], 0.0);
    assert_db(note.peak[1], 0.0);
}
