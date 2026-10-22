//! Graphics backends built from the shared native OBS configuration.
use crate::{NativeBuild, SharedLibrary, libobs::Options};
use std::path::{Path, PathBuf};

pub fn build_d3d11(
    build: &NativeBuild,
    root: &Path,
    core: &Path,
    options: &Options,
    gpu_priority: i32,
) -> Result<PathBuf, String> {
    let root = root.canonicalize().map_err(|e| e.to_string())?;
    let mut library = SharedLibrary {
        name: "libobs-d3d11".into(),
        include_dirs: vec![
            root.clone(),
            root.join("libobs"),
            root.join("libobs-d3d11"),
            build.output.join("config"),
        ],
        dependencies: vec![core.canonicalize().map_err(|e| e.to_string())?],
        ..SharedLibrary::default()
    };
    append_sources(&root, "D3D11_CORE", &mut library)?;
    crate::platform::configure_d3d11(build, &root, options, &mut library, gpu_priority)?;
    configure_groups(build, &root, options, &mut library)?;
    build.shared_library(&library)
}

pub fn build_opengl(
    build: &NativeBuild,
    root: &Path,
    core: &Path,
    options: &Options,
) -> Result<PathBuf, String> {
    let root = root.canonicalize().map_err(|e| e.to_string())?;
    let mut library = SharedLibrary {
        name: "obs-opengl".into(),
        include_dirs: vec![
            root.clone(),
            root.join("libobs"),
            root.join("libobs-opengl"),
            root.join("deps/glad/include"),
            build.output.join("config"),
        ],
        dependencies: vec![core.canonicalize().map_err(|e| e.to_string())?],
        ..SharedLibrary::default()
    };
    append_sources(&root, "OPENGL_CORE", &mut library)?;
    crate::platform::configure_opengl(build, &root, options, &mut library)?;
    configure_groups(build, &root, options, &mut library)?;
    library.dependencies.push(build_glad(build, &root)?);
    let artifact = build.shared_library(&library)?;
    let linker_name = artifact.with_file_name(crate::platform::shared_filename(
        &build.target,
        "obs-opengl",
    ));
    if artifact != linker_name {
        std::fs::copy(&artifact, &linker_name).map_err(|e| e.to_string())?;
    }
    Ok(artifact)
}

pub(crate) fn build_glad(build: &NativeBuild, root: &Path) -> Result<PathBuf, String> {
    let mut glad = SharedLibrary {
        name: "obsglad".into(),
        include_dirs: vec![root.join("deps/glad/include")],
        ..SharedLibrary::default()
    };
    append_sources(root, "GLAD_CORE", &mut glad)?;
    crate::platform::configure_glad(build, root, &mut glad)?;
    build.static_library(&glad)
}

pub(crate) fn append_sources(
    root: &Path,
    name: &str,
    library: &mut SharedLibrary,
) -> Result<(), String> {
    let manifest = root.join("cmake/graphics-source-sets.json");
    println!("cargo:rerun-if-changed={}", manifest.display());
    let sets: std::collections::BTreeMap<String, Vec<String>> =
        serde_json::from_slice(&std::fs::read(&manifest).map_err(|e| e.to_string())?)
            .map_err(|e| e.to_string())?;
    let sources = sets
        .get(name)
        .ok_or_else(|| format!("missing graphics source set {name}"))?;
    library
        .sources
        .extend(sources.iter().map(|source| root.join(source)));
    Ok(())
}

fn configure_groups(
    build: &NativeBuild,
    root: &Path,
    options: &Options,
    library: &mut SharedLibrary,
) -> Result<(), String> {
    if !options.unity {
        build.separate_target_sources(root, library)?;
    }
    if options.pch {
        library.precompiled_headers = crate::groups::headers(root, &library.sources)?;
    }
    Ok(())
}
