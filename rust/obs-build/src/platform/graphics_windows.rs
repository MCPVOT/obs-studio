//! Windows graphics compiler, SDK linkage and version resources.
use crate::{NativeBuild, SharedLibrary, libobs::Options};
use std::path::Path;

fn configure(
    build: &NativeBuild,
    root: &Path,
    options: &Options,
    library: &mut SharedLibrary,
) -> Result<(), String> {
    super::libobs_windows::defaults(library);
    library
        .cpp_compile_options
        .extend(["/std:c++17".into(), "/EHsc".into()]);
    let dependencies = options
        .native_dependencies
        .as_ref()
        .ok_or("Windows graphics requires an explicit OBS dependency bundle")?;
    if !dependencies.join("include/simde/x86/sse2.h").is_file() {
        return Err("Windows graphics dependency bundle lacks simde".into());
    }
    library
        .include_dirs
        .extend([dependencies.join("include"), root.join("deps/w32-pthreads")]);
    let name = library
        .file_name
        .as_ref()
        .ok_or("missing graphics DLL name")?;
    let stem = Path::new(name)
        .file_stem()
        .ok_or("missing graphics DLL filename stem")?
        .to_string_lossy();
    super::libobs_windows::add_resource(
        root,
        &format!("{stem}/cmake/windows/obs-module.rc.in"),
        &build.output.join(format!("config/{stem}.rc")),
        library,
    )?;
    Ok(())
}

pub fn d3d11(
    build: &NativeBuild,
    root: &Path,
    options: &Options,
    library: &mut SharedLibrary,
    gpu_priority: i32,
) -> Result<(), String> {
    library.file_name = Some("libobs-d3d11.dll".into());
    configure(build, root, options, library)?;
    library
        .definitions
        .push(("GPU_PRIORITY_VAL".into(), Some(gpu_priority.to_string())));
    if gpu_priority != 0 {
        library.definitions.push(("USE_GPU_PRIORITY".into(), None));
    }
    library.link_options.extend(
        [
            "d3d9.lib",
            "d3d11.lib",
            "d3dcompiler.lib",
            "dxgi.lib",
            "shcore.lib",
            "user32.lib",
            "gdi32.lib",
        ]
        .map(String::from),
    );
    Ok(())
}

pub fn opengl(
    build: &NativeBuild,
    root: &Path,
    options: &Options,
    library: &mut SharedLibrary,
) -> Result<(), String> {
    library.file_name = Some("libobs-opengl.dll".into());
    configure(build, root, options, library)?;
    crate::graphics::append_sources(root, "OPENGL_WINDOWS", library)?;
    library
        .link_options
        .extend(["opengl32.lib", "gdi32.lib", "user32.lib"].map(String::from));
    Ok(())
}

pub fn glad(build: &NativeBuild, root: &Path, library: &mut SharedLibrary) -> Result<(), String> {
    super::libobs_windows::defaults(library);
    // The generated GL and WGL loaders share a tentative C global. Match
    // MSVC's common-symbol behavior when using clang-cl's modern default.
    let compiler = build
        .compiler(library, &build.output, false)
        .try_get_compiler()
        .map_err(|e| e.to_string())?;
    if compiler.is_like_clang_cl() {
        library.c_compile_options.push("/clang:-fcommon".into());
    }
    crate::graphics::append_sources(root, "GLAD_WINDOWS", library)
}
