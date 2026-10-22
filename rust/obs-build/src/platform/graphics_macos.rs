//! macOS graphics compiler settings, frameworks and Swift/Metal module.
use crate::{NativeBuild, SharedLibrary, SourceCompileOptions, SwiftLibrary, libobs::Options};
use std::path::{Path, PathBuf};

fn dependency_include(options: &Options) -> Result<PathBuf, String> {
    let dependencies = options
        .native_dependencies
        .as_ref()
        .ok_or("macOS graphics requires an explicit OBS dependency bundle")?;
    let include = dependencies.join("include");
    if !include.join("simde/x86/sse2.h").is_file() {
        return Err("macOS graphics dependency bundle lacks simde".into());
    }
    include.canonicalize().map_err(|e| e.to_string())
}

pub fn opengl(root: &Path, options: &Options, library: &mut SharedLibrary) -> Result<(), String> {
    library.file_name = Some("libobs-opengl.dylib".into());
    library.include_dirs.push(dependency_include(options)?);
    library
        .definitions
        .push(("GL_SILENCE_DEPRECATION".into(), None));
    library.c_compile_options.push("-std=gnu17".into());
    library.compile_options.extend(
        [
            "-fno-strict-aliasing",
            "-fvisibility=hidden",
            "-mmacosx-version-min=13.0",
            "-Wno-strict-prototypes",
        ]
        .map(String::from),
    );
    let cocoa = crate::graphics::source_set(root, "OPENGL_MACOS")?;
    library.source_compile_options.push(SourceCompileOptions {
        sources: cocoa.clone(),
        options: vec!["-fno-objc-arc".into()],
    });
    library.sources.extend(cocoa);
    for framework in ["Cocoa", "IOSurface", "OpenGL"] {
        library
            .link_options
            .extend(["-framework".into(), framework.into()]);
    }
    library.link_options.extend([
        "-Wl,-compatibility_version,1.0".into(),
        "-Wl,-current_version,33".into(),
    ]);
    Ok(())
}

pub fn glad(library: &mut SharedLibrary) {
    library.c_compile_options.push("-std=gnu17".into());
    library.compile_options.extend(
        [
            "-fvisibility=hidden",
            "-mmacosx-version-min=13.0",
            "-Wno-strict-prototypes",
        ]
        .map(String::from),
    );
}

pub fn metal(
    build: &NativeBuild,
    root: &Path,
    core: &Path,
    options: &Options,
) -> Result<PathBuf, String> {
    let root = root.canonicalize().map_err(|e| e.to_string())?;
    let mut bridges = crate::graphics::source_set(&root, "METAL_BRIDGE")?;
    if bridges.len() != 1 {
        return Err("Metal requires exactly one canonical bridging header".into());
    }
    let bridging_header = bridges.remove(0);
    build.swift_shared_library(&SwiftLibrary {
        name: "libobs-metal".into(),
        module_name: "libobs_metal".into(),
        file_name: "libobs-metal.dylib".into(),
        sources: crate::graphics::source_set(&root, "METAL_SWIFT")?,
        bridging_header,
        input_files: vec![
            root.join("libobs"),
            build.output.join("config"),
            dependency_include(options)?,
        ],
        include_dirs: vec![
            root.clone(),
            root.join("libobs"),
            build.output.join("config"),
            dependency_include(options)?,
        ],
        definitions: vec![],
        dependencies: vec![core.canonicalize().map_err(|e| e.to_string())?],
        deployment_target: "13.0".into(),
        compile_options: vec!["-swift-version".into(), "6".into()],
        link_options: vec![
            "-Xlinker".into(),
            "-compatibility_version".into(),
            "-Xlinker".into(),
            "1.0".into(),
            "-Xlinker".into(),
            "-current_version".into(),
            "-Xlinker".into(),
            "33".into(),
        ],
    })
}
