#[path = "../build-options.rs"]
mod build_options;

#[test]
fn bundled_native_build_modes_are_independent_and_validate_inputs() {
    use build_options::BuildModes;
    for (unity, pch, expected) in [
        (None, None, (true, false)),
        (Some("0"), None, (false, false)),
        (None, Some("1"), (true, true)),
        (Some("0"), Some("1"), (false, true)),
        (Some("1"), Some("0"), (true, false)),
    ] {
        let modes = BuildModes::from_lookup(|name| match name {
            "OBS_SYS_UNITY" => unity.map(Into::into),
            "OBS_SYS_PCH" => pch.map(Into::into),
            _ => panic!("unexpected native build variable: {name}"),
        })
        .unwrap();
        assert_eq!((modes.unity, modes.pch), expected);
    }
    for variable in build_options::VARIABLES {
        for value in ["", "yes", "2", "-1"] {
            let error = BuildModes::from_lookup(|name| {
                (name == variable).then(|| std::ffi::OsString::from(value))
            })
            .unwrap_err();
            assert!(error.contains(variable), "{error}");
            assert!(error.contains("0 or 1"), "{error}");
        }
    }
}

#[test]
fn bundled_native_build_uses_requested_modes() {
    let modes = build_options::BuildModes::from_lookup(|name| std::env::var_os(name)).unwrap();
    assert_eq!(
        obs_sys::NATIVE_UNITY,
        Some(if modes.unity { "1" } else { "0" })
    );
    assert_eq!(obs_sys::NATIVE_PCH, Some(if modes.pch { "1" } else { "0" }));
}

#[test]
fn bundled_libobs_reports_the_public_c_api_version() {
    // SAFETY: the bundled build links the original no-argument C API function.
    let version = unsafe { obs_sys::ffi::obs_get_version() };
    assert_eq!(version >> 24, 33);
    assert!(
        std::path::Path::new(obs_sys::BUNDLED_LIBRARY.expect("bundled library path")).is_file()
    );
}

#[test]
fn bundled_libobs_starts_and_shuts_down_without_a_frontend() {
    // SAFETY: the locale is a NUL-terminated static string. Null optional paths
    // and profiler store are explicitly supported by obs_startup in obs.h.
    // This is the only test that owns the process-global OBS lifecycle.
    unsafe {
        assert!(!obs_sys::ffi::obs_initialized());
        assert!(obs_sys::ffi::obs_startup(
            c"en-US".as_ptr(),
            std::ptr::null(),
            std::ptr::null_mut(),
        ));
        assert!(obs_sys::ffi::obs_initialized());
        obs_sys::ffi::obs_shutdown();
        assert!(!obs_sys::ffi::obs_initialized());
    }
}
