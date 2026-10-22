//! Build-system integration contracts; these compile and load actual C artifacts.
use obs_build::{NativeBuild, SharedLibrary};
use std::{fs, path::Path};

fn target(name: &str, source: &Path, dependencies: Vec<std::path::PathBuf>) -> SharedLibrary {
    SharedLibrary {
        name: name.into(),
        sources: vec![source.into()],
        include_dirs: vec![],
        dependencies,
        unity: true,
        ..SharedLibrary::default()
    }
}

unsafe fn answer(library: &Path, symbol: &[u8], dependencies: &[&Path]) -> i32 {
    // SAFETY: the fixture below defines this symbol with the C ABI and no args;
    // its owning library stays loaded for the entire call.
    unsafe {
        // Load dependencies explicitly as well: Windows does not search sibling
        // target directories for DLLs. Keep their handles alive through the call.
        let _dependencies: Vec<_> = dependencies
            .iter()
            .map(|path| libloading::Library::new(path).unwrap())
            .collect();
        let lib = libloading::Library::new(library).unwrap();
        let function = lib.get::<unsafe extern "C" fn() -> i32>(symbol).unwrap();
        function()
    }
}

#[test]
fn native_profile_switch_rebuilds_assertion_policy() {
    if native_answer_probe() {
        return;
    }
    let selector = format!(
        "{}::native_profile_switch_rebuilds_assertion_policy",
        module_path!().split_once("::").unwrap().1
    );
    let directory = tempfile::tempdir().unwrap();
    let source = directory.path().join("profile.c");
    fs::write(
        &source,
        format!(
            "{EXPORT} int obs_fixture_profile(void) {{\n#ifdef NDEBUG\nreturn 1;\n#else\nreturn 0;\n#endif\n}}\n"
        ),
    )
    .unwrap();
    let build = NativeBuild {
        profile: Default::default(),
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: directory.path().join("artifacts"),
    };
    use obs_build::NativeProfile::{Debug, MinSizeRel, RelWithDebInfo, Release};
    for (profile, expected) in [
        (Debug, 0),
        (Release, 1),
        (RelWithDebInfo, 1),
        (MinSizeRel, 1),
        (Debug, 0),
    ] {
        let artifact = build
            .with_profile(profile)
            .shared_library(&target("fixture-profile", &source, vec![]))
            .unwrap();
        assert_native_answer_fresh(&artifact, "obs_fixture_profile", expected, &selector);
    }
}

#[test]
fn production_runtime_stages_selected_artifacts_and_rejects_stale_outputs() {
    use obs_build::{application, plugins};
    let temporary = tempfile::tempdir().unwrap();
    let root = temporary.path().join("source");
    let artifacts = temporary.path().join("artifacts");
    fs::create_dir_all(root.join("libobs/data")).unwrap();
    fs::create_dir_all(root.join("frontend/data")).unwrap();
    fs::create_dir_all(artifacts.join("module-data/locale")).unwrap();
    fs::write(root.join("libobs/data/default.effect"), "shader").unwrap();
    fs::write(root.join("frontend/data/theme.qss"), "theme").unwrap();
    fs::write(root.join("AUTHORS"), "authors").unwrap();
    for name in [
        "obs-studio-rust",
        "libobs.so",
        "libobs-frontend-api.so",
        "libobs-opengl.so",
        "fixture-source.so",
        "obs-ffmpeg.so",
        "obs-ffmpeg-mux",
    ] {
        fs::write(artifacts.join(name), name).unwrap();
    }
    fs::write(artifacts.join("module-data/locale/en-US.ini"), "translated").unwrap();
    let module = plugins::Module {
        library: artifacts.join("fixture-source.so"),
        data: artifacts.join("module-data"),
    };
    let ffmpeg = plugins::FfmpegArtifacts {
        module: plugins::Module {
            library: artifacts.join("obs-ffmpeg.so"),
            data: artifacts.join("module-data"),
        },
        muxer: artifacts.join("obs-ffmpeg-mux"),
    };
    let build = NativeBuild {
        host: obs_build::BUILD_HOST.into(),
        target: "x86_64-unknown-linux-gnu".into(),
        output: artifacts.clone(),
        profile: Default::default(),
    };
    let executable = artifacts.join("obs-studio-rust");
    let core = artifacts.join("libobs.so");
    let api = artifacts.join("libobs-frontend-api.so");
    let renderer = artifacts.join("libobs-opengl.so");
    let inputs = application::RuntimeInputs {
        root: &root,
        executable: &executable,
        core: &core,
        api: &api,
        renderer: &renderer,
        ffmpeg: &ffmpeg,
        nvenc_helper: None,
        modules: &[&module, &ffmpeg.module],
        browser: None,
        scripting: None,
    };
    let destination = temporary.path().join("runtime");
    let runtime = application::stage_runtime(&build, &inputs, &destination).unwrap();
    assert_eq!(
        fs::read_to_string(&runtime.executable).unwrap(),
        "obs-studio-rust"
    );
    assert_eq!(runtime.modules, ["fixture-source", "obs-ffmpeg"]);
    for (path, expected) in [
        ("lib64/libobs.so", "libobs.so"),
        ("bin/obs-ffmpeg-mux", "obs-ffmpeg-mux"),
        (
            "lib64/obs-modules/core/fixture-source.so",
            "fixture-source.so",
        ),
        (
            "share/obs/obs-modules/core/fixture-source/locale/en-US.ini",
            "translated",
        ),
        ("share/obs/libobs/default.effect", "shader"),
        ("share/obs/obs-studio/theme.qss", "theme"),
        ("share/obs/obs-studio/authors", "authors"),
    ] {
        assert_eq!(
            fs::read_to_string(destination.join(path)).unwrap(),
            expected
        );
    }
    fs::write(destination.join("lib64/obs-modules/core/stale.so"), "stale").unwrap();
    let error = application::stage_runtime(&build, &inputs, &destination)
        .err()
        .unwrap();
    assert!(error.contains("empty"), "{error}");
    assert_eq!(
        fs::read_to_string(destination.join("lib64/obs-modules/core/stale.so")).unwrap(),
        "stale"
    );
    let unsupported = NativeBuild {
        target: "x86_64-pc-windows-msvc".into(),
        ..build
    };
    let rejected = temporary.path().join("unsupported");
    assert!(application::stage_runtime(&unsupported, &inputs, &rejected).is_err());
    assert!(!rejected.exists());
}

#[test]
fn bundled_frontend_plugins_follow_application_feature_selection() {
    let mut options =
        obs_build::application::BuildOptions::for_target("x86_64-unknown-linux-gnu").unwrap();
    for (tools, websocket) in [(true, true), (true, false), (false, true), (false, false)] {
        options.frontend_tools = tools;
        options.websocket = websocket.then(Default::default);
        let names = options.core_module_names();
        assert_eq!(names.iter().any(|name| name == "frontend-tools"), tools);
        assert_eq!(names.iter().any(|name| name == "obs-websocket"), websocket);
    }
}

#[test]
fn production_archive_command_uses_the_native_rust_cache_safeguard() {
    if !obs_build::BUILD_TARGET.contains("linux") {
        return;
    }
    if let Some(output) = std::env::var_os("OBS_NATIVE_CACHE_PROBE_OUTPUT") {
        let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
        let result = obs_build::cli::run([
            std::ffi::OsString::from("build"),
            "--root".into(),
            root.into_os_string(),
            "--output".into(),
            output,
        ]);
        assert!(result.unwrap_err().contains("Rust archive build failed"));
        return;
    }
    let directory = tempfile::tempdir().unwrap();
    let soldr = directory.path().join("soldr");
    fs::write(&soldr, "#!/bin/sh\nprintf '%s' \"${SOLDR_RUSTC_WRAPPER:-unset}\" > \"$OBS_NATIVE_CACHE_LOG\"\nexit 19\n").unwrap();
    assert!(
        std::process::Command::new("chmod")
            .arg("+x")
            .arg(&soldr)
            .status()
            .unwrap()
            .success()
    );
    let marker = directory.path().join("wrapper");
    let path = std::env::join_paths(
        std::iter::once(directory.path().to_path_buf())
            .chain(std::env::split_paths(&std::env::var_os("PATH").unwrap())),
    )
    .unwrap();
    let selector = format!(
        "{}::production_archive_command_uses_the_native_rust_cache_safeguard",
        module_path!().split_once("::").unwrap().1
    );
    let status = std::process::Command::new(std::env::current_exe().unwrap())
        .args(["--exact", &selector, "--nocapture"])
        .env("PATH", path)
        .env_remove("SOLDR_RUSTC_WRAPPER")
        .env("OBS_NATIVE_CACHE_LOG", &marker)
        .env(
            "OBS_NATIVE_CACHE_PROBE_OUTPUT",
            directory.path().join("output"),
        )
        .status()
        .unwrap();
    assert!(status.success());
    assert_eq!(fs::read_to_string(marker).unwrap(), "none");
}

#[test]
fn production_command_selects_explicit_profile_target_and_features() {
    use obs_build::{NativeProfile, cli};
    let invocation = cli::parse([
        "build",
        "--root",
        "source tree",
        "--output",
        "build output",
        "--target",
        "x86_64-unknown-linux-gnu",
        "--profile",
        "release",
        "--separate-core",
        "--core-pch",
        "--disable-service-updates",
        "--without-websocket",
        "--without-scripting",
        "--with-vlc",
    ])
    .unwrap();
    let cli::Invocation::Build(request) = invocation else {
        panic!("build request was lost")
    };
    assert_eq!(request.root, std::path::PathBuf::from("source tree"));
    assert_eq!(request.output, std::path::PathBuf::from("build output"));
    assert_eq!(request.target, "x86_64-unknown-linux-gnu");
    assert_eq!(request.profile, NativeProfile::Release);
    assert!(!request.options.core.unity);
    assert!(request.options.core.pch);
    assert!(request.options.websocket.is_none());
    assert!(request.options.scripting.is_none());
    assert!(
        request
            .options
            .sources
            .iter()
            .any(|source| matches!(source, obs_build::plugins::SourceModule::Vlc))
    );
    assert!(request.options.sources.iter().any(|source| matches!(
        source,
        obs_build::plugins::SourceModule::RtmpServices { updates: false }
    )));
    for arguments in [
        vec!["build", "--profile", "fast"],
        vec!["build", "--target", "x86_64-pc-windows-msvc"],
        vec!["build", "--unrecognized"],
        vec!["build", "--output"],
        vec!["build", "--profile", "debug", "--profile", "release"],
    ] {
        assert!(cli::parse(arguments).is_err());
    }
    assert!(matches!(
        cli::parse(["--help"]).unwrap(),
        cli::Invocation::Help
    ));
}

#[test]
fn swift_bridge_preserves_native_linkage_header_flags_and_source_rebuilds() {
    if native_answer_probe() {
        return;
    }
    if !obs_build::BUILD_TARGET.contains("apple-darwin") {
        return;
    }
    let selector = format!(
        "{}::swift_bridge_preserves_native_linkage_header_flags_and_source_rebuilds",
        module_path!().split_once("::").unwrap().1
    );
    let dir = tempfile::tempdir().unwrap();
    let root = dir.path();
    let build = NativeBuild {
        profile: Default::default(),
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: root.join("artifacts"),
    };
    let base = root.join("base.c");
    fs::write(&base, "int base_value(void) { return 1; }\n").unwrap();
    let native = build
        .shared_library(&target("fixture-swift-base", &base, vec![]))
        .unwrap();
    let header = root.join("value.h");
    fs::write(&header, "#define HEADER_VALUE 38\n").unwrap();
    let bridge = root.join("bridge.h");
    fs::write(&bridge, "#include \"value.h\"\nstatic const int bridge_value = HEADER_VALUE;\nstatic const int build_flag = BUILD_FLAG;\nint base_value(void);\n").unwrap();
    let source = root.join("fixture.swift");
    let text = "@_cdecl(\"obs_fixture_swift\")\npublic func obs_fixture_swift() -> Int32 { return base_value() + bridge_value + build_flag }\n";
    fs::write(&source, text).unwrap();
    let mut library = obs_build::SwiftLibrary {
        name: "fixture-swift".into(),
        module_name: "NativeSwiftFixture".into(),
        file_name: "libfixture-swift.dylib".into(),
        sources: vec![source.clone()],
        bridging_header: bridge,
        input_files: vec![header.clone()],
        include_dirs: vec![root.into()],
        definitions: vec![("BUILD_FLAG".into(), Some("3".into()))],
        dependencies: vec![native],
        deployment_target: "13.0".into(),
        compile_options: vec!["-swift-version".into(), "6".into()],
        link_options: vec![],
    };
    let artifact = build.swift_shared_library(&library).unwrap();
    assert!(
        artifact
            .parent()
            .unwrap()
            .join("NativeSwiftFixture.swiftmodule")
            .is_file()
    );
    assert!(
        artifact
            .parent()
            .unwrap()
            .join("NativeSwiftFixture-Swift.h")
            .is_file()
    );
    assert_native_answer_fresh(&artifact, "obs_fixture_swift", 42, &selector);
    fs::write(&header, "#define HEADER_VALUE 48\n").unwrap();
    let artifact = build.swift_shared_library(&library).unwrap();
    assert_native_answer_fresh(&artifact, "obs_fixture_swift", 52, &selector);
    library.definitions[0].1 = Some("5".into());
    let artifact = build.swift_shared_library(&library).unwrap();
    assert_native_answer_fresh(&artifact, "obs_fixture_swift", 54, &selector);
    fs::write(&source, text.replace("+ build_flag", "+ build_flag + 2")).unwrap();
    let artifact = build.swift_shared_library(&library).unwrap();
    assert_native_answer_fresh(&artifact, "obs_fixture_swift", 56, &selector);
    fs::write(&base, "int base_value(void) { return 3; }\n").unwrap();
    build
        .shared_library(&target("fixture-swift-base", &base, vec![]))
        .unwrap();
    // The unchanged Swift dylib must observe the rebuilt shared dependency.
    assert_native_answer_fresh(&artifact, "obs_fixture_swift", 58, &selector);
}

#[test]
fn native_plugin_links_shared_core_and_observes_rebuilt_dependency() {
    let dir = tempfile::tempdir().unwrap();
    let core_source = dir.path().join("core.c");
    let plugin_source = dir.path().join("plugin.c");
    fs::write(
        &core_source,
        "__declspec_placeholder__ int obs_fixture_answer(void) { return 42; }"
            .replace("__declspec_placeholder__", EXPORT),
    )
    .unwrap();
    fs::write(&plugin_source, format!("extern int obs_fixture_answer(void); {EXPORT} int obs_fixture_plugin(void) {{ return obs_fixture_answer() + 1; }}")).unwrap();
    let build = NativeBuild {
        profile: Default::default(),
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: dir.path().join("artifacts"),
    };
    let core = build
        .shared_library(&target("fixture-core", &core_source, vec![]))
        .unwrap();
    let mut plugin_target = target("fixture-plugin", &plugin_source, vec![core.clone()]);
    plugin_target.kind = obs_build::LibraryKind::Module;
    let plugin = build.shared_library(&plugin_target).unwrap();
    if obs_build::BUILD_TARGET.contains("apple-darwin") {
        let bytes = fs::read(&plugin).unwrap();
        assert_eq!(u32::from_le_bytes(bytes[12..16].try_into().unwrap()), 8);
        let bytes = fs::read(&core).unwrap();
        assert_eq!(u32::from_le_bytes(bytes[12..16].try_into().unwrap()), 6);
    }
    // SAFETY: both sources above define the expected no-argument C functions.
    assert_eq!(
        unsafe { answer(&plugin, b"obs_fixture_plugin", &[&core]) },
        43
    );
    fs::write(
        &core_source,
        format!("{EXPORT} int obs_fixture_answer(void) {{ return 84; }}"),
    )
    .unwrap();
    build
        .shared_library(&target("fixture-core", &core_source, vec![]))
        .unwrap();
    // SAFETY: the rebuilt core preserves the function ABI; it changes its result.
    assert_eq!(
        unsafe { answer(&plugin, b"obs_fixture_plugin", &[&core]) },
        85
    );
}

#[test]
fn unity_toggle_preserves_exports_and_explicit_generated_source_wins() {
    let dir = tempfile::tempdir().unwrap();
    let source_dir = dir.path().join("source");
    let generated_dir = dir.path().join("generated");
    fs::create_dir_all(&source_dir).unwrap();
    fs::create_dir_all(&generated_dir).unwrap();
    fs::write(
        source_dir.join("manifest.c"),
        "#error stale source tree manifest selected\n",
    )
    .unwrap();
    let generated = generated_dir.join("manifest.c");
    fs::write(
        &generated,
        format!("{EXPORT} int obs_fixture_count(void) {{ return 17; }}"),
    )
    .unwrap();
    let extra = source_dir.join("extra.c");
    fs::write(&extra, "int obs_fixture_other(void) { return 23; }").unwrap();
    for unity in [false, true] {
        let build = NativeBuild {
            profile: Default::default(),
            host: obs_build::BUILD_HOST.into(),
            target: obs_build::BUILD_TARGET.into(),
            output: dir.path().join(format!("output-{unity}")),
        };
        let mut library = target("fixture-manifest", &generated, vec![]);
        library.sources.push(extra.clone());
        library.include_dirs.push(source_dir.clone());
        library.unity = unity;
        let artifact = build.shared_library(&library).unwrap();
        // SAFETY: the generated fixture defines this no-argument C function.
        assert_eq!(unsafe { answer(&artifact, b"obs_fixture_count", &[]) }, 17);
    }
}

#[test]
fn executable_runs_against_its_shared_core_dependency() {
    let dir = tempfile::tempdir().unwrap();
    let core_source = dir.path().join("core.c");
    let executable_source = dir.path().join("main.c");
    let helper_source = dir.path().join("helper.c");
    fs::write(
        &helper_source,
        "int fixture_private_value(void) { return 42; }",
    )
    .unwrap();
    fs::write(
        &core_source,
        format!("extern int fixture_private_value(void); {EXPORT} int fixture_value(void) {{ return fixture_private_value(); }}"),
    )
    .unwrap();
    fs::write(
        &executable_source,
        "extern int fixture_value(void); int main(void) { return fixture_value() == 42 ? 0 : 7; }",
    )
    .unwrap();
    let build = NativeBuild {
        profile: Default::default(),
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: dir.path().join("artifacts"),
    };
    let helper = build
        .static_library(&target("fixture-helper", &helper_source, vec![]))
        .unwrap();
    let core = build
        .shared_library(&target("fixture-core", &core_source, vec![helper]))
        .unwrap();
    let executable = build
        .executable(&target(
            "fixture-app",
            &executable_source,
            vec![core.clone()],
        ))
        .unwrap();
    let old_path = std::env::var_os("PATH").unwrap_or_default();
    let search = std::env::join_paths(
        std::iter::once(core.parent().unwrap().to_path_buf())
            .chain(std::env::split_paths(&old_path)),
    )
    .unwrap();
    assert!(
        std::process::Command::new(executable)
            .env("PATH", search)
            .status()
            .unwrap()
            .success()
    );
}

#[test]
fn staged_executable_prefers_runtime_core_over_build_outputs() {
    if !obs_build::BUILD_TARGET.contains("linux") {
        return;
    }
    let directory = tempfile::tempdir().unwrap();
    let source = directory.path().join("core.c");
    let main = directory.path().join("main.c");
    fs::write(
        &main,
        "extern int fixture_value(void); int main(void) { return fixture_value() == 42 ? 0 : 7; }",
    )
    .unwrap();
    let build = NativeBuild {
        profile: Default::default(),
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: directory.path().join("original"),
    };
    fs::write(&source, "int fixture_value(void) { return 1; }").unwrap();
    let original = build
        .shared_library(&target("fixture-core", &source, vec![]))
        .unwrap();
    let executable = build
        .executable(&target("fixture-app", &main, vec![original.clone()]))
        .unwrap();
    fs::write(&source, "int fixture_value(void) { return 42; }").unwrap();
    let runtime_build = NativeBuild {
        output: directory.path().join("replacement"),
        ..build.clone()
    };
    let replacement = runtime_build
        .shared_library(&target("fixture-core", &source, vec![]))
        .unwrap();
    let runtime = directory.path().join("runtime");
    fs::create_dir_all(runtime.join("bin")).unwrap();
    fs::create_dir_all(runtime.join("lib64")).unwrap();
    let staged = runtime.join("bin/fixture-app");
    fs::copy(&executable, &staged).unwrap();
    fs::copy(
        &replacement,
        runtime.join("lib64").join(replacement.file_name().unwrap()),
    )
    .unwrap();
    let status = std::process::Command::new(&staged)
        .env_remove("LD_LIBRARY_PATH")
        .status()
        .unwrap();
    assert!(
        status.success(),
        "staged executable loaded the original core: {status}"
    );
    // A separately loaded module must resolve its own dependency from the
    // staged lib64 directory, without the executable preloading that core.
    let plugin_source = directory.path().join("plugin.c");
    fs::write(
        &plugin_source,
        "extern int fixture_value(void); int plugin_value(void) { return fixture_value(); }",
    )
    .unwrap();
    let plugin = build
        .shared_library(&target("fixture-plugin", &plugin_source, vec![original]))
        .unwrap();
    let modules = runtime.join("lib64/obs-modules/core");
    fs::create_dir_all(&modules).unwrap();
    let staged_plugin = modules.join(plugin.file_name().unwrap());
    fs::copy(plugin, &staged_plugin).unwrap();
    let loader_source = directory.path().join("loader.c");
    fs::write(&loader_source, "#include <dlfcn.h>\nint main(int argc, char **argv) { if (argc != 2) return 2; void *lib = dlopen(argv[1], RTLD_NOW); if (!lib) return 3; int (*value)(void) = dlsym(lib, \"plugin_value\"); return value && value() == 42 ? 0 : 7; }").unwrap();
    let mut loader = target("fixture-loader", &loader_source, vec![]);
    loader.link_options.push("-ldl".into());
    let loader = build.executable(&loader).unwrap();
    let output = std::process::Command::new(loader)
        .arg(staged_plugin)
        .env_remove("LD_LIBRARY_PATH")
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "staged module loaded the original core: {}\n{}",
        output.status,
        String::from_utf8_lossy(&output.stderr)
    );
}

const EXPORT: &str = "\n#if defined(_WIN32)\n__declspec(dllexport)\n#endif\n";

#[test]
fn windows_resource_payload_tracks_headers_definitions_and_unity() {
    let requested = std::env::var("OBS_NATIVE_WINDOWS_TARGET")
        .unwrap_or_else(|_| obs_build::BUILD_TARGET.into());
    if !requested.contains("windows-msvc") {
        return;
    }
    let directory = tempfile::tempdir().unwrap();
    let root = directory.path();
    let source = root.join("resource-probe.c");
    let script = root.join("payload.rc");
    let header = root.join("resource-values.h");
    fs::write(
        &source,
        r#"
#include <windows.h>
#include "resource-values.h"
__declspec(dllexport) int obs_fixture_resource(void) {
    HMODULE owner = NULL;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&obs_fixture_resource, &owner)) return -1;
    HRSRC resource = FindResourceW(owner, MAKEINTRESOURCEW(17), MAKEINTRESOURCEW(10));
    if (!resource) return -2;
    HGLOBAL loaded = LoadResource(owner, resource);
    const WORD *value = loaded ? LockResource(loaded) : NULL;
    return value ? value[0] + value[1] : -3;
}
int main(void) { return obs_fixture_resource() == HEADER_VALUE + RESOURCE_VALUE ? 0 : 7; }
"#,
    )
    .unwrap();
    fs::write(
        &script,
        "#include \"resource-values.h\"\n17 RCDATA { HEADER_VALUE, RESOURCE_VALUE }\n",
    )
    .unwrap();
    for unity in [false, true] {
        fs::write(&header, "#define HEADER_VALUE 2\n").unwrap();
        let build = NativeBuild {
            profile: Default::default(),
            host: obs_build::BUILD_HOST.into(),
            target: requested.clone(),
            output: root.join(format!("resources-{unity}")),
        };
        let mut library = target("fixture-resources", &source, vec![]);
        library.unity = unity;
        library.resource_files.push(script.clone());
        library.resource_input_files.push(header.clone());
        library.include_dirs.push(root.into());
        library
            .definitions
            .push(("RESOURCE_VALUE".into(), Some("40".into())));
        // RC must receive resource options, not C-only compiler options.
        library.c_compile_options.push("/std:c17".into());
        library.resource_compile_options.push("/c65001".into());
        let artifact = build.shared_library(&library).unwrap();
        assert_windows_resource(&artifact, 42, [2, 40], &requested);
        fs::write(&header, "#define HEADER_VALUE 10\n").unwrap();
        let artifact = build.shared_library(&library).unwrap();
        assert_windows_resource(&artifact, 50, [10, 40], &requested);
        library.definitions[0].1 = Some("50".into());
        let artifact = build.shared_library(&library).unwrap();
        assert_windows_resource(&artifact, 60, [10, 50], &requested);
        library.name = "fixture-resource-app".into();
        let executable = build.executable(&library).unwrap();
        assert_windows_resource(&executable, 60, [10, 50], &requested);
        assert!(
            build
                .static_library(&library)
                .unwrap_err()
                .contains("resources")
        );
    }
}

fn assert_windows_resource(library: &Path, expected: i32, words: [u16; 2], target: &str) {
    if target == obs_build::BUILD_TARGET {
        if library
            .extension()
            .is_some_and(|extension| extension == "exe")
        {
            assert!(
                std::process::Command::new(library)
                    .status()
                    .unwrap()
                    .success()
            );
            return;
        }
        // SAFETY: the fixture exports this no-argument C ABI function, and the
        // resource lookup does not retain an extra reference to the DLL.
        assert_eq!(
            unsafe { answer(library, b"obs_fixture_resource", &[]) },
            expected
        );
    } else {
        // Cross compilation proves the PE payload; native CI also calls the
        // exported lookup above against the loaded DLL.
        let result = std::process::Command::new(
            std::env::var_os("OBS_NATIVE_WINDOWS_READOBJ").unwrap_or_else(|| "llvm-readobj".into()),
        )
        .arg("--coff-resources")
        .arg(library)
        .output()
        .unwrap();
        assert!(
            result.status.success(),
            "{}",
            String::from_utf8_lossy(&result.stderr)
        );
        let output = String::from_utf8(result.stdout).unwrap();
        let payload = words
            .into_iter()
            .flat_map(u16::to_le_bytes)
            .map(|byte| format!("{byte:02X}"))
            .collect::<String>();
        assert!(output.contains("Total Number of Resources: 1"), "{output}");
        assert!(output.contains("Name: (ID 17)"), "{output}");
        assert!(output.contains("DataSize: 4"), "{output}");
        assert!(
            output
                .lines()
                .any(|line| line.split_whitespace().nth(1) == Some(&payload)),
            "{output}"
        );
    }
}

#[test]
fn msvc_pch_links_its_object_and_preserves_language_header_and_flag_changes() {
    let requested = std::env::var("OBS_NATIVE_WINDOWS_TARGET")
        .unwrap_or_else(|_| obs_build::BUILD_TARGET.into());
    if !requested.contains("windows-msvc") {
        return;
    }
    let directory = tempfile::tempdir().unwrap();
    let root = directory.path();
    let header = root.join("common.h");
    let c = root.join("part.c");
    let cpp = root.join("part.cpp");
    let wrapper = root.join("archive-probe.cpp");
    fs::write(&wrapper, "extern \"C\" int obs_fixture_msvc_pch(void);\nextern \"C\" __declspec(dllexport) int obs_fixture_archive_probe(void) { return obs_fixture_msvc_pch(); }\n").unwrap();
    let write_header = |value| {
        fs::write(
            &header,
            format!(
                r#"
#pragma once
#define HEADER_VALUE {value}
#ifdef __cplusplus
#include <vector>
#ifdef FIXTURE_C
#error C flags leaked into C++ PCH
#endif
__declspec(selectany) int fixture_cpp_storage = HEADER_VALUE;
#else
#include <stdint.h>
#ifdef FIXTURE_CPP
#error C++ flags leaked into C PCH
#endif
__declspec(selectany) int fixture_c_storage = HEADER_VALUE;
#endif
"#
            ),
        )
        .unwrap()
    };
    fs::write(&c, "#include \"common.h\"\nint fixture_pch_c(void) { return fixture_c_storage + FIXTURE_C; }\n").unwrap();
    fs::write(&cpp, "#include \"common.h\"\nextern \"C\" int fixture_pch_c(void);\nextern \"C\" __declspec(dllexport) int obs_fixture_msvc_pch(void) { std::vector<int> values{fixture_cpp_storage, FIXTURE_CPP}; return fixture_pch_c() + values[0] + values[1]; }\n").unwrap();
    for unity in [false, true] {
        write_header(17);
        let build = NativeBuild {
            profile: Default::default(),
            host: obs_build::BUILD_HOST.into(),
            target: requested.clone(),
            output: root.join(format!("msvc-pch-{unity}")),
        };
        let mut library = target("fixture-msvc-pch", &c, vec![]);
        library.sources.push(cpp.clone());
        library.include_dirs.push(root.into());
        library.unity = unity;
        library.c_compile_options = vec!["/std:c17".into(), "/DFIXTURE_C=3".into()];
        library.cpp_compile_options = vec!["/std:c++17".into(), "/DFIXTURE_CPP=5".into()];
        library
            .precompiled_headers
            .push(obs_build::PrecompiledHeader {
                header: header.clone(),
                sources: vec![c.clone(), cpp.clone()],
            });
        for (value, c_flag, cpp_flag, expected) in [
            (17, 3, 5, 42),
            (34, 3, 5, 76),
            (34, 6, 5, 79),
            (34, 6, 10, 84),
        ] {
            write_header(value);
            library.c_compile_options[1] = format!("/DFIXTURE_C={c_flag}");
            library.cpp_compile_options[1] = format!("/DFIXTURE_CPP={cpp_flag}");
            let artifact = build.shared_library(&library).unwrap();
            assert_windows_function(&artifact, "obs_fixture_msvc_pch", expected, &requested);
        }
        let archive = build.static_library(&library).unwrap();
        let artifact = build
            .shared_library(&target("fixture-archive-pch", &wrapper, vec![archive]))
            .unwrap();
        assert_windows_function(&artifact, "obs_fixture_archive_probe", 84, &requested);
        let pch: Vec<_> = fs::read_dir(build.output.join("pch"))
            .unwrap()
            .map(|entry| entry.unwrap().path())
            .collect();
        assert_eq!(
            pch.len(),
            4,
            "PCH identity must distinguish C/C++ and their changed flags"
        );
        for directory in pch {
            assert!(directory.join("prefix.pch").metadata().unwrap().len() > 0);
            assert!(
                directory.join("prefix.obj").is_file(),
                "MSVC PCH creation also emits a required object"
            );
        }
    }
}

fn assert_windows_function(library: &Path, symbol: &str, expected: i32, target: &str) {
    if target == obs_build::BUILD_TARGET {
        // SAFETY: both owning fixtures define no-argument C ABI functions;
        // answer keeps each library loaded throughout the call.
        assert_eq!(unsafe { answer(library, symbol.as_bytes(), &[]) }, expected);
    } else {
        let result = std::process::Command::new(
            std::env::var_os("OBS_NATIVE_WINDOWS_READOBJ").unwrap_or_else(|| "llvm-readobj".into()),
        )
        .arg("--coff-exports")
        .arg(library)
        .output()
        .unwrap();
        assert!(
            result.status.success(),
            "{}",
            String::from_utf8_lossy(&result.stderr)
        );
        let name = format!("Name: {symbol}");
        assert!(
            String::from_utf8_lossy(&result.stdout)
                .lines()
                .any(|line| line.trim() == name)
        );
    }
}

#[test]
fn pch_preserves_mixed_languages_unity_and_header_flag_invalidation() {
    if !obs_build::BUILD_TARGET.contains("linux") && !obs_build::BUILD_TARGET.contains("apple") {
        eprintln!("compiled PCH integration covers Linux GCC/Clang and Apple Clang");
        return;
    }
    let directory = tempfile::tempdir().unwrap();
    let root = directory.path();
    let header = root.join("common.h");
    let transitive = root.join("value.h");
    let write_header = |value| {
        fs::write(&header, format!("#pragma once\n#include \"value.h\"\n#define HEADER_VALUE {value}\n#ifdef __cplusplus\n#include <vector>\n#else\n#include <stdint.h>\n#endif\n")).unwrap()
    };
    let c = root.join("part.c");
    let cpp = root.join("part.cpp");
    fs::write(&c, "#include \"common.h\"\n#ifdef FIXTURE_CPP\n#error C++ flags leaked into C\n#endif\nint fixture_c(void) { return HEADER_VALUE + FIXTURE_C + TRANSITIVE_VALUE; }\n").unwrap();
    fs::write(&cpp, "#include \"common.h\"\n#ifdef FIXTURE_C\n#error C flags leaked into C++\n#endif\nextern \"C\" int fixture_c(void);\nextern \"C\" int obs_fixture_pch(void) { std::vector<int> values{HEADER_VALUE + TRANSITIVE_VALUE, FIXTURE_CPP}; return fixture_c() + values[0] + values[1]; }\n").unwrap();
    for unity in [false, true] {
        write_header(17);
        fs::write(&transitive, "#define TRANSITIVE_VALUE 0\n").unwrap();
        let build = NativeBuild {
            profile: Default::default(),
            host: obs_build::BUILD_HOST.into(),
            target: obs_build::BUILD_TARGET.into(),
            output: root.join(format!("output-{unity}")),
        };
        let mut library = target("fixture-pch", &c, vec![]);
        library.sources.push(cpp.clone());
        library.include_dirs.push(root.into());
        library.unity = unity;
        library.c_compile_options.push("-DFIXTURE_C=3".into());
        library.cpp_compile_options.push("-DFIXTURE_CPP=5".into());
        library
            .precompiled_headers
            .push(obs_build::PrecompiledHeader {
                header: header.clone(),
                sources: vec![c.clone(), cpp.clone()],
            });
        let artifact = build.shared_library(&library).unwrap();
        // SAFETY: the fixture defines this no-argument C ABI function.
        assert_eq!(unsafe { answer(&artifact, b"obs_fixture_pch", &[]) }, 42);
        write_header(37);
        let artifact = build.shared_library(&library).unwrap();
        // SAFETY: the edited header preserves the function ABI.
        assert_eq!(unsafe { answer(&artifact, b"obs_fixture_pch", &[]) }, 82);
        fs::write(&transitive, "#define TRANSITIVE_VALUE 11\n").unwrap();
        let artifact = build.shared_library(&library).unwrap();
        // SAFETY: the edited transitive header preserves the function ABI.
        assert_eq!(unsafe { answer(&artifact, b"obs_fixture_pch", &[]) }, 104);
        library.c_compile_options = vec!["-DFIXTURE_C=7".into()];
        let artifact = build.shared_library(&library).unwrap();
        // SAFETY: changing the compiler macro preserves the function ABI.
        assert_eq!(unsafe { answer(&artifact, b"obs_fixture_pch", &[]) }, 108);
    }
}

#[test]
fn explicit_groups_compile_as_individual_sources_and_observe_member_edits() {
    let directory = tempfile::tempdir().unwrap();
    let root = directory.path();
    fs::create_dir(root.join("build")).unwrap();
    fs::write(root.join("build/fixture.pch.h"), "#define VALUE 17\n").unwrap();
    let first = root.join("first.c.h");
    fs::write(
        &first,
        "#include \"build/fixture.pch.h\"\nint first(void) { return VALUE; }\n",
    )
    .unwrap();
    fs::write(root.join("second.c.h"), format!("#include \"build/fixture.pch.h\"\nextern int first(void); {EXPORT} int obs_fixture_separate(void) {{ return first() + VALUE; }}\n")).unwrap();
    let group = root.join("build/fixture+.h.c");
    fs::write(&group, "/* explicit group */\n#include \"build/fixture.pch.h\"\n#include \"first.c.h\"\n#include \"second.c.h\"\n").unwrap();
    let build = NativeBuild {
        profile: Default::default(),
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: root.join("artifacts"),
    };
    let mut library = target("fixture-separate", &group, vec![]);
    library.unity = false;
    library.include_dirs.push(root.into());
    library.sources = build
        .separate_group_sources(root, &library.sources)
        .unwrap();
    assert_eq!(library.sources.len(), 2);
    assert!(!library.sources.contains(&group));
    let artifact = build.shared_library(&library).unwrap();
    // SAFETY: the fixture defines this no-argument C function.
    assert_eq!(
        unsafe { answer(&artifact, b"obs_fixture_separate", &[]) },
        34
    );
    fs::write(
        &first,
        "#include \"build/fixture.pch.h\"\nint first(void) { return VALUE + 1; }\n",
    )
    .unwrap();
    fs::write(root.join("build/fixture.pch.h"), "#define VALUE 21\n").unwrap();
    let artifact = build.shared_library(&library).unwrap();
    // SAFETY: the rebuilt fixture preserves its function ABI.
    assert_eq!(
        unsafe { answer(&artifact, b"obs_fixture_separate", &[]) },
        43
    );
}

#[test]
fn individual_compilation_rejects_conditional_group_selection() {
    let directory = tempfile::tempdir().unwrap();
    let group = directory.path().join("conditional+.h.c");
    fs::write(
        &group,
        "#if ENABLE_OPTION\n#include \"optional.c.h\"\n#endif\n",
    )
    .unwrap();
    let build = NativeBuild {
        profile: Default::default(),
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: directory.path().join("artifacts"),
    };
    let error = build
        .separate_group_sources(directory.path(), &[group])
        .unwrap_err();
    assert!(error.contains("non-include group line"), "{error}");
}

#[test]
fn deploying_mux_helper_in_place_preserves_its_contents() {
    let directory = tempfile::tempdir().unwrap();
    let helper = directory.path().join("obs-ffmpeg-mux");
    std::fs::write(&helper, b"existing executable").unwrap();
    let artifacts = obs_build::plugins::FfmpegArtifacts {
        module: obs_build::plugins::Module {
            library: std::path::PathBuf::new(),
            data: std::path::PathBuf::new(),
        },
        muxer: helper.clone(),
    };
    assert_eq!(artifacts.deploy_muxer(directory.path()).unwrap(), helper);
    assert_eq!(std::fs::read(helper).unwrap(), b"existing executable");
}

#[test]
fn mixed_c_and_cpp_use_independent_flags_and_the_cpp_linker() {
    let directory = tempfile::tempdir().unwrap();
    let c_source = directory.path().join("helper.c");
    let cpp_source = directory.path().join("main.cpp");
    std::fs::write(&c_source, "#if !defined(C_ONLY) || defined(CPP_ONLY)\n#error wrong C flags\n#endif\nint helper(void) { return 42; }\n").unwrap();
    std::fs::write(&cpp_source, "#include <vector>\n#if !defined(CPP_ONLY) || defined(C_ONLY)\n#error wrong C++ flags\n#endif\nextern \"C\" int helper(void);\nint main() { std::vector<int> values{helper()}; return values.at(0) == 42 ? 0 : 1; }\n").unwrap();
    let target = SharedLibrary {
        name: "mixed-native".into(),
        sources: vec![c_source, cpp_source],
        c_compile_options: vec!["-DC_ONLY".into()],
        cpp_compile_options: vec!["-DCPP_ONLY".into()],
        ..SharedLibrary::default()
    };
    let build = NativeBuild {
        profile: Default::default(),
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: directory.path().join("out"),
    };
    let artifact = build.executable(&target).unwrap();
    assert!(
        std::process::Command::new(artifact)
            .status()
            .unwrap()
            .success()
    );
}

#[test]
fn pch_batches_preserve_constructor_order_with_interleaved_headers() {
    if !obs_build::BUILD_TARGET.contains("linux") {
        eprintln!("compiled PCH integration currently covers Linux GCC and Clang");
        return;
    }
    let directory = tempfile::tempdir().unwrap();
    let root = directory.path();
    let a = root.join("a-header.h");
    let z = root.join("z-header.h");
    for header in [&a, &z] {
        fs::write(header, "#pragma once\nextern int fixture_order;\n").unwrap();
    }
    let mut sources = Vec::new();
    for index in 1..=3 {
        let path = root.join(format!("part-{index}.cpp"));
        let header = if index == 2 {
            "a-header.h"
        } else {
            "z-header.h"
        };
        fs::write(&path, format!("#include \"{header}\"\nstruct Init{index} {{ Init{index}() {{ fixture_order = fixture_order * 10 + {index}; }} }};\nstatic Init{index} init{index};\n")).unwrap();
        sources.push(path);
    }
    let entry = root.join("entry.cpp");
    fs::write(&entry, "int fixture_order = 0;\nextern \"C\" int obs_fixture_order(void) { return fixture_order; }\n").unwrap();
    for unity in [false, true] {
        let build = NativeBuild {
            profile: Default::default(),
            host: obs_build::BUILD_HOST.into(),
            target: obs_build::BUILD_TARGET.into(),
            output: root.join(format!("artifacts-{unity}")),
        };
        let mut library = target("fixture-order", &entry, vec![]);
        library.sources = sources.clone();
        library.sources.push(entry.clone());
        library.include_dirs.push(root.into());
        library.unity = unity;
        library.precompiled_headers = vec![
            obs_build::PrecompiledHeader {
                header: a.clone(),
                sources: vec![sources[1].clone()],
            },
            obs_build::PrecompiledHeader {
                header: z.clone(),
                sources: vec![sources[0].clone(), sources[2].clone()],
            },
        ];
        let artifact = build.shared_library(&library).unwrap();
        // SAFETY: the fixture defines this no-argument C ABI function.
        assert_eq!(unsafe { answer(&artifact, b"obs_fixture_order", &[]) }, 123);
    }
}

#[test]
fn production_hybrid_requires_a_real_rust_archive_before_native_compilation() {
    let directory = tempfile::tempdir().unwrap();
    let build = NativeBuild {
        profile: Default::default(),
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: directory.path().join("build"),
    };
    let error = obs_build::libobs::build_hybrid_with_options(
        &build,
        directory.path(),
        &obs_build::libobs::Options::default(),
        &directory.path().join("missing-rust.a"),
    )
    .unwrap_err();
    assert!(error.contains("Rust archive does not exist"), "{error}");
    assert!(!build.output.exists(), "must not silently start a C build");
}

#[test]
fn objective_groups_expand_to_individual_language_correct_sources() {
    let directory = tempfile::tempdir().unwrap();
    let root = directory.path();
    let build = NativeBuild {
        profile: Default::default(),
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: root.join("artifacts"),
    };
    for (suffix, implementation, extension) in
        [("+.h.m", ".m.h", "m"), ("+.hpp.mm", ".mm.hpp", "mm")]
    {
        let member = format!("member{implementation}");
        fs::write(root.join(&member), "/* fixture */\n").unwrap();
        let group = root.join(format!("fixture{suffix}"));
        fs::write(&group, format!("#include \"{member}\"\n")).unwrap();
        let sources = build
            .separate_group_sources(root, std::slice::from_ref(&group))
            .unwrap();
        assert_eq!(sources.len(), 1);
        assert_ne!(
            sources[0], group,
            "unity-off must expand Objective-C groups"
        );
        assert_eq!(sources[0].extension().unwrap(), extension);
        assert!(fs::read_to_string(&sources[0]).unwrap().contains(&member));
    }
}

#[test]
fn mixed_objective_languages_compile_load_and_rebuild_with_unity_and_pch() {
    if native_answer_probe() {
        return;
    }
    if !obs_build::BUILD_TARGET.contains("linux") && !obs_build::BUILD_TARGET.contains("apple") {
        return; // The fixture requires an Objective-C-capable Clang toolchain.
    }
    let selector = format!(
        "{}::mixed_objective_languages_compile_load_and_rebuild_with_unity_and_pch",
        module_path!().split_once("::").unwrap().1
    );
    if std::env::var_os("OBS_BUILD_OBJC_CHILD").is_none() {
        let result = std::process::Command::new(std::env::current_exe().unwrap())
            .args(["--exact", &selector, "--nocapture"])
            .env("OBS_BUILD_OBJC_CHILD", "1")
            .env("CC", "clang")
            .env("CXX", "clang++")
            .output()
            .unwrap();
        assert!(
            result.status.success(),
            "{}\n{}",
            String::from_utf8_lossy(&result.stdout),
            String::from_utf8_lossy(&result.stderr)
        );
        assert!(String::from_utf8_lossy(&result.stdout).contains("1 passed"));
        return;
    }
    let directory = tempfile::tempdir().unwrap();
    let root = directory.path();
    let header = root.join("fixture.pch.h");
    let write_header = |value| {
        fs::write(&header, format!("#define VALUE {value}\n#if defined(__OBJC__)\n#if !__has_feature(objc_arc)\n#error ARC flag missing\n#endif\n#ifdef __cplusplus\n#if !defined(OBJCPP_ONLY) || defined(OBJC_ONLY)\n#error Objective-C++ flags missing or contaminated\n#endif\n#else\n#if !defined(OBJC_ONLY) || defined(OBJCPP_ONLY)\n#error Objective-C flags missing or contaminated\n#endif\n#endif\n#else\n#if defined(OBJC_ONLY) || defined(OBJCPP_ONLY)\n#error Objective-C flags leaked to C or C++\n#endif\n#endif\n")).unwrap();
    };
    write_header(1);
    let c = root.join("part.c");
    let cpp = root.join("part.cpp");
    let objc = root.join("part.m");
    let objcpp = root.join("part.mm");
    fs::write(&c, "#include \"fixture.pch.h\"\n#if defined(__OBJC__) || defined(__cplusplus)\n#error wrong C language\n#endif\nint fixture_c(void) { return VALUE; }\n").unwrap();
    fs::write(&objc, "#include \"fixture.pch.h\"\n#if !defined(__OBJC__) || defined(__cplusplus)\n#error wrong Objective-C language\n#endif\nint fixture_objc(void) { return 4 * VALUE * OBJC_ONLY * (@encode(int)[0] == 'i'); }\n").unwrap();
    fs::write(&objcpp, "#include \"fixture.pch.h\"\n#if !defined(__OBJC__) || !defined(__cplusplus)\n#error wrong Objective-C++ language\n#endif\ntemplate<int N> constexpr int value() { return N; }\nextern \"C\" int fixture_objcpp(void) { return value<8>() * VALUE * OBJCPP_ONLY * (@encode(int)[0] == 'i'); }\n").unwrap();
    fs::write(&cpp, "#include \"fixture.pch.h\"\n#if defined(__OBJC__) || !defined(__cplusplus)\n#error wrong C++ language\n#endif\nextern \"C\" int fixture_c(void);\nextern \"C\" int fixture_objc(void);\nextern \"C\" int fixture_objcpp(void);\nextern \"C\" int obs_fixture_languages(void) { return fixture_c() + fixture_objc() + fixture_objcpp() + 2 * VALUE; }\n").unwrap();
    for unity in [false, true] {
        for pch in [false, true] {
            write_header(1);
            let build = NativeBuild {
                profile: Default::default(),
                host: obs_build::BUILD_HOST.into(),
                target: obs_build::BUILD_TARGET.into(),
                output: root.join(format!("output-{unity}-{pch}")),
            };
            let mut library = target("fixture-languages", &c, vec![]);
            library
                .sources
                .extend([objc.clone(), cpp.clone(), objcpp.clone()]);
            library.include_dirs.push(root.into());
            library.unity = unity;
            library.cpp_compile_options.push("-fno-exceptions".into());
            // Select a modern runtime for ARC parsing on Linux too. The fixture
            // emits no Objective-C objects or runtime calls and needs no SDK.
            library.objc_compile_options = vec![
                "-fobjc-arc".into(),
                "-fobjc-runtime=macosx-10.12".into(),
                "-DOBJC_ONLY=1".into(),
            ];
            library.objcpp_compile_options = vec![
                "-fobjc-arc".into(),
                "-fobjc-runtime=macosx-10.12".into(),
                "-DOBJCPP_ONLY=1".into(),
            ];
            if pch {
                library
                    .precompiled_headers
                    .push(obs_build::PrecompiledHeader {
                        header: header.clone(),
                        sources: library.sources.clone(),
                    });
            }
            let artifact = build.shared_library(&library).unwrap();
            assert_native_answer_fresh(&artifact, "obs_fixture_languages", 15, &selector);
            write_header(2);
            let artifact = build.shared_library(&library).unwrap();
            assert_native_answer_fresh(&artifact, "obs_fixture_languages", 30, &selector);
            library.objc_compile_options[2] = "-DOBJC_ONLY=2".into();
            let artifact = build.shared_library(&library).unwrap();
            assert_native_answer_fresh(&artifact, "obs_fixture_languages", 38, &selector);
            library.objcpp_compile_options[2] = "-DOBJCPP_ONLY=2".into();
            let artifact = build.shared_library(&library).unwrap();
            assert_native_answer_fresh(&artifact, "obs_fixture_languages", 54, &selector);
        }
    }
}

#[test]
fn arc_and_manual_groups_keep_source_flags_through_expansion_and_pch() {
    if native_answer_probe() {
        return;
    }
    if !obs_build::BUILD_TARGET.contains("linux") && !obs_build::BUILD_TARGET.contains("apple") {
        return;
    }
    let selector = format!(
        "{}::arc_and_manual_groups_keep_source_flags_through_expansion_and_pch",
        module_path!().split_once("::").unwrap().1
    );
    if std::env::var_os("OBS_BUILD_SOURCE_FLAGS_CHILD").is_none() {
        let result = std::process::Command::new(std::env::current_exe().unwrap())
            .args(["--exact", &selector, "--nocapture"])
            .env("OBS_BUILD_SOURCE_FLAGS_CHILD", "1")
            .env("CC", "clang")
            .env("CXX", "clang++")
            .output()
            .unwrap();
        assert!(
            result.status.success(),
            "{}\n{}",
            String::from_utf8_lossy(&result.stdout),
            String::from_utf8_lossy(&result.stderr)
        );
        assert!(String::from_utf8_lossy(&result.stdout).contains("1 passed"));
        return;
    }
    let directory = tempfile::tempdir().unwrap();
    let root = directory.path();
    let header = root.join("fixture.pch.h");
    fs::write(&header, "#pragma once\n#ifndef SOURCE_VALUE\n#error source flags missing from PCH\n#endif\n#define HEADER_ARC __has_feature(objc_arc)\n").unwrap();
    let arc_member = root.join("arc.m.h");
    let manual_member = root.join("manual.m.h");
    fs::write(&arc_member, "#include \"fixture.pch.h\"\n#if !HEADER_ARC\n#error ARC disabled for ARC member\n#endif\nint fixture_arc(void) { return SOURCE_VALUE; }\n").unwrap();
    fs::write(&manual_member, "#include \"fixture.pch.h\"\n#if HEADER_ARC\n#error ARC enabled for manual member\n#endif\nint fixture_arc(void);\nint obs_fixture_source_flags(void) { return SOURCE_VALUE + fixture_arc(); }\n").unwrap();
    let arc = root.join("arc+.h.m");
    let manual = root.join("manual+.h.m");
    fs::write(&arc, "#include \"fixture.pch.h\"\n#include \"arc.m.h\"\n").unwrap();
    fs::write(
        &manual,
        "#include \"fixture.pch.h\"\n#include \"manual.m.h\"\n",
    )
    .unwrap();
    for separate in [false, true] {
        for pch in [false, true] {
            let build = NativeBuild {
                profile: Default::default(),
                host: obs_build::BUILD_HOST.into(),
                target: obs_build::BUILD_TARGET.into(),
                output: root.join(format!("output-{separate}-{pch}")),
            };
            let mut library = target("fixture-source-flags", &arc, vec![]);
            library.sources.push(manual.clone());
            library.include_dirs.push(root.into());
            library.objc_compile_options =
                vec!["-fobjc-runtime=macosx-10.12".into(), "-fno-objc-arc".into()];
            library.source_compile_options = vec![
                obs_build::SourceCompileOptions {
                    sources: vec![arc.clone()],
                    options: vec!["-fobjc-arc".into(), "-DSOURCE_VALUE=17".into()],
                },
                obs_build::SourceCompileOptions {
                    sources: vec![manual.clone()],
                    options: vec!["-DSOURCE_VALUE=25".into()],
                },
            ];
            if pch {
                library
                    .precompiled_headers
                    .push(obs_build::PrecompiledHeader {
                        header: header.clone(),
                        sources: library.sources.clone(),
                    });
            }
            if separate {
                build.separate_target_sources(root, &mut library).unwrap();
            }
            let artifact = build.shared_library(&library).unwrap();
            assert_native_answer_fresh(&artifact, "obs_fixture_source_flags", 42, &selector);
            library.source_compile_options[0].options[1] = "-DSOURCE_VALUE=35".into();
            let artifact = build.shared_library(&library).unwrap();
            assert_native_answer_fresh(&artifact, "obs_fixture_source_flags", 60, &selector);
        }
    }
}

#[test]
fn native_source_options_reject_ambiguous_or_orphan_assignments() {
    let dir = tempfile::tempdir().unwrap();
    let source = dir.path().join("source.c");
    let other = dir.path().join("other.c");
    fs::write(&source, "int fixture(void) { return 1; }\n").unwrap();
    fs::write(&other, "int other(void) { return 2; }\n").unwrap();
    let build = NativeBuild {
        profile: Default::default(),
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: dir.path().join("artifacts"),
    };
    let mut library = target("fixture-options", &source, vec![]);
    let entry = obs_build::SourceCompileOptions {
        sources: vec![source.clone()],
        options: vec!["-DVALUE=1".into()],
    };
    library.source_compile_options = vec![entry.clone(), entry];
    assert!(
        build
            .shared_library(&library)
            .unwrap_err()
            .contains("duplicate compiler options")
    );
    library.source_compile_options = vec![obs_build::SourceCompileOptions {
        sources: vec![other],
        options: vec![],
    }];
    assert!(
        build
            .shared_library(&library)
            .unwrap_err()
            .contains("outside this target")
    );
    assert!(
        build
            .separate_target_sources(dir.path(), &mut library)
            .unwrap_err()
            .contains("annotation outside target")
    );
    assert_eq!(
        library.sources,
        vec![source],
        "an invalid expansion must not mutate the target"
    );
}

// dyld retains Objective-C dylibs after dlclose. Probe rebuilt images in fresh
// processes so compiler/header invalidation is observed independently of that
// loader lifetime policy. The probe reuses its owning test in this category.
fn native_answer_probe() -> bool {
    let Some(library) = std::env::var_os("OBS_NATIVE_ANSWER_LIBRARY") else {
        return false;
    };
    let symbol = std::env::var("OBS_NATIVE_ANSWER_SYMBOL").unwrap();
    let expected: i32 = std::env::var("OBS_NATIVE_ANSWER_EXPECTED")
        .unwrap()
        .parse()
        .unwrap();
    // SAFETY: these self-built fixtures define a no-argument C ABI function;
    // answer keeps the library alive throughout the call.
    assert_eq!(
        unsafe { answer(Path::new(&library), symbol.as_bytes(), &[]) },
        expected
    );
    true
}

fn assert_native_answer_fresh(library: &Path, symbol: &str, expected: i32, selector: &str) {
    let result = std::process::Command::new(std::env::current_exe().unwrap())
        .args(["--exact", selector, "--nocapture"])
        .env("OBS_NATIVE_ANSWER_LIBRARY", library)
        .env("OBS_NATIVE_ANSWER_SYMBOL", symbol)
        .env("OBS_NATIVE_ANSWER_EXPECTED", expected.to_string())
        .output()
        .unwrap();
    assert!(
        result.status.success(),
        "{}\n{}",
        String::from_utf8_lossy(&result.stdout),
        String::from_utf8_lossy(&result.stderr)
    );
    assert!(String::from_utf8_lossy(&result.stdout).contains("1 passed"));
}
