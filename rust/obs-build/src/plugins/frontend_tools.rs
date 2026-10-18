//! Qt frontend tools and optional scripting interface.
use super::{Module, finish, module};
use crate::{NativeBuild, frontend, scripting};
use std::path::Path;

pub fn build(
    build: &NativeBuild,
    root: &Path,
    core: &Path,
    api: &Path,
    scripting: Option<&scripting::Artifacts>,
) -> Result<Module, String> {
    let root = root.canonicalize().map_err(|e| e.to_string())?;
    let mut library = module(build, &root, core, "frontend-tools")?;
    library.sources = ["core+.hpp.cpp", "c+.h.c", "output-timer+.hpp.cpp"]
        .iter()
        .map(|group| {
            root.join(format!(
                "plugins/frontend-tools/build/frontend-tools.{group}"
            ))
        })
        .collect();
    library.include_dirs.push(root.join("frontend/api"));
    library
        .dependencies
        .push(api.canonicalize().map_err(|e| e.to_string())?);
    crate::platform::configure_frontend_tools(&build.target, &root, &mut library)?;
    crate::libobs::add_package(&mut library, &build.target, "Qt6Widgets")?;
    let manifest: serde_json::Value = serde_json::from_slice(
        &std::fs::read(root.join("frontend/build/cargo-sources.json"))
            .map_err(|e| e.to_string())?,
    )
    .map_err(|e| e.to_string())?;
    let interfaces: std::collections::BTreeMap<String, Vec<String>> =
        serde_json::from_value(manifest["interfaces"].clone()).map_err(|e| e.to_string())?;
    // These are the same shared Qt interfaces compiled into the application.
    for (directory, inputs) in interfaces
        .iter()
        .filter(|(directory, _)| directory.as_str() != "shared/qt/idian")
    {
        library.include_dirs.push(root.join(directory));
        frontend::add_inputs(build, &root, &mut library, inputs)?;
    }
    frontend::add_spinbox(build, &root, &mut library)?;
    let mut inputs: Vec<String> = [
        "auto-scene-switcher.hpp",
        "output-timer.hpp",
        "forms/auto-scene-switcher.ui",
        "forms/captions.ui",
        "forms/output-timer.ui",
        "forms/scripts.ui",
    ]
    .iter()
    .map(|name| format!("plugins/frontend-tools/{name}"))
    .collect();
    if let Some(scripting) = scripting {
        library.dependencies.push(scripting.library.clone());
        library.include_dirs.push(root.join("shared/obs-scripting"));
        library.definitions.push(("ENABLE_SCRIPTING".into(), None));
        if scripting.lua_module.is_some() {
            library.definitions.push(("LUAJIT_FOUND".into(), None));
        }
        if scripting.python_module.is_some() {
            library.definitions.push(("Python_FOUND".into(), None));
        }
        inputs.extend([
            "plugins/frontend-tools/build/frontend-tools.scripts+.hpp.cpp".into(),
            "plugins/frontend-tools/scripts.hpp".into(),
        ]);
    }
    frontend::add_inputs(build, &root, &mut library, &inputs)?;
    // Moc sees the same complete type definitions as the original groups.
    let mut context = String::new();
    for header in [
        "plugins/frontend-tools/build/frontend-tools.pch.hpp",
        "plugins/frontend-tools/build/frontend-tools-output-timer.pch.hpp",
        "shared/properties-view/build/properties-view.pch.hpp",
        "shared/qt/obs-spinbox/build/qt-obs-spinbox.pch.hpp",
    ] {
        context.push_str(&format!("#include \"{header}\"\n"));
    }
    if scripting.is_some() {
        context
            .push_str("#include \"plugins/frontend-tools/build/frontend-tools-scripts.pch.hpp\"\n");
    }
    let generated = build.output.join("frontend-tools/qt");
    let prefix = generated.join("moc-context.hpp");
    frontend::write(&prefix, &context)?;
    crate::qt::combine_moc(
        &mut library,
        &generated.join("mocs_compilation.cpp"),
        &prefix,
    )?;
    finish(build, &root, library)
}
