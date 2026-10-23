//! Explicit Qt code generation for native Cargo targets.
use crate::SharedLibrary;
use std::{
    path::{Path, PathBuf},
    process::Command,
};

/// Generate one moc translation unit using the target's includes and definitions.
/// Tools come from explicit SDK inputs (see [`Tools::resolve`]).
pub fn moc(library: &mut SharedLibrary, header: &Path, output: &Path) -> Result<PathBuf, String> {
    let mut command = command("moc")?;
    for directory in &library.include_dirs {
        command.arg("-I").arg(directory);
    }
    for (name, value) in &library.definitions {
        command.arg(match value {
            Some(value) => format!("-D{name}={value}"),
            None => format!("-D{name}"),
        });
    }
    generate(command, header, output)?;
    library.sources.push(output.into());
    Ok(output.into())
}

/// Generate a UI header and put its output directory first in the include search.
pub fn uic(library: &mut SharedLibrary, form: &Path, output: &Path) -> Result<PathBuf, String> {
    generate(command("uic")?, form, output)?;
    let directory = output
        .parent()
        .ok_or("UI output has no parent directory")?
        .canonicalize()
        .map_err(|e| e.to_string())?;
    library.include_dirs.retain(|path| path != &directory);
    library.include_dirs.insert(0, directory);
    Ok(output.into())
}

/// Embed a resource collection and track each resource file as a Cargo input.
pub fn rcc(
    library: &mut SharedLibrary,
    resources: &Path,
    output: &Path,
) -> Result<PathBuf, String> {
    let resources = resources.canonicalize().map_err(|e| e.to_string())?;
    let mut list = command("rcc")?;
    list.arg("--list");
    let files = run(list, &resources)?;
    for file in std::str::from_utf8(&files)
        .map_err(|e| e.to_string())?
        .lines()
    {
        let path = resources
            .parent()
            .ok_or("resource input has no parent")?
            .join(file);
        println!("cargo:rerun-if-changed={}", path.display());
    }
    let name = resources
        .file_stem()
        .and_then(|name| name.to_str())
        .ok_or("invalid resource collection name")?;
    let mut compiler = command("rcc")?;
    compiler.arg("--name").arg(name);
    generate(compiler, &resources, output)?;
    library.sources.push(output.into());
    Ok(output.into())
}

fn command(name: &str) -> Result<Command, String> {
    let (host, target) = build_triples();
    let inputs = Inputs::from_env();
    let tools = Tools::resolve(&host, &target, &inputs)?;
    if let Some(prefix) = &inputs.target_prefix {
        // One check per build script: the generators match the target SDK.
        static CHECKED: std::sync::OnceLock<Result<(), String>> = std::sync::OnceLock::new();
        CHECKED
            .get_or_init(|| {
                tools.check_version(Sdk::open(&target, prefix, inputs.crt_static)?.version())
            })
            .clone()?;
    }
    Ok(Command::new(match name {
        "moc" => tools.moc,
        "uic" => tools.uic,
        _ => tools.rcc,
    }))
}

fn run(mut command: Command, input: &Path) -> Result<Vec<u8>, String> {
    let input = input.canonicalize().map_err(|e| e.to_string())?;
    println!("cargo:rerun-if-changed={}", input.display());
    let result = command
        .arg(&input)
        .output()
        .map_err(|e| format!("execute Qt tool: {e}"))?;
    if !result.status.success() {
        return Err(format!(
            "Qt tool {command:?} failed: {}",
            String::from_utf8_lossy(&result.stderr)
        ));
    }
    Ok(result.stdout)
}

fn generate(command: Command, input: &Path, output: &Path) -> Result<(), String> {
    let code = run(command, input)?;
    if code.is_empty() {
        return Err(format!("Qt tool produced no code for {}", input.display()));
    }
    if let Some(parent) = output.parent() {
        std::fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    }
    if std::fs::read(output).ok().as_deref() != Some(code.as_slice()) {
        std::fs::write(output, code).map_err(|e| e.to_string())?;
    }
    Ok(())
}

/// Combine generated metaobjects with the target's textual header closure.
/// This mirrors Qt's mocs_compilation.cpp without requiring precompiled headers.
pub fn combine_moc(
    library: &mut SharedLibrary,
    output: &Path,
    prelude: &Path,
) -> Result<(), String> {
    let directory = output.parent().ok_or("moc group output has no parent")?;
    let is_moc = |path: &PathBuf| {
        path.parent() == Some(directory)
            && path
                .file_name()
                .and_then(|name| name.to_str())
                .is_some_and(|name| name.starts_with("moc_"))
    };
    let sources: Vec<_> = library
        .sources
        .iter()
        .filter(|path| is_moc(path))
        .cloned()
        .collect();
    if sources.is_empty() {
        return Ok(());
    }
    let mut content = String::from("/* Generated Qt metaobject group. */\n");
    for path in std::iter::once(prelude.to_owned()).chain(sources) {
        let path = path.canonicalize().map_err(|e| e.to_string())?;
        let name = path
            .to_str()
            .ok_or("non-UTF8 moc include")?
            .replace('\\', "/");
        if name.contains(['\n', '\r', '"']) {
            return Err("moc include path contains a quote or newline".into());
        }
        content.push_str(&format!("#include \"{name}\"\n"));
    }
    if std::fs::read_to_string(output).ok().as_deref() != Some(&content) {
        std::fs::write(output, content).map_err(|e| e.to_string())?;
    }
    library.sources.retain(|path| !is_moc(path));
    library.sources.push(output.into());
    Ok(())
}

mod sdk;
pub use sdk::Sdk;

/// Explicit Qt SDK inputs. A target prefix selects the libraries Cargo links;
/// a host prefix (or per-tool override) selects the generators that run.
#[derive(Clone, Debug, Default)]
pub struct Inputs {
    pub target_prefix: Option<PathBuf>,
    pub host_prefix: Option<PathBuf>,
    pub moc: Option<PathBuf>,
    pub uic: Option<PathBuf>,
    pub rcc: Option<PathBuf>,
    /// The target links the static CRT (`crt-static`).
    pub crt_static: bool,
}

const VARIABLES: [&str; 6] = [
    "OBS_QT_TARGET_PREFIX",
    "OBS_QT_HOST_PREFIX",
    "OBS_QT_MOC",
    "OBS_QT_UIC",
    "OBS_QT_RCC",
    "CARGO_CFG_TARGET_FEATURE",
];

impl Inputs {
    pub fn from_lookup(lookup: impl Fn(&str) -> Option<std::ffi::OsString>) -> Self {
        let path = |name: &str| {
            lookup(name)
                .filter(|value| !value.is_empty())
                .map(PathBuf::from)
        };
        Self {
            target_prefix: path(VARIABLES[0]),
            host_prefix: path(VARIABLES[1]),
            moc: path(VARIABLES[2]),
            uic: path(VARIABLES[3]),
            rcc: path(VARIABLES[4]),
            crt_static: lookup(VARIABLES[5]).is_some_and(|features| {
                features
                    .to_string_lossy()
                    .split(',')
                    .any(|feature| feature == "crt-static")
            }),
        }
    }

    /// Read the inputs and register them with Cargo; nothing is written back.
    pub fn from_env() -> Self {
        for variable in VARIABLES {
            println!("cargo:rerun-if-env-changed={variable}");
        }
        Self::from_lookup(|name| std::env::var_os(name))
    }

    fn tool(&self, name: &str) -> Option<&PathBuf> {
        match name {
            "moc" => self.moc.as_ref(),
            "uic" => self.uic.as_ref(),
            _ => self.rcc.as_ref(),
        }
    }
}

/// Host executables bound to one Qt SDK.
#[derive(Clone, Debug)]
pub struct Tools {
    pub moc: PathBuf,
    pub uic: PathBuf,
    pub rcc: PathBuf,
}

impl Tools {
    /// Per-tool override, then the host SDK. A native build may use its target
    /// SDK, or the system Qt6Core libexecdir when no SDK is given; a cross build
    /// must name its host tools.
    pub fn resolve(host: &str, target: &str, inputs: &Inputs) -> Result<Self, String> {
        let prefix = inputs
            .host_prefix
            .as_ref()
            .or(inputs.target_prefix.as_ref().filter(|_| host == target));
        let tool = |name: &str| -> Result<PathBuf, String> {
            let path = if let Some(path) = inputs.tool(name) {
                path.clone()
            } else if let Some(prefix) = prefix {
                crate::platform::qt_tool(host, prefix, name)
            } else if host != target {
                return Err(format!(
                    "cross build {host} -> {target} needs OBS_QT_HOST_PREFIX or OBS_QT_{} for Qt host tools",
                    name.to_ascii_uppercase()
                ));
            } else {
                return Ok(PathBuf::from(
                    pkg_config::get_variable("Qt6Core", "libexecdir").map_err(|e| e.to_string())?,
                )
                .join(name));
            };
            if !path.is_file() {
                return Err(format!("Qt host tool {} is missing", path.display()));
            }
            Ok(path)
        };
        Ok(Self {
            moc: tool("moc")?,
            uic: tool("uic")?,
            rcc: tool("rcc")?,
        })
    }

    /// Generators must come from the same Qt release as the target SDK.
    pub fn check_version(&self, version: &str) -> Result<(), String> {
        for tool in [&self.moc, &self.uic, &self.rcc] {
            let output = Command::new(tool)
                .arg("--version")
                .output()
                .map_err(|error| format!("{}: {error}", tool.display()))?;
            let reported = String::from_utf8_lossy(&output.stdout);
            if reported.split_whitespace().last() != Some(version) {
                return Err(format!(
                    "Qt host tool {} reports {:?}; the target SDK is Qt {version}",
                    tool.display(),
                    reported.trim()
                ));
            }
        }
        Ok(())
    }
}

/// Resolve a Qt6 package from explicit inputs; `None` leaves a native build on
/// its system package path.
pub fn package(
    host: &str,
    target: &str,
    package: &str,
    inputs: &Inputs,
) -> Result<Option<crate::validation::graph::Dependency>, String> {
    let Some(module) = package
        .strip_prefix("Qt6")
        .filter(|module| module.starts_with(|c: char| c.is_ascii_uppercase()))
    else {
        return Ok(None);
    };
    let Some(prefix) = &inputs.target_prefix else {
        if host != target {
            return Err(format!(
                "cross build {host} -> {target} needs OBS_QT_TARGET_PREFIX for {package}; the host's pkg-config would select host libraries"
            ));
        }
        return Ok(None);
    };
    Sdk::open(target, prefix, inputs.crt_static)?
        .interface(module)
        .map(Some)
}

/// Host and target of the current build script, or of this build engine.
pub(crate) fn build_triples() -> (String, String) {
    let triple = |name, fallback: &str| std::env::var(name).unwrap_or_else(|_| fallback.into());
    (
        triple("HOST", crate::BUILD_HOST),
        triple("TARGET", crate::BUILD_TARGET),
    )
}
