//! Build the unchanged native C tests through the Cargo native engine.
use crate::{NativeBuild, SharedLibrary, libobs::add_package};
use std::{
    collections::{BTreeSet, HashMap},
    path::{Path, PathBuf},
};

pub struct Executable {
    pub name: String,
    pub path: PathBuf,
}

pub struct Options {
    pub hevc: bool,
    pub per_target_sanitizers: bool,
}

impl Default for Options {
    fn default() -> Self {
        Self {
            hevc: true,
            per_target_sanitizers: true,
        }
    }
}

/// Build the unchanged standalone WHIP clock and POSIX host tests.
pub fn build_webrtc_tests(build: &NativeBuild, root: &Path) -> Result<Vec<Executable>, String> {
    let root = dunce::canonicalize(root).map_err(|e| e.to_string())?;
    let mut base = SharedLibrary {
        include_dirs: vec![root.clone(), root.join("plugins/obs-webrtc")],
        ..SharedLibrary::default()
    };
    crate::platform::configure_webrtc_tests(&build.target, &mut base)?;
    add_package(&mut base, &build.target, "cmocka")?;
    let mut result = Vec::new();
    for (name, extension) in [
        ("test_whip_rtp_timestamp", "c"),
        ("test_whip_host_os_name", "cpp"),
    ] {
        let library = SharedLibrary {
            name: name.into(),
            sources: vec![root.join(format!("test/cmocka/{name}.{extension}"))],
            ..base.clone()
        };
        result.push(Executable {
            name: name.into(),
            path: build.executable(&library)?,
        });
    }
    for directory in ["test/cmocka", "plugins/obs-webrtc"] {
        println!("cargo:rerun-if-changed={}", root.join(directory).display());
    }
    Ok(result)
}

/// Build unchanged cmocka tests that require the native graphics module.
pub fn build_graphics_tests(
    build: &NativeBuild,
    root: &Path,
    core: &Path,
    graphics: &Path,
) -> Result<Vec<Executable>, String> {
    let root = dunce::canonicalize(root).map_err(|e| e.to_string())?;
    let graphics = dunce::canonicalize(graphics).map_err(|e| e.to_string())?;
    let quoted = serde_json::to_string(graphics.to_str().ok_or("non-UTF8 graphics test path")?)
        .map_err(|e| e.to_string())?;
    let name = "test_group_hide_transition_audio";
    let mut library = SharedLibrary {
        name: name.into(),
        sources: vec![root.join(format!("test/cmocka/{name}.c"))],
        include_dirs: vec![
            root.join("libobs"),
            root.join("deps/libcaption"),
            build.output.join("config"),
        ],
        definitions: vec![("GRAPHICS_MODULE_PATH".into(), Some(quoted))],
        dependencies: vec![core.into(), crate::libobs::build_caption(build, &root)?],
        ..SharedLibrary::default()
    };
    crate::platform::configure_c_tests(build, &mut library, None)?;
    for package in ["cmocka", "simde"] {
        add_package(&mut library, &build.target, package)?;
    }
    println!("cargo:rerun-if-changed={}", library.sources[0].display());
    println!(
        "cargo:rerun-if-changed={}",
        root.join("libobs/data").display()
    );
    Ok(vec![Executable {
        name: name.into(),
        path: build.executable(&library)?,
    }])
}

/// Build the unchanged VLC compatibility tests against the real native modules.
pub fn build_vlc_tests(
    build: &NativeBuild,
    root: &Path,
    core: &Path,
    vlc: &crate::plugins::Module,
    graphics: &Path,
) -> Result<Vec<Executable>, String> {
    let root = dunce::canonicalize(root).map_err(|e| e.to_string())?;
    let mut base = SharedLibrary {
        include_dirs: vec![
            root.clone(),
            root.join("libobs"),
            root.join("test/cmocka"),
            root.join("plugins/vlc-video"),
            build.output.join("config"),
        ],
        dependencies: vec![core.into()],
        ..SharedLibrary::default()
    };
    crate::platform::configure_c_tests(build, &mut base, None)?;
    for package in ["cmocka", "simde"] {
        add_package(&mut base, &build.target, package)?;
    }
    for (name, path) in [
        ("VLC_MODULE_PATH", vlc.library.as_path()),
        ("VLC_DATA_PATH", vlc.data.as_path()),
        ("GRAPHICS_MODULE_PATH", graphics),
    ] {
        let path = dunce::canonicalize(path).map_err(|e| e.to_string())?;
        let quoted = serde_json::to_string(path.to_str().ok_or("non-UTF8 VLC test path")?)
            .map_err(|e| e.to_string())?;
        base.definitions.push((name.into(), Some(quoted)));
    }
    let mut result = Vec::new();
    for name in [
        "test_vlc_drain_gate",
        "test_vlc_host_path",
        "test_vlc_channel_order",
        "test_vlc_audio_track",
        "test_vlc_audio_pacing",
        "test_vlc_playlist_gap",
    ] {
        let mut library = SharedLibrary {
            name: name.into(),
            sources: vec![root.join(format!("test/cmocka/{name}.c"))],
            ..base.clone()
        };
        if name == "test_vlc_audio_pacing" {
            for package in ["libavcodec", "libavformat", "libavutil"] {
                add_package(&mut library, &build.target, package)?;
            }
        }
        result.push(Executable {
            name: name.into(),
            path: build.executable(&library)?,
        });
    }
    for directory in ["test/cmocka", "plugins/vlc-video", "libobs/data"] {
        println!("cargo:rerun-if-changed={}", root.join(directory).display());
    }
    Ok(result)
}

pub fn build_c_tests(
    build: &NativeBuild,
    root: &Path,
    core: &Path,
    options: &Options,
) -> Result<Vec<Executable>, String> {
    let root = dunce::canonicalize(root).map_err(|e| e.to_string())?;
    let manifest: Vec<serde_json::Value> =
        serde_json::from_str(include_str!("../manifests/c-tests-linux.json"))
            .map_err(|e| e.to_string())?;
    let mut result = Vec::new();
    let mut archives = HashMap::new();
    let mut watched: BTreeSet<PathBuf> = ["libobs", "test/cmocka", "deps/libcaption"]
        .into_iter()
        .map(|path| root.join(path))
        .collect();
    let mut base = SharedLibrary {
        include_dirs: vec![
            root.clone(),
            root.join("libobs"),
            root.join("deps/libcaption"),
            root.join("test/cmocka"),
            build.output.join("config"),
        ],
        dependencies: vec![core.into()],
        ..SharedLibrary::default()
    };
    if options.hevc {
        base.definitions.push(("ENABLE_HEVC".into(), None));
    }
    for package in ["cmocka", "simde"] {
        add_package(&mut base, &build.target, package)?;
    }
    // Shared group headers expose Jansson declarations even to a private AVC
    // implementation fixture. Header discovery adds no new runtime link edge.
    headers_only(&mut base, "jansson")?;
    for spec in manifest {
        let name = spec["name"].as_str().ok_or("C test name missing")?;
        if name == "test_hevc" && !options.hevc {
            continue;
        }
        let mut library = SharedLibrary {
            name: name.into(),
            ..base.clone()
        };
        let sanitizer = options
            .per_target_sanitizers
            .then(|| spec["sanitizers"].as_str())
            .flatten();
        crate::platform::configure_c_tests(build, &mut library, sanitizer)?;
        for source in strings(&spec, "sources")? {
            // Watch each selected component, including its group headers, rather
            // than the repository root (which may contain native build output).
            let component: PathBuf = Path::new(source).components().take(2).collect();
            if source.starts_with("plugins/") || source.starts_with("shared/") {
                watched.insert(root.join(component));
            }
            let source = root.join(source);
            let source = if source.to_string_lossy().ends_with(".c.h") {
                let relative = source.strip_prefix(&root).map_err(|e| e.to_string())?;
                let wrapper = build
                    .output
                    .join(name)
                    .join("inputs")
                    .join(relative)
                    .with_extension("h.c");
                let include = source
                    .to_str()
                    .ok_or("non-UTF8 C test source")?
                    .replace('\\', "/");
                if include.contains(['\n', '\r', '"']) {
                    return Err(format!("unrepresentable C test include {include:?}"));
                }
                std::fs::create_dir_all(wrapper.parent().ok_or("C test wrapper has no parent")?)
                    .map_err(|e| e.to_string())?;
                crate::frontend::write(&wrapper, &format!("#include \"{include}\"\n"))?;
                wrapper
            } else {
                source
            };
            library.sources.push(source);
        }
        for include in strings(&spec, "includes")? {
            library.include_dirs.push(root.join(include));
        }
        for define in strings(&spec, "definitions")? {
            let (name, value) = define
                .split_once('=')
                .map_or((define, None), |(name, value)| {
                    (name, Some(value.to_string()))
                });
            library.definitions.push((name.into(), value));
        }
        for package in strings(&spec, "packages")? {
            add_package(&mut library, &build.target, package)?;
        }
        for package in strings(&spec, "header_packages")? {
            headers_only(&mut library, package)?;
        }
        library.compile_options.extend(
            strings(&spec, "compile_options")?
                .into_iter()
                .map(String::from),
        );
        for flag in strings(&spec, "compile_options_if_supported")? {
            if build
                .compiler(&library, &build.output, false)
                .is_flag_supported(flag)
                .map_err(|e| e.to_string())?
            {
                library.compile_options.push(flag.into());
            }
        }
        library.link_options.extend(
            strings(&spec, "link_options")?
                .into_iter()
                .map(String::from),
        );
        for archive in strings(&spec, "archives")? {
            if !archives.contains_key(archive) {
                let path = if archive == "caption" {
                    crate::libobs::build_caption(build, &root)?
                } else {
                    crate::deps::shared_archive(build, &root, archive)?
                };
                archives.insert(archive.to_string(), path);
            }
            library.dependencies.push(archives[archive].clone());
        }
        result.push(Executable {
            name: name.into(),
            path: build.executable(&library)?,
        });
    }
    for directory in watched {
        println!("cargo:rerun-if-changed={}", directory.display());
    }
    Ok(result)
}

fn headers_only(library: &mut SharedLibrary, package: &str) -> Result<(), String> {
    let headers = pkg_config::Config::new()
        .cargo_metadata(false)
        .probe(package)
        .map_err(|e| e.to_string())?;
    library.include_dirs.extend(headers.include_paths);
    let mut definitions: Vec<_> = headers.defines.into_iter().collect();
    definitions.sort();
    library.definitions.extend(definitions);
    Ok(())
}

fn strings<'a>(spec: &'a serde_json::Value, key: &str) -> Result<Vec<&'a str>, String> {
    match spec.get(key) {
        None => Ok(vec![]),
        Some(value) => value
            .as_array()
            .ok_or_else(|| format!("C test {key} is not an array"))?
            .iter()
            .map(|item| {
                item.as_str()
                    .ok_or_else(|| format!("C test {key} entry is not a string"))
            })
            .collect(),
    }
}
