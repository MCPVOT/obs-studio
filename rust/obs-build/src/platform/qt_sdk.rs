//! Target rules for explicit Qt SDKs: binary formats, CRT, linking and loading.
use std::{
    collections::BTreeMap,
    path::{Path, PathBuf},
};

/// Architecture names shared by target triples and SDK binary headers.
pub fn qt_architecture(target: &str) -> Result<&'static str, String> {
    let architecture = target.split('-').next().unwrap_or_default();
    Ok(match architecture {
        "x86_64" => "x86_64",
        "aarch64" | "arm64" => "aarch64",
        "i386" | "i586" | "i686" => "x86",
        _ => return Err(format!("unsupported Qt target architecture {architecture}")),
    })
}

/// Reject SDK/target pairs Cargo cannot link, before any compilation.
pub fn qt_check_target(target: &str, crt_static: bool) -> Result<(), String> {
    if target.contains("windows") && !target.contains("msvc") {
        return Err(format!(
            "explicit Qt SDKs support MSVC Windows targets; {target} is unsupported"
        ));
    }
    if !(target.contains("windows") || target.contains("apple-darwin") || target.contains("linux"))
    {
        return Err(format!("explicit Qt SDKs do not support target {target}"));
    }
    if target.contains("msvc") && crt_static {
        return Err(
            "the crt-static target feature links the static CRT; a shared Qt SDK needs the dynamic CRT"
                .into(),
        );
    }
    Ok(())
}

/// Whether an SDK configuration's CRT matches Cargo's. Rust's MSVC targets
/// always link the release CRT (/MD), and Qt's DEBUG configuration uses /MDd.
pub fn qt_configuration_usable(target: &str, configuration: &str) -> bool {
    !(target.contains("msvc") && configuration == "DEBUG")
}

/// The compiler identity Qt's exported generator expressions test.
pub fn qt_compiler_id(target: &str) -> &'static str {
    if target.contains("msvc") {
        "MSVC"
    } else if target.contains("apple") {
        "AppleClang"
    } else {
        "GNU"
    }
}

/// Architectures contained in an SDK binary in the target's native format.
pub fn qt_binary_architectures(target: &str, bytes: &[u8]) -> Result<Vec<&'static str>, String> {
    let u16le = |at: usize| {
        bytes
            .get(at..at + 2)
            .map(|b| u16::from_le_bytes([b[0], b[1]]))
    };
    let u32le = |at: usize| {
        bytes
            .get(at..at + 4)
            .map(|b| u32::from_le_bytes([b[0], b[1], b[2], b[3]]))
    };
    let u32be = |at: usize| {
        bytes
            .get(at..at + 4)
            .map(|b| u32::from_be_bytes([b[0], b[1], b[2], b[3]]))
    };
    if target.contains("windows") {
        let header = u32le(0x3c).ok_or("truncated PE header")? as usize;
        if !bytes.starts_with(b"MZ") || bytes.get(header..header + 4) != Some(b"PE\0\0") {
            return Err("not a PE image".into());
        }
        return Ok(match u16le(header + 4).ok_or("truncated PE header")? {
            0x8664 => vec!["x86_64"],
            0xaa64 => vec!["aarch64"],
            0x014c => vec!["x86"],
            _ => vec!["unknown"],
        });
    }
    if target.contains("apple") {
        let cpu = |value: u32| match value {
            0x0100_0007 => "x86_64",
            0x0100_000c => "aarch64",
            7 => "x86",
            _ => "unknown",
        };
        return match (u32be(0), u32le(0)) {
            (Some(0xcafe_babe), _) => {
                let count = u32be(4).ok_or("truncated Mach-O header")? as usize;
                (0..count)
                    .map(|index| {
                        u32be(8 + index * 20)
                            .map(cpu)
                            .ok_or_else(|| "truncated universal Mach-O header".into())
                    })
                    .collect()
            }
            (_, Some(0xfeed_facf | 0xfeed_face)) => {
                Ok(vec![cpu(u32le(4).ok_or("truncated Mach-O header")?)])
            }
            _ => Err("not a Mach-O image".into()),
        };
    }
    if !bytes.starts_with(b"\x7fELF") || bytes.get(5) != Some(&1) {
        return Err("not a little-endian ELF image".into());
    }
    Ok(match u16le(18).ok_or("truncated ELF header")? {
        62 => vec!["x86_64"],
        183 => vec!["aarch64"],
        3 => vec!["x86"],
        _ => vec!["unknown"],
    })
}

/// The file a consumer passes to the linker for one Qt module.
pub fn qt_link_artifact(
    target: &str,
    location: &Path,
    import_library: Option<&Path>,
) -> Result<PathBuf, String> {
    if target.contains("windows") {
        return import_library
            .map(Path::to_path_buf)
            .ok_or_else(|| format!("Qt module {} has no import library", location.display()));
    }
    Ok(location.into())
}

/// Link arguments for a system library named in Qt's exported interface.
pub fn qt_system_library(target: &str, name: &str) -> String {
    if target.contains("windows") {
        format!("{name}.lib")
    } else {
        format!("-l{name}")
    }
}

/// Link arguments for the non-Qt imported targets in Qt's exported interface.
pub fn qt_imported_target(target: &str, name: &str) -> Result<Vec<String>, String> {
    match name {
        // Qt's Threads::Threads uses the pthread flag (THREADS_PREFER_PTHREAD_FLAG).
        "Threads::Threads" if target.contains("windows") => Ok(vec![]),
        "Threads::Threads" => Ok(vec!["-pthread".into()]),
        // Atomics and OpenGL are dependencies of the shared Qt libraries
        // themselves, recorded in their own dynamic dependency lists.
        "WrapAtomic::WrapAtomic" | "WrapOpenGL::WrapOpenGL" => Ok(vec![]),
        _ => Err(format!("unsupported Qt interface dependency {name}")),
    }
}

/// Loader inputs for a linked module: an rpath on Unix, PATH on Windows.
pub fn qt_runtime_directory(
    target: &str,
    location: &Path,
    link_options: &mut Vec<String>,
    runtime: &mut BTreeMap<String, Vec<String>>,
) -> Result<(), String> {
    let mut directory = location.parent().ok_or("Qt module has no directory")?;
    if target.contains("apple") {
        // @rpath/QtCore.framework/... resolves against the frameworks' parent.
        directory = location
            .ancestors()
            .find(|path| path.extension().is_some_and(|ext| ext == "framework"))
            .and_then(Path::parent)
            .ok_or_else(|| format!("{} is not inside a framework", location.display()))?;
    }
    let directory = directory.display().to_string();
    if target.contains("windows") {
        let paths = runtime.entry("PATH".into()).or_default();
        if !paths.contains(&directory) {
            paths.push(directory);
        }
    } else {
        let rpath = format!("-Wl,-rpath,{directory}");
        if !link_options.contains(&rpath) {
            link_options.push(rpath);
        }
    }
    Ok(())
}

/// Where an SDK keeps its generators on a given host.
pub fn qt_tool(host: &str, prefix: &Path, tool: &str) -> PathBuf {
    if host.contains("windows") {
        prefix.join("bin").join(format!("{tool}.exe"))
    } else {
        prefix.join("libexec").join(tool)
    }
}
