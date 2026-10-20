//! `obs_volmeter_t`. Opaque in the public header.

use std::sync::Mutex;

use obs_media_io::audio_io::{audio_channels, speaker_layout_from_c};

use crate::peak::{sample_peak, store_last, true_peak};
use crate::{MAX_AUDIO_CHANNELS, TRUE_PEAK_METER, db_to_mul, mul_to_db};

/// `obs_volmeter_updated_t`, with the `void *` param carried as `usize`.
///
/// The three arrays are magnitude, peak, and input peak, in dB, one slot
/// per channel up to [`MAX_AUDIO_CHANNELS`].
pub type MeterCb = fn(
    param: usize,
    magnitude: &[f32; MAX_AUDIO_CHANNELS],
    peak: &[f32; MAX_AUDIO_CHANNELS],
    input_peak: &[f32; MAX_AUDIO_CHANNELS],
);

/// One capture callback's view of `audio_data`, plus the two mute flags
/// the C callback closes over.
///
/// `aligned[i] == false` is the unaligned-plane path: that plane's peak is
/// forced to 1 and its peak history is left alone. Magnitude still runs.
/// The C `printf` for that path stays in the shim.
pub struct AudioPacket<'a> {
    pub planes: [Option<&'a [f32]>; MAX_AUDIO_CHANNELS],
    pub frames: usize,
    pub speakers: i32,
    pub aligned: [bool; MAX_AUDIO_CHANNELS],
    pub capture_muted: bool,
    pub user_muted: bool,
}

struct State {
    cur_db: f32,
    attached: bool,
    /// `source->sample_info.speakers`. Unknown (0) until the first packet.
    speakers: i32,
    peak_type: i32,
    prev: [[f32; 4]; MAX_AUDIO_CHANNELS],
    magnitude: [f32; MAX_AUDIO_CHANNELS],
    peak: [f32; MAX_AUDIO_CHANNELS],
    /// Characterized, not endorsed: stored at create and never read.
    #[allow(dead_code)]
    fader_type: i32,
}

/// Level meter. The fader type is stored and never read.
pub struct Volmeter {
    state: Mutex<State>,
    callbacks: Mutex<Vec<(MeterCb, usize)>>,
}

impl Volmeter {
    /// `obs_volmeter_create`. Any type, including an unknown one, creates.
    /// The peak meter starts as `SAMPLE_PEAK_METER` (the zeroed C field).
    pub fn create(ty: i32) -> Self {
        Self {
            state: Mutex::new(State {
                cur_db: 0.0,
                attached: false,
                speakers: 0,
                peak_type: 0,
                prev: [[0.0; 4]; MAX_AUDIO_CHANNELS],
                magnitude: [0.0; MAX_AUDIO_CHANNELS],
                peak: [0.0; MAX_AUDIO_CHANNELS],
                fader_type: ty,
            }),
            callbacks: Mutex::new(Vec::new()),
        }
    }

    /// `obs_volmeter_attach_source`. Copies `volume` into the current dB
    /// and forgets the previous source's speaker layout. Peak history stays,
    /// as it does in C.
    pub fn attach_volume(&self, volume: f32) {
        self.detach();
        let mut state = self.lock();
        state.attached = true;
        state.cur_db = mul_to_db(volume);
        state.speakers = 0;
    }

    /// `obs_volmeter_detach_source`. Channel count then reads as 0.
    pub fn detach(&self) {
        self.lock().attached = false;
    }

    /// The meter's `volume` signal handler. Updates the current dB only.
    /// Level callbacks are not run. Disconnect is the caller's job.
    pub fn on_volume(&self, mul: f32) {
        self.lock().cur_db = mul_to_db(mul);
    }

    /// The meter's `destroy` signal handler. Detaches.
    pub fn on_destroy(&self) {
        self.detach();
    }

    /// `obs_volmeter_set_peak_meter_type`. No null check in C.
    pub fn set_peak_meter_type(&self, ty: i32) {
        self.lock().peak_type = ty;
    }

    /// Store `source->sample_info.speakers` for [`Self::nr_channels`].
    ///
    /// C reads that field at the call, not a copy from the last packet.
    /// Ignored while nothing is attached, which is the branch that
    /// reports 0 channels.
    pub(crate) fn note_speakers(&self, speakers: i32) {
        let mut state = self.lock();
        if state.attached {
            state.speakers = speakers;
        }
    }

    /// `obs_volmeter_get_nr_channels`.
    ///
    /// Speakers stay unknown until the first packet, so this is 0 after
    /// attach and before any audio. `mix_speakers` is the obs mix layout.
    /// `None` is `obs_get_audio_info` failing, which caps the count at 2.
    /// The count is clamped to that mix. After detach it is 0.
    pub fn nr_channels(&self, mix_speakers: Option<i32>) -> i32 {
        let state = self.lock();
        let source_ch = if state.attached {
            layout_channels(state.speakers)
        } else {
            0
        };
        let cap = match mix_speakers {
            Some(layout) => layout_channels(layout),
            None => 2,
        };
        source_ch.clamp(0, cap)
    }

    /// The capture callback: peaks, RMS magnitude, then level callbacks.
    ///
    /// `mul` is 0 when `capture_muted && !user_muted` (a disabled source).
    /// User mute sets both flags, so the meter is not zeroed. Input peak
    /// ignores that multiplier. Characterized, not endorsed.
    pub fn on_data(&self, packet: &AudioPacket<'_>) {
        let (magnitude, peak, input_peak) = {
            let mut state = self.lock();
            state.speakers = packet.speakers;
            let nr = plane_count(packet);
            process_peak(&mut state, packet, nr);
            process_magnitude(&mut state, packet, nr);
            let mul = if packet.capture_muted && !packet.user_muted {
                0.0
            } else {
                db_to_mul(state.cur_db)
            };
            let mut magnitude = [0.0; MAX_AUDIO_CHANNELS];
            let mut peak = [0.0; MAX_AUDIO_CHANNELS];
            let mut input_peak = [0.0; MAX_AUDIO_CHANNELS];
            for channel in 0..MAX_AUDIO_CHANNELS {
                magnitude[channel] = mul_to_db(state.magnitude[channel] * mul);
                peak[channel] = mul_to_db(state.peak[channel] * mul);
                input_peak[channel] = mul_to_db(state.peak[channel]);
            }
            (magnitude, peak, input_peak)
        };
        self.signal(&magnitude, &peak, &input_peak);
    }

    /// `obs_volmeter_add_callback`.
    pub fn add_callback(&self, cb: MeterCb, param: usize) {
        self.callbacks
            .lock()
            .expect("meter callbacks")
            .push((cb, param));
    }

    /// `obs_volmeter_remove_callback`. One matching `(cb, param)` pair.
    pub fn remove_callback(&self, cb: MeterCb, param: usize) {
        let mut callbacks = self.callbacks.lock().expect("meter callbacks");
        if let Some(index) = callbacks.iter().position(|&(existing, existing_param)| {
            std::ptr::fn_addr_eq(existing, cb) && existing_param == param
        }) {
            callbacks.remove(index);
        }
    }

    fn lock(&self) -> std::sync::MutexGuard<'_, State> {
        self.state.lock().expect("volmeter")
    }

    fn signal(
        &self,
        magnitude: &[f32; MAX_AUDIO_CHANNELS],
        peak: &[f32; MAX_AUDIO_CHANNELS],
        input_peak: &[f32; MAX_AUDIO_CHANNELS],
    ) {
        let callbacks = self.callbacks.lock().expect("meter callbacks");
        for &(cb, param) in callbacks.iter().rev() {
            cb(param, magnitude, peak, input_peak);
        }
    }
}

fn layout_channels(speakers: i32) -> i32 {
    match speaker_layout_from_c(speakers) {
        Some(layout) => audio_channels(layout) as i32,
        None => 0,
    }
}

fn plane_count(packet: &AudioPacket<'_>) -> usize {
    packet
        .planes
        .iter()
        .filter(|plane| plane.is_some())
        .count()
        .min(MAX_AUDIO_CHANNELS)
}

fn process_peak(state: &mut State, packet: &AudioPacket<'_>, nr_channels: usize) {
    let mut channel = 0;
    let mut plane_nr = 0;
    while channel < nr_channels && plane_nr < MAX_AUDIO_CHANNELS {
        let Some(samples) = packet.planes[plane_nr] else {
            plane_nr += 1;
            continue;
        };
        if !packet.aligned[plane_nr] {
            state.peak[channel] = 1.0;
            channel += 1;
            plane_nr += 1;
            continue;
        }
        let frames = packet.frames;
        let prev = state.prev[channel];
        let peak = if state.peak_type == TRUE_PEAK_METER {
            true_peak(prev, samples, frames)
        } else {
            sample_peak(prev, samples, frames)
        };
        store_last(&mut state.prev[channel], samples, frames);
        state.peak[channel] = peak;
        channel += 1;
        plane_nr += 1;
    }
    while channel < MAX_AUDIO_CHANNELS {
        state.peak[channel] = 0.0;
        channel += 1;
    }
}

fn process_magnitude(state: &mut State, packet: &AudioPacket<'_>, nr_channels: usize) {
    let mut channel = 0;
    let mut plane_nr = 0;
    while channel < nr_channels && plane_nr < MAX_AUDIO_CHANNELS {
        let Some(samples) = packet.planes[plane_nr] else {
            plane_nr += 1;
            continue;
        };
        let mut sum = 0.0f32;
        for sample in &samples[..packet.frames] {
            sum += sample * sample;
        }
        state.magnitude[channel] = (sum / packet.frames as f32).sqrt();
        channel += 1;
        plane_nr += 1;
    }
}
