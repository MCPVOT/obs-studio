//! Tier 1 for obs_output::service (#1899): the cases of
//! test/cmocka/test_service_getters.c, against a callback table in Rust.

use core::cell::{Cell, RefCell};
use core::ffi::CStr;

use obs_output::service::{self, Callbacks, NullLog};

#[derive(Default)]
struct Log(RefCell<Vec<(String, String)>>);

impl NullLog for Log {
    fn null_param(&self, func: &'static CStr, param: &'static CStr) {
        self.0.borrow_mut().push((
            func.to_string_lossy().into_owned(),
            param.to_string_lossy().into_owned(),
        ));
    }
}

impl Log {
    fn lines(&self) -> Vec<(String, String)> {
        self.0.borrow().clone()
    }
}

fn line(func: &str, param: &str) -> (String, String) {
    (func.to_owned(), param.to_owned())
}

/// "full" has every callback, "bare" only the required protocol.
struct Svc {
    full: bool,
    can_connect: bool,
    calls: Cell<u32>,
    last_kind: Cell<u32>,
}

impl Svc {
    fn full() -> Self {
        Svc {
            full: true,
            can_connect: true,
            calls: Cell::new(0),
            last_kind: Cell::new(0),
        }
    }
    fn bare() -> Self {
        Svc {
            full: false,
            ..Svc::full()
        }
    }
    fn seen(&self) {
        self.calls.set(self.calls.get() + 1);
    }
}

const VIDEO: &[&str] = &["h264", "hevc"];
const AUDIO: &[&str] = &["aac"];

impl Callbacks for Svc {
    type Str = &'static str;
    type List = &'static [&'static str];
    type Resolutions = Option<Vec<(u32, u32)>>;

    fn protocol(&self) -> &'static str {
        self.seen();
        if self.full { "SRT" } else { "RTMP" }
    }
    fn connect_info(&self, kind: u32) -> Option<&'static str> {
        if !self.full {
            return None;
        }
        self.seen();
        self.last_kind.set(kind);
        Some(if kind == 0 {
            "srt://example.invalid:9000"
        } else {
            ""
        })
    }
    fn can_try_to_connect(&self) -> Option<bool> {
        self.full.then(|| {
            self.seen();
            self.can_connect
        })
    }
    fn output_type(&self) -> Option<&'static str> {
        self.full.then(|| {
            self.seen();
            "ffmpeg_mpegts_muxer"
        })
    }
    fn max_fps(&self, fps: &mut i32) -> bool {
        if self.full {
            self.seen();
            *fps = 60;
        }
        self.full
    }
    fn max_bitrate(&self, video: Option<&mut i32>, _audio: Option<&mut i32>) -> bool {
        if self.full {
            self.seen();
            if let Some(video) = video {
                *video = 6000;
            }
        }
        self.full
    }
    fn resolutions(&self, res: &mut Self::Resolutions, count: &mut usize) -> bool {
        if self.full {
            self.seen();
            *res = Some(vec![(1920, 1080), (1280, 720)]);
            *count = 2;
        }
        self.full
    }
    fn video_codecs(&self) -> Option<Self::List> {
        self.full.then(|| {
            self.seen();
            VIDEO
        })
    }
    fn audio_codecs(&self) -> Option<Self::List> {
        self.full.then(|| {
            self.seen();
            AUDIO
        })
    }
}

#[test]
fn protocol() {
    let (full, bare, log) = (Svc::full(), Svc::bare(), Log::default());
    assert_eq!(service::protocol(Some(&full), &log), Some("SRT"));
    assert_eq!(service::protocol(Some(&bare), &log), Some("RTMP"));
    assert_eq!(service::protocol::<Svc>(None, &log), None);
    assert_eq!(log.lines(), [line("obs_service_get_protocol", "service")]);
}

#[test]
fn connect_info() {
    let (full, bare, log) = (Svc::full(), Svc::bare(), Log::default());
    assert_eq!(
        service::connect_info(Some(&full), &log, 0),
        Some("srt://example.invalid:9000")
    );
    assert_eq!(full.last_kind.get(), 0);
    // Any kind reaches the plugin unchanged.
    service::connect_info(Some(&full), &log, 0xffff_fff0);
    assert_eq!(full.last_kind.get(), 0xffff_fff0);
    assert_eq!(service::connect_info(Some(&bare), &log, 0), None);
    assert_eq!(service::connect_info::<Svc>(None, &log, 0), None);
    assert_eq!(full.calls.get(), 2);
    // The C's function name for this check is "obs_service_get_info".
    assert_eq!(log.lines(), [line("obs_service_get_info", "service")]);
}

#[test]
fn can_try_to_connect() {
    let (bare, log) = (Svc::bare(), Log::default());
    let mut full = Svc::full();
    assert!(service::can_try_to_connect(Some(&full), &log));
    full.can_connect = false;
    assert!(!service::can_try_to_connect(Some(&full), &log));
    // No callback: the service can always try.
    assert!(service::can_try_to_connect(Some(&bare), &log));
    assert!(!service::can_try_to_connect::<Svc>(None, &log));
    assert_eq!(full.calls.get(), 2);
    assert_eq!(log.lines(), [line("obs_service_can_connect", "service")]);
}

#[test]
fn preferred_output_type() {
    let (full, bare, log) = (Svc::full(), Svc::bare(), Log::default());
    assert_eq!(
        service::preferred_output_type(Some(&full), &log),
        Some("ffmpeg_mpegts_muxer")
    );
    assert_eq!(service::preferred_output_type(Some(&bare), &log), None);
    assert_eq!(service::preferred_output_type::<Svc>(None, &log), None);
    assert_eq!(full.calls.get(), 1);
    assert_eq!(
        log.lines(),
        [line("obs_service_get_preferred_output_type", "service")]
    );
}

#[test]
fn max_fps() {
    let (full, bare, log) = (Svc::full(), Svc::bare(), Log::default());
    let mut fps = 7;
    service::max_fps(Some(&full), &log, Some(&mut fps));
    assert_eq!(fps, 60);
    fps = 7;
    service::max_fps(Some(&bare), &log, Some(&mut fps));
    assert_eq!(fps, 0);
    // A null service returns before zeroing.
    fps = 7;
    service::max_fps::<Svc>(None, &log, Some(&mut fps));
    assert_eq!(fps, 7);
    service::max_fps(Some(&full), &log, None);
    assert_eq!(full.calls.get(), 1);
    assert_eq!(
        log.lines(),
        [
            line("obs_service_get_max_fps", "service"),
            line("obs_service_get_max_fps", "fps")
        ]
    );
}

#[test]
fn max_bitrate() {
    let (full, bare, log) = (Svc::full(), Svc::bare(), Log::default());
    let (mut video, mut audio) = (7, 7);
    service::max_bitrate(Some(&full), &log, Some(&mut video), Some(&mut audio));
    assert_eq!((video, audio), (6000, 0));
    (video, audio) = (7, 7);
    service::max_bitrate(Some(&bare), &log, Some(&mut video), Some(&mut audio));
    assert_eq!((video, audio), (0, 0));
    // Characterized, not endorsed: zeroed before the service is checked.
    (video, audio) = (7, 7);
    service::max_bitrate::<Svc>(None, &log, Some(&mut video), Some(&mut audio));
    assert_eq!((video, audio), (0, 0));
    video = 7;
    service::max_bitrate(Some(&full), &log, Some(&mut video), None);
    assert_eq!(video, 6000);
    service::max_bitrate(Some(&full), &log, None, None);
    assert_eq!(full.calls.get(), 3);
    assert_eq!(
        log.lines(),
        [line("obs_service_get_max_bitrate", "service")]
    );
}

#[test]
fn supported_resolutions() {
    let (full, bare, log) = (Svc::full(), Svc::bare(), Log::default());
    let stale = Some(vec![(1, 1)]);
    let (mut res, mut count) = (stale.clone(), 7);
    service::supported_resolutions(Some(&full), &log, Some(&mut res), Some(&mut count), None);
    assert_eq!((res, count), (Some(vec![(1920, 1080), (1280, 720)]), 2));

    let (mut res, mut count) = (stale.clone(), 7);
    service::supported_resolutions(Some(&bare), &log, Some(&mut res), Some(&mut count), None);
    assert_eq!((res, count), (None, 0));

    // A null service or out-parameter returns before anything is written.
    let (mut res, mut count) = (stale.clone(), 7);
    service::supported_resolutions::<Svc>(None, &log, Some(&mut res), Some(&mut count), None);
    assert_eq!((&res, count), (&stale, 7));
    service::supported_resolutions(Some(&full), &log, None, Some(&mut count), None);
    assert_eq!(count, 7);
    service::supported_resolutions(Some(&full), &log, Some(&mut res), None, None);
    assert_eq!(res, stale);
    assert_eq!(full.calls.get(), 1);
    let f = "obs_service_supported_resolutions";
    assert_eq!(
        log.lines(),
        [line(f, "service"), line(f, "resolutions"), line(f, "count")]
    );
}

#[test]
fn supported_codecs() {
    let (full, bare) = (Svc::full(), Svc::bare());
    assert_eq!(service::video_codecs(&full), Some(VIDEO));
    assert_eq!(service::audio_codecs(&full), Some(AUDIO));
    assert_eq!(service::video_codecs(&bare), None);
    assert_eq!(service::audio_codecs(&bare), None);
    assert_eq!(full.calls.get(), 2);
}
