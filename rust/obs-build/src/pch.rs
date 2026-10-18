//! Build PCH through the same cc tool and flags used by its consumers.
use crate::NativeBuild;
use std::{
    fs,
    hash::{Hash, Hasher},
    path::Path,
};

pub(crate) fn apply(
    build: &NativeBuild,
    compiler: &mut cc::Build,
    cpp: bool,
    header: &Path,
) -> Result<(), String> {
    let tool = compiler.get_compiler();
    let kind = crate::platform::pch_kind(&build.target, &tool)?;
    let mut signature = std::collections::hash_map::DefaultHasher::new();
    (
        &build.host,
        &build.target,
        header,
        cpp,
        tool.path(),
        tool.args(),
        tool.env(),
    )
        .hash(&mut signature);
    let directory = build
        .output
        .join("pch")
        .join(format!("{:016x}", signature.finish()));
    fs::create_dir_all(&directory).map_err(|e| e.to_string())?;
    let directory = dunce::canonicalize(directory).map_err(|e| e.to_string())?;
    let source = directory.join("prefix-input.h");
    let path = dunce::simplified(header)
        .to_str()
        .ok_or("non-UTF8 PCH path")?
        .replace('\\', "/");
    if path.contains(['\n', '\r', '"']) {
        return Err(format!("unrepresentable PCH include: {path:?}"));
    }
    crate::frontend::write(&source, &format!("#include \"{path}\"\n"))?;
    println!("cargo:rerun-if-changed={}", header.display());
    let artifact = directory.join(match kind {
        crate::platform::PchKind::Gnu => "prefix.h.gch",
        crate::platform::PchKind::Clang => "prefix.pch",
    });
    let mut command = tool.to_command();
    command
        .args(["-x", if cpp { "c++-header" } else { "c-header" }, "-c"])
        .arg(&source)
        .arg("-o")
        .arg(&artifact);
    let result = command
        .output()
        .map_err(|e| format!("execute PCH compiler: {e}"))?;
    if !result.status.success() {
        return Err(format!(
            "PCH {} failed ({})\n{}\n{}",
            header.display(),
            result.status,
            String::from_utf8_lossy(&result.stdout),
            String::from_utf8_lossy(&result.stderr)
        ));
    }
    match kind {
        crate::platform::PchKind::Gnu => {
            let fallback = directory.join("prefix.h");
            // GCC otherwise ignores an unusable PCH silently. The fallback
            // header makes requested PCH consumption a checked invariant.
            crate::frontend::write(
                &fallback,
                "#error obs-build requested PCH was not consumed\n",
            )?;
            compiler
                .flag("-Werror=invalid-pch")
                .flag("-include")
                .flag(fallback.to_str().ok_or("non-UTF8 PCH fallback path")?);
        }
        crate::platform::PchKind::Clang => {
            compiler
                .flag("-include-pch")
                .flag(artifact.to_str().ok_or("non-UTF8 PCH artifact path")?);
        }
    }
    Ok(())
}
