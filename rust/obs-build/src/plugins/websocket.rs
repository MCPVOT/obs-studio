//! Original obs-websocket plugin, including its Qt interfaces and resources.
use super::{Module, finish, module};
use crate::{NativeBuild, frontend, libobs::add_package};
use std::path::Path;

pub struct Options {
    pub unity: bool,
}

impl Default for Options {
    fn default() -> Self {
        Self { unity: true }
    }
}

pub fn build(
    build: &NativeBuild,
    root: &Path,
    core: &Path,
    api: &Path,
    options: &Options,
) -> Result<Module, String> {
    let root = root.canonicalize().map_err(|e| e.to_string())?;
    println!(
        "cargo:rerun-if-changed={}",
        root.join("plugins/obs-websocket").display()
    );
    let manifest: serde_json::Value =
        serde_json::from_str(include_str!("../../manifests/obs-websocket.json"))
            .map_err(|e| e.to_string())?;
    let mut library = module(build, &root, core, "obs-websocket")?;
    library.sources.clear();
    library.unity = options.unity;
    library.dependencies.push(api.into());
    library.include_dirs.extend([
        root.join("frontend/api"),
        root.join("plugins/obs-websocket/src"),
        root.join("plugins/obs-websocket/lib"),
    ]);
    crate::platform::configure_websocket(&build.target, &mut library)?;
    for (package, minimum) in [("nlohmann_json", "3.11"), ("asio", "1.12.1")] {
        pkg_config::Config::new()
            .cargo_metadata(false)
            .atleast_version(minimum)
            .probe(package)
            .map_err(|e| format!("obs-websocket requires {package} >= {minimum}: {e}"))?;
        add_package(&mut library, &build.target, package)?;
    }
    for package in ["Qt6Widgets", "Qt6Svg", "Qt6Network"] {
        add_package(&mut library, &build.target, package)?;
    }
    library.definitions.push(("ASIO_STANDALONE".into(), None));
    let generated = build.output.join("obs-websocket/qt");
    std::fs::create_dir_all(&generated).map_err(|e| e.to_string())?;
    library.include_dirs.insert(0, generated.clone());
    let template = root.join("plugins/obs-websocket/src/plugin-macros.h.in");
    println!("cargo:rerun-if-changed={}", template.display());
    let content = std::fs::read_to_string(&template)
        .map_err(|e| e.to_string())?
        .replace(
            "@obs-websocket_VERSION@",
            manifest["version"]
                .as_str()
                .ok_or("websocket version missing")?,
        )
        .replace(
            "@OBS_WEBSOCKET_RPC_VERSION@",
            &manifest["rpc_version"].to_string(),
        );
    frontend::write(&generated.join("plugin-macros.generated.h"), &content)?;
    let inputs: Vec<String> =
        serde_json::from_value(manifest["inputs"].clone()).map_err(|e| e.to_string())?;
    let inputs: Vec<String> = inputs
        .iter()
        .map(|input| format!("plugins/obs-websocket/{input}"))
        .collect();
    frontend::add_inputs(build, &root, &mut library, &inputs)?;
    finish(build, &root, library)
}
