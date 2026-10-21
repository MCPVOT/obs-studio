//! Full frontend runtime validation with an isolated profile and display.
use obs_build::{NativeBuild, SharedLibrary, plugins, scripting};
use std::path::{Path, PathBuf};

pub struct Inputs<'a> {
    pub root: &'a Path,
    pub build: &'a NativeBuild,
    pub executable: &'a Path,
    pub api: &'a Path,
    pub renderer: &'a Path,
    pub ffmpeg: &'a plugins::FfmpegArtifacts,
    pub nvenc_helper: Option<&'a Path>,
    pub modules: &'a [plugins::Module],
    pub websocket: Option<&'a plugins::Module>,
    pub browser: Option<&'a plugins::BrowserArtifacts>,
    pub frontend_tools: Option<&'a plugins::Module>,
    pub script_artifacts: Option<&'a scripting::Artifacts>,
    pub gui_probe: Option<&'a plugins::Module>,
}

pub struct Variant<'a> {
    pub core: &'a Path,
    pub nvenc: Option<&'a plugins::Module>,
}

pub fn exercise(inputs: Inputs<'_>, variants: &[Variant<'_>]) -> Result<(), String> {
    let inspector = inspector(inputs.build)?;
    for (index, variant) in variants.iter().enumerate() {
        assert!(
            variants[..index]
                .iter()
                .all(|previous| previous.core != variant.core),
            "application compatibility variants must select distinct libraries"
        );
        let core = variant.core;
        // Retain this owned runtime, profile, log and screenshot as evidence.
        let runtime = tempfile::Builder::new()
            .prefix("app-runtime-")
            .tempdir_in(&inputs.build.output)
            .map_err(|e| e.to_string())?
            .keep();
        let bin = runtime.join("bin");
        let libraries = runtime.join("lib64");
        std::fs::create_dir_all(&bin).map_err(|e| e.to_string())?;
        std::fs::create_dir_all(&libraries).map_err(|e| e.to_string())?;
        copy(inputs.executable, &bin.join("obs-studio-rust"))?;
        for library in [core, inputs.api, inputs.renderer] {
            copy(
                library,
                &libraries.join(
                    library
                        .file_name()
                        .ok_or("runtime library has no filename")?,
                ),
            )?;
        }
        inputs.ffmpeg.deploy_muxer(&bin)?;
        if let Some(scripts) = inputs.script_artifacts {
            let library_name = scripts
                .library
                .file_name()
                .ok_or("scripting library has no filename")?;
            copy(&scripts.library, &libraries.join(library_name))?;
            let mut modules = Vec::new();
            let mut languages = Vec::new();
            for (language, module) in [
                ("lua", scripts.lua_module.as_deref()),
                ("python", scripts.python_module.as_deref()),
            ] {
                if let Some(module) = module {
                    let relative = module
                        .strip_prefix(&scripts.runtime)
                        .map_err(|e| e.to_string())?;
                    copy(module, &runtime.join(relative))?;
                    modules.push(relative.to_string_lossy().into_owned());
                    languages.push(language);
                }
            }
            if let Some(wrapper) = &scripts.python_wrapper {
                copy(
                    wrapper,
                    &runtime.join(
                        wrapper
                            .strip_prefix(&scripts.runtime)
                            .map_err(|e| e.to_string())?,
                    ),
                )?;
            }
            let metadata = serde_json::json!({"library": format!("lib64/{}", library_name.to_string_lossy()), "modules": modules, "languages": languages});
            std::fs::write(
                runtime.join("expected-scripting.json"),
                serde_json::to_vec(&metadata).map_err(|e| e.to_string())?,
            )
            .map_err(|e| e.to_string())?;
        }
        if let Some(helper) = inputs.nvenc_helper {
            copy(
                helper,
                &bin.join(helper.file_name().ok_or("NVENC helper has no filename")?),
            )?;
        }
        copy_tree(
            &inputs.root.join("libobs/data"),
            &runtime.join("share/obs/libobs"),
        )?;
        copy_tree(
            &inputs.root.join("frontend/data"),
            &runtime.join("share/obs/obs-studio"),
        )?;
        copy(
            &inputs.root.join("AUTHORS"),
            &runtime.join("share/obs/obs-studio/authors"),
        )?;
        let mut names = Vec::new();
        for module in inputs
            .modules
            .iter()
            .chain(variant.nvenc)
            .chain(inputs.frontend_tools)
            .chain(inputs.websocket)
            .chain(inputs.browser.map(|browser| &browser.module))
            .chain(inputs.gui_probe)
        {
            let name = module
                .library
                .file_stem()
                .and_then(|name| name.to_str())
                .ok_or("invalid module filename")?;
            let target = runtime.join("lib64/obs-modules/core");
            std::fs::create_dir_all(&target).map_err(|e| e.to_string())?;
            copy(
                &module.library,
                &target.join(module.library.file_name().ok_or("module has no filename")?),
            )?;
            copy_tree(
                &module.data,
                &runtime.join("share/obs/obs-modules/core").join(name),
            )?;
            names.push(name);
        }
        std::fs::write(
            runtime.join("expected-modules.json"),
            serde_json::to_vec(&names).map_err(|e| e.to_string())?,
        )
        .map_err(|e| e.to_string())?;
        if let Some(browser) = inputs.browser {
            browser.deploy_runtime(&runtime.join("lib64/obs-modules/core"))?;
        }
        // D-Bus activated services can inherit standard streams after the
        // launcher exits. Files let us wait for the owned launcher without
        // waiting indefinitely for every inherited pipe writer to disappear.
        let probe_log = runtime.join("application-probe.log");
        let log = std::fs::File::create(&probe_log).map_err(|e| e.to_string())?;
        let errors = log.try_clone().map_err(|e| e.to_string())?;
        let result = std::process::Command::new("dbus-run-session")
            .args(["--", "uv", "run", "--no-project", "python"])
            .arg(
                inputs
                    .root
                    .join("rust/obs-build/tests/libobs/application_probe.py"),
            )
            .arg("--runtime")
            .arg(&runtime)
            .arg("--inspector")
            .arg(&inspector)
            .stdout(log)
            .stderr(errors)
            .status()
            .map_err(|e| e.to_string())?;
        if !result.success() {
            return Err(format!(
                "{} GUI runtime {} failed: {}\n{}",
                core.display(),
                runtime.display(),
                result,
                std::fs::read_to_string(&probe_log).map_err(|e| e.to_string())?
            ));
        }
        println!(
            "OBS GUI runtime passed for {}: {}",
            core.display(),
            runtime.display()
        );
    }
    Ok(())
}

fn copy(source: &Path, target: &Path) -> Result<(), String> {
    if let Some(parent) = target.parent() {
        std::fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    }
    std::fs::copy(source, target)
        .map(|_| ())
        .map_err(|e| format!("copy {} to {}: {e}", source.display(), target.display()))
}

pub(super) fn copy_tree(source: &Path, target: &Path) -> Result<(), String> {
    std::fs::create_dir_all(target).map_err(|e| e.to_string())?;
    for entry in std::fs::read_dir(source).map_err(|e| e.to_string())? {
        let entry = entry.map_err(|e| e.to_string())?;
        let destination = target.join(entry.file_name());
        if entry.path().is_dir() {
            copy_tree(&entry.path(), &destination)?;
        } else {
            copy(&entry.path(), &destination)?;
        }
    }
    Ok(())
}

fn inspector(build: &NativeBuild) -> Result<PathBuf, String> {
    let source = build.output.join("application-window-probe.c");
    if std::fs::read_to_string(&source).ok().as_deref() != Some(WINDOW_PROBE) {
        std::fs::write(&source, WINDOW_PROBE).map_err(|e| e.to_string())?;
    }
    let mut target = SharedLibrary {
        name: "application-window-probe".into(),
        sources: vec![source],
        ..SharedLibrary::default()
    };
    let x11 = pkg_config::Config::new()
        .cargo_metadata(false)
        .probe("x11")
        .map_err(|e| e.to_string())?;
    target.include_dirs.extend(x11.include_paths);
    for path in x11.link_paths {
        target.link_options.extend([
            format!("-L{}", path.display()),
            format!("-Wl,-rpath,{}", path.display()),
        ]);
    }
    target
        .link_options
        .extend(x11.libs.iter().map(|name| format!("-l{name}")));
    build.executable(&target)
}

const WINDOW_PROBE: &str = r#"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ignore_error(Display *display, XErrorEvent *error) { (void)display; (void)error; return 0; }

static int matches(Display *display, Window window, unsigned long pid)
{
    Atom type;
    int format;
    unsigned long length, remaining;
    unsigned char *value = NULL;
    if (XGetWindowProperty(display, window, XInternAtom(display, "_NET_WM_PID", False), 0, 1, False,
            XA_CARDINAL, &type, &format, &length, &remaining, &value) != Success || !value)
        return 0;
    int same = format == 32 && length == 1 && *(unsigned long *)value == pid;
    XFree(value);
    if (!same)
        return 0;
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(display, window, &attributes) || attributes.map_state != IsViewable ||
        attributes.width < 300 || attributes.height < 200)
        return 0;
    value = NULL;
    if (XGetWindowProperty(display, window, XInternAtom(display, "_NET_WM_WINDOW_TYPE", False), 0, 32, False,
            XA_ATOM, &type, &format, &length, &remaining, &value) != Success || !value)
        return 0;
    Atom normal = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    int result = 0;
    if (format == 32)
        for (unsigned long i = 0; i < length; ++i)
            result |= ((Atom *)value)[i] == normal;
    XFree(value);
    return result;
}

static Window find(Display *display, Window window, unsigned long pid)
{
    if (matches(display, window, pid))
        return window;
    Window root, parent, *children = NULL, result = 0;
    unsigned count = 0;
    if (XQueryTree(display, window, &root, &parent, &children, &count))
        for (unsigned i = 0; i < count && !result; ++i)
            result = find(display, children[i], pid);
    if (children)
        XFree(children);
    return result;
}

int main(int argc, char **argv)
{
    if (argc != 3)
        return 2;
    Display *display = XOpenDisplay(NULL);
    if (!display)
        return 3;
    XSetErrorHandler(ignore_error);
    Window window = find(display, DefaultRootWindow(display), strtoul(argv[2], NULL, 10));
    if (!window) { XCloseDisplay(display); return 1; }
    if (strcmp(argv[1], "close") == 0) {
        XEvent event = {0};
        event.xclient.type = ClientMessage;
        event.xclient.window = window;
        event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
        event.xclient.format = 32;
        event.xclient.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", False);
        event.xclient.data.l[1] = CurrentTime;
        XSendEvent(display, window, False, NoEventMask, &event);
        XFlush(display);
    }
    printf("%lu\n", window);
    XCloseDisplay(display);
    return 0;
}
"#;
