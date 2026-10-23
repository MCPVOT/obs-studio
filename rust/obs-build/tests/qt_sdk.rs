//! Explicit Qt SDK inputs: canonical module interfaces from the SDK's own
//! exported targets, validated against the target before anything compiles.
use obs_build::qt::{Inputs, Sdk, Tools};
use std::{
    fs,
    path::{Path, PathBuf},
};

const LINUX: &str = "x86_64-unknown-linux-gnu";
const WINDOWS: &str = "x86_64-pc-windows-msvc";
const MACOS: &str = "aarch64-apple-darwin";

/// Minimal headers for the binary formats each target loads.
fn elf(machine: u16) -> Vec<u8> {
    let mut bytes = vec![0u8; 64];
    bytes[..4].copy_from_slice(b"\x7fELF");
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[18..20].copy_from_slice(&machine.to_le_bytes());
    bytes
}

fn pe(machine: u16) -> Vec<u8> {
    let mut bytes = vec![0u8; 0x90];
    bytes[..2].copy_from_slice(b"MZ");
    bytes[0x3c..0x40].copy_from_slice(&0x80u32.to_le_bytes());
    bytes[0x80..0x84].copy_from_slice(b"PE\0\0");
    bytes[0x84..0x86].copy_from_slice(&machine.to_le_bytes());
    bytes
}

fn macho(cpus: &[u32]) -> Vec<u8> {
    if let [cpu] = cpus {
        let mut bytes = 0xfeed_facfu32.to_le_bytes().to_vec();
        bytes.extend(cpu.to_le_bytes());
        bytes.resize(32, 0);
        return bytes;
    }
    let mut bytes = 0xcafe_babeu32.to_be_bytes().to_vec();
    bytes.extend((cpus.len() as u32).to_be_bytes());
    for cpu in cpus {
        bytes.extend(cpu.to_be_bytes());
        bytes.extend([0u8; 16]);
    }
    bytes
}

const X86_64_MACHO: u32 = 0x0100_0007;
const ARM64_MACHO: u32 = 0x0100_000c;

fn write(path: &Path, content: impl AsRef<[u8]>) {
    fs::create_dir_all(path.parent().unwrap()).unwrap();
    fs::write(path, content).unwrap();
}

fn targets(prefix: &Path, package: &str, target: &str, properties: &[(&str, &str)]) {
    let mut text = format!(
        "add_library({target} SHARED IMPORTED)\n\nset_target_properties({target} PROPERTIES\n"
    );
    for (key, value) in properties {
        text.push_str(&format!("  {key} \"{value}\"\n"));
    }
    text.push_str(")\n");
    let name = if package == "Qt6" { "Qt6" } else { package };
    write(
        &prefix.join(format!("lib/cmake/{package}/{name}Targets.cmake")),
        text,
    );
}

fn configuration(prefix: &Path, module: &str, configuration: &str, properties: &[(&str, &str)]) {
    let upper = configuration.to_ascii_uppercase();
    let mut text = format!(
        "set_property(TARGET Qt6::{module} APPEND PROPERTY IMPORTED_CONFIGURATIONS {upper})\nset_target_properties(Qt6::{module} PROPERTIES\n"
    );
    for (key, value) in properties {
        text.push_str(&format!("  {key}_{upper} \"{value}\"\n"));
    }
    text.push_str(")\n");
    write(
        &prefix.join(format!(
            "lib/cmake/Qt6{module}/Qt6{module}Targets-{configuration}.cmake"
        )),
        text,
    );
}

const MODULES: [(&str, &str); 3] = [
    ("Core", "Qt6::Platform;WrapAtomic::WrapAtomic"),
    ("Gui", "Qt6::Core;WrapOpenGL::WrapOpenGL"),
    ("Widgets", "Qt6::Core;Qt6::Gui"),
];

fn qconfig(path: &Path, version: &str) {
    write(path, format!("#define QT_VERSION_STR \"{version}\"\n"));
}

fn linux_sdk(prefix: &Path, machine: u16, version: &str) {
    targets(
        prefix,
        "Qt6",
        "Qt6::Platform",
        &[
            ("INTERFACE_COMPILE_FEATURES", "cxx_std_17"),
            (
                "INTERFACE_INCLUDE_DIRECTORIES",
                "${_IMPORT_PREFIX}/mkspecs/linux-g++;${_IMPORT_PREFIX}/include",
            ),
            ("INTERFACE_LINK_LIBRARIES", "Threads::Threads"),
        ],
    );
    for (module, links) in MODULES {
        let upper = module.to_ascii_uppercase();
        let includes =
            format!("${{_IMPORT_PREFIX}}/include/Qt{module};${{_IMPORT_PREFIX}}/include");
        let definition = format!("QT_{upper}_LIB");
        targets(
            prefix,
            &format!("Qt6{module}"),
            &format!("Qt6::{module}"),
            &[
                ("INTERFACE_COMPILE_DEFINITIONS", &definition),
                ("INTERFACE_INCLUDE_DIRECTORIES", &includes),
                ("INTERFACE_LINK_LIBRARIES", links),
            ],
        );
        let library = format!("lib/libQt6{module}.so.{version}");
        configuration(
            prefix,
            module,
            "release",
            &[
                (
                    "IMPORTED_LOCATION",
                    &format!("${{_IMPORT_PREFIX}}/{library}"),
                ),
                ("IMPORTED_SONAME", &format!("libQt6{module}.so.6")),
            ],
        );
        write(&prefix.join(library), elf(machine));
        fs::create_dir_all(prefix.join(format!("include/Qt{module}"))).unwrap();
    }
    fs::create_dir_all(prefix.join("mkspecs/linux-g++")).unwrap();
    qconfig(&prefix.join("include/QtCore/qconfig.h"), version);
}

/// Exported properties copied from the obs-studio-libs Windows Qt 6.11.1 SDK.
fn windows_sdk(prefix: &Path, configurations: &[&str]) {
    targets(
        prefix,
        "Qt6",
        "Qt6::Platform",
        &[
            (
                "INTERFACE_COMPILE_DEFINITIONS",
                r"WIN32;_ENABLE_EXTENDED_ALIGNED_STORAGE;WIN64;_WIN64;\$<\$<NOT:\$<BOOL:\$<TARGET_PROPERTY:QT_NO_UNICODE_DEFINES>>>:UNICODE\$<SEMICOLON>_UNICODE>",
            ),
            ("INTERFACE_COMPILE_FEATURES", "cxx_std_17"),
            (
                "INTERFACE_COMPILE_OPTIONS",
                r"\$<\$<AND:\$<CXX_COMPILER_ID:MSVC>,\$<COMPILE_LANGUAGE:CXX>>:-Zc:__cplusplus;-permissive->;\$<\$<AND:\$<NOT:\$<BOOL:\$<TARGET_PROPERTY:QT_NO_UTF8_SOURCE>>>,\$<COMPILE_LANGUAGE:C,CXX>>:\$<\$<CXX_COMPILER_ID:MSVC>:-utf-8>>",
            ),
            (
                "INTERFACE_INCLUDE_DIRECTORIES",
                "${_IMPORT_PREFIX}/mkspecs/win32-msvc;${_IMPORT_PREFIX}/include",
            ),
            ("INTERFACE_LINK_LIBRARIES", "Threads::Threads"),
        ],
    );
    for (module, links) in MODULES {
        let links = if module == "Core" {
            r"Qt6::Platform;mpr;userenv;WrapAtomic::WrapAtomic;\$<\$<AND:\$<NOT:\$<BOOL:\$<TARGET_PROPERTY:qt_no_entrypoint>>>,\$<STREQUAL:\$<TARGET_PROPERTY:TYPE>,EXECUTABLE>,\$<BOOL:\$<TARGET_PROPERTY:WIN32_EXECUTABLE>>>:Qt6::EntryPointPrivate>"
        } else if module == "Gui" {
            "Qt6::Core;d3d11;dxgi;dxguid;d3d12"
        } else {
            links
        };
        let upper = module.to_ascii_uppercase();
        let includes =
            format!("${{_IMPORT_PREFIX}}/include/Qt{module};${{_IMPORT_PREFIX}}/include");
        let definition = format!("QT_{upper}_LIB");
        targets(
            prefix,
            &format!("Qt6{module}"),
            &format!("Qt6::{module}"),
            &[
                ("INTERFACE_COMPILE_DEFINITIONS", &definition),
                ("INTERFACE_INCLUDE_DIRECTORIES", &includes),
                ("INTERFACE_LINK_LIBRARIES", links),
            ],
        );
        for config in configurations {
            let suffix = if *config == "debug" { "d" } else { "" };
            configuration(
                prefix,
                module,
                config,
                &[
                    (
                        "IMPORTED_IMPLIB",
                        &format!("${{_IMPORT_PREFIX}}/lib/Qt6{module}{suffix}.lib"),
                    ),
                    (
                        "IMPORTED_LOCATION",
                        &format!("${{_IMPORT_PREFIX}}/bin/Qt6{module}{suffix}.dll"),
                    ),
                ],
            );
            write(
                &prefix.join(format!("lib/Qt6{module}{suffix}.lib")),
                b"!<arch>\n",
            );
            write(
                &prefix.join(format!("bin/Qt6{module}{suffix}.dll")),
                pe(0x8664),
            );
            crlf(&prefix.join(format!(
                "lib/cmake/Qt6{module}/Qt6{module}Targets-{config}.cmake"
            )));
        }
        // Qt aliases other configurations to the built one in a separate file;
        // only Targets-<configuration> files carry artifacts.
        write(
            &prefix.join(format!(
                "lib/cmake/Qt6{module}/Qt6{module}AdditionalTargetInfo.cmake"
            )),
            format!(
                "set_property(TARGET Qt6::{module} APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)\r\nset_property(TARGET Qt6::{module} APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)\r\n"
            ),
        );
        fs::create_dir_all(prefix.join(format!("include/Qt{module}"))).unwrap();
    }
    fs::create_dir_all(prefix.join("mkspecs/win32-msvc")).unwrap();
    qconfig(&prefix.join("include/QtCore/qconfig.h"), "6.11.1");
}

/// Framework layout and exports from the obs-studio-libs macOS universal2 SDK.
/// Windows SDK exports use CRLF line endings.
fn crlf(path: &Path) {
    let text = fs::read_to_string(path).unwrap();
    fs::write(path, text.replace('\n', "\r\n")).unwrap();
}

fn macos_sdk(prefix: &Path, cpus: &[u32]) {
    targets(
        prefix,
        "Qt6",
        "Qt6::Platform",
        &[
            ("INTERFACE_COMPILE_FEATURES", "cxx_std_17"),
            (
                "INTERFACE_INCLUDE_DIRECTORIES",
                "${_IMPORT_PREFIX}/mkspecs/macx-clang;${_IMPORT_PREFIX}/include",
            ),
            ("INTERFACE_LINK_LIBRARIES", "Threads::Threads"),
        ],
    );
    for (module, links) in MODULES {
        let links = match module {
            "Core" => {
                "Qt6::Platform;-framework IOKit;-framework DiskArbitration;-framework UniformTypeIdentifiers;WrapAtomic::WrapAtomic"
            }
            "Gui" => {
                "Qt6::Core;WrapOpenGL::WrapOpenGL;-framework AppKit;-framework OpenGL;-framework ImageIO;-framework Metal;-framework Metal"
            }
            _ => links,
        };
        let upper = module.to_ascii_uppercase();
        let framework = format!("${{_IMPORT_PREFIX}}/lib/Qt{module}.framework");
        let includes = format!("{framework}/Headers;{framework}");
        let definition = format!("QT_{upper}_LIB");
        targets(
            prefix,
            &format!("Qt6{module}"),
            &format!("Qt6::{module}"),
            &[
                ("INTERFACE_COMPILE_DEFINITIONS", &definition),
                ("INTERFACE_INCLUDE_DIRECTORIES", &includes),
                ("INTERFACE_LINK_LIBRARIES", links),
            ],
        );
        let binary = format!("lib/Qt{module}.framework/Versions/A/Qt{module}");
        configuration(
            prefix,
            module,
            "release",
            &[
                (
                    "IMPORTED_LOCATION",
                    &format!("${{_IMPORT_PREFIX}}/{binary}"),
                ),
                (
                    "IMPORTED_SONAME",
                    &format!("@rpath/Qt{module}.framework/Versions/A/Qt{module}"),
                ),
            ],
        );
        write(&prefix.join(binary), macho(cpus));
        fs::create_dir_all(prefix.join(format!("lib/Qt{module}.framework/Headers"))).unwrap();
    }
    fs::create_dir_all(prefix.join("mkspecs/macx-clang")).unwrap();
    fs::create_dir_all(prefix.join("include")).unwrap();
    qconfig(
        &prefix.join("lib/QtCore.framework/Headers/qconfig.h"),
        "6.11.1",
    );
}

fn position(options: &[String], value: &str) -> usize {
    options
        .iter()
        .position(|option| option == value)
        .unwrap_or_else(|| panic!("{value} missing from {options:?}"))
}

#[test]
fn qt_sdk_resolves_linux_widgets_closure_from_exported_targets() {
    let directory = tempfile::tempdir().unwrap();
    let prefix = directory.path();
    linux_sdk(prefix, 62, "6.11.1");
    let sdk = Sdk::open(LINUX, prefix, false).unwrap();
    assert_eq!(sdk.version(), "6.11.1");
    assert_eq!(sdk.configuration(), "RELEASE");
    let widgets = sdk.interface("Widgets").unwrap();
    for include in [
        "include/QtWidgets",
        "include/QtGui",
        "include/QtCore",
        "include",
        "mkspecs/linux-g++",
    ] {
        assert!(
            widgets
                .include_dirs
                .contains(&prefix.join(include).components().collect::<PathBuf>()),
            "{include} missing from {:?}",
            widgets.include_dirs
        );
    }
    for definition in ["QT_WIDGETS_LIB", "QT_GUI_LIB", "QT_CORE_LIB"] {
        assert!(widgets.definitions.contains(&(definition.into(), None)));
    }
    let link = &widgets.link_options;
    let library = |module: &str| {
        prefix
            .join(format!("lib/libQt6{module}.so.6.11.1"))
            .components()
            .collect::<PathBuf>()
            .display()
            .to_string()
    };
    // Shared-library link order: dependents before their dependencies.
    assert!(position(link, &library("Widgets")) < position(link, &library("Gui")));
    assert!(position(link, &library("Gui")) < position(link, &library("Core")));
    assert!(link.contains(&"-pthread".into()));
    assert!(link.contains(
        &format!("-Wl,-rpath,{}", prefix.join("lib").components().collect::<PathBuf>().display())
    ));
    // Each library appears once even though Gui and Widgets both need Core.
    assert_eq!(
        link.iter()
            .filter(|option| **option == library("Core"))
            .count(),
        1
    );
    let core = sdk.interface("Core").unwrap();
    assert!(!core.definitions.contains(&("QT_WIDGETS_LIB".into(), None)));
    assert!(sdk.interface("Charts").unwrap_err().contains("Qt6Charts"));
}

#[test]
fn qt_sdk_rejects_wrong_architecture_and_major_version() {
    let directory = tempfile::tempdir().unwrap();
    let arm = directory.path().join("arm");
    linux_sdk(&arm, 183, "6.11.1");
    let error = Sdk::open(LINUX, &arm, false).unwrap_err();
    assert!(
        error.contains("aarch64") && error.contains("x86_64"),
        "{error}"
    );
    assert!(Sdk::open("aarch64-unknown-linux-gnu", &arm, false).is_ok());
    let old = directory.path().join("old");
    linux_sdk(&old, 62, "5.15.2");
    let error = Sdk::open(LINUX, &old, false).unwrap_err();
    assert!(
        error.contains("5.15.2") && error.contains("Qt 6"),
        "{error}"
    );
    let error = Sdk::open(LINUX, &directory.path().join("missing"), false).unwrap_err();
    assert!(error.contains("Qt6Targets.cmake"), "{error}");
}

#[test]
fn qt_sdk_resolves_windows_import_libraries_and_runtime() {
    let directory = tempfile::tempdir().unwrap();
    let prefix = directory.path();
    windows_sdk(prefix, &["relwithdebinfo"]);
    let sdk = Sdk::open(WINDOWS, prefix, false).unwrap();
    assert_eq!(sdk.configuration(), "RELWITHDEBINFO");
    let widgets = sdk.interface("Widgets").unwrap();
    for definition in [
        "WIN32",
        "WIN64",
        "_WIN64",
        "UNICODE",
        "_UNICODE",
        "QT_CORE_LIB",
    ] {
        assert!(
            widgets.definitions.contains(&(definition.into(), None)),
            "{definition} missing from {:?}",
            widgets.definitions
        );
    }
    assert_eq!(
        widgets.cpp_compile_options,
        ["-Zc:__cplusplus", "-permissive-"]
    );
    assert_eq!(widgets.compile_options, ["-utf-8"]);
    let link = &widgets.link_options;
    let implib = |module: &str| {
        prefix
            .join(format!("lib/Qt6{module}.lib"))
            .components()
            .collect::<PathBuf>()
            .display()
            .to_string()
    };
    assert!(position(link, &implib("Widgets")) < position(link, &implib("Core")));
    for system in ["mpr.lib", "userenv.lib", "d3d11.lib"] {
        assert!(
            link.contains(&system.into()),
            "{system} missing from {link:?}"
        );
    }
    assert!(
        !link.iter().any(|option| option.contains("EntryPoint")),
        "{link:?}"
    );
    assert_eq!(
        widgets.runtime_path_prepend["PATH"],
        [prefix
            .join("bin")
            .components()
            .collect::<PathBuf>()
            .display()
            .to_string()]
    );
    let error = Sdk::open(WINDOWS, prefix, true).unwrap_err();
    assert!(error.contains("crt-static"), "{error}");
    let error = Sdk::open("aarch64-pc-windows-msvc", prefix, false).unwrap_err();
    assert!(
        error.contains("aarch64") && error.contains("x86_64"),
        "{error}"
    );
}

#[test]
fn qt_sdk_rejects_debug_crt_only_windows_sdks() {
    let directory = tempfile::tempdir().unwrap();
    let debug = directory.path().join("debug");
    windows_sdk(&debug, &["debug"]);
    let error = Sdk::open(WINDOWS, &debug, false).unwrap_err();
    assert!(error.contains("DEBUG") && error.contains("CRT"), "{error}");
    // Multi-configuration SDKs keep working by selecting the release CRT.
    let both = directory.path().join("both");
    windows_sdk(&both, &["debug", "release"]);
    let sdk = Sdk::open(WINDOWS, &both, false).unwrap();
    assert_eq!(sdk.configuration(), "RELEASE");
    let core = sdk.interface("Core").unwrap();
    assert!(
        core.link_options.contains(
            &both
                .join("lib/Qt6Core.lib")
                .components()
                .collect::<PathBuf>()
                .display()
                .to_string()
        )
    );
}

#[test]
fn qt_sdk_resolves_macos_frameworks_for_each_universal_slice() {
    let directory = tempfile::tempdir().unwrap();
    let universal = directory.path().join("universal");
    macos_sdk(&universal, &[X86_64_MACHO, ARM64_MACHO]);
    for target in [MACOS, "x86_64-apple-darwin"] {
        let sdk = Sdk::open(target, &universal, false).unwrap();
        let widgets = sdk.interface("Widgets").unwrap();
        assert!(
            widgets.include_dirs.contains(
                &universal
                    .join("lib/QtWidgets.framework/Headers")
                    .components()
                    .collect::<PathBuf>()
            )
        );
        let link = &widgets.link_options;
        let framework = universal
            .join("lib/QtCore.framework/Versions/A/QtCore")
            .components()
            .collect::<PathBuf>()
            .display()
            .to_string();
        assert!(link.contains(&framework), "{link:?}");
        let iokit = position(link, "IOKit");
        assert_eq!(link[iokit - 1], "-framework");
        // Repeated frameworks in Qt's export collapse to one link input.
        assert_eq!(link.iter().filter(|option| *option == "Metal").count(), 1);
        assert!(link.contains(&format!(
                "-Wl,-rpath,{}",
                universal
                    .join("lib")
                    .components()
                    .collect::<PathBuf>()
                    .display()
            )));
    }
    let intel = directory.path().join("intel");
    macos_sdk(&intel, &[X86_64_MACHO]);
    let error = Sdk::open(MACOS, &intel, false).unwrap_err();
    assert!(
        error.contains("aarch64") && error.contains("x86_64"),
        "{error}"
    );
}

fn inputs(pairs: &[(&str, &Path)]) -> Inputs {
    Inputs::from_lookup(|name| {
        pairs
            .iter()
            .find(|(key, _)| *key == name)
            .map(|(_, value)| value.as_os_str().to_owned())
    })
}

fn tools(prefix: &Path, directory: &str, extension: &str) -> Vec<PathBuf> {
    ["moc", "uic", "rcc"]
        .iter()
        .map(|tool| {
            let path = prefix.join(directory).join(format!("{tool}{extension}"));
            write(&path, b"");
            path
        })
        .collect()
}

#[test]
fn qt_inputs_keep_cross_builds_off_ambient_pkg_config() {
    let directory = tempfile::tempdir().unwrap();
    let cross = "aarch64-unknown-linux-gnu";
    // Native builds without explicit inputs keep the system package path.
    assert!(
        obs_build::qt::package(LINUX, LINUX, "Qt6Widgets", &Inputs::default())
            .unwrap()
            .is_none()
    );
    let error = obs_build::qt::package(LINUX, cross, "Qt6Widgets", &Inputs::default()).unwrap_err();
    assert!(error.contains("OBS_QT_TARGET_PREFIX"), "{error}");
    // Non-Qt packages are outside this resolver.
    assert!(
        obs_build::qt::package(LINUX, cross, "cmocka", &Inputs::default())
            .unwrap()
            .is_none()
    );
    let target = directory.path().join("target");
    linux_sdk(&target, 183, "6.11.1");
    let explicit = inputs(&[("OBS_QT_TARGET_PREFIX", &target)]);
    let widgets = obs_build::qt::package(LINUX, cross, "Qt6Widgets", &explicit)
        .unwrap()
        .unwrap();
    assert!(
        widgets
            .include_dirs
            .contains(&target.join("include/QtWidgets"))
    );

    let error = Tools::resolve(LINUX, cross, &explicit).unwrap_err();
    assert!(error.contains("OBS_QT_HOST_PREFIX"), "{error}");
    let host = directory.path().join("host");
    let expected = tools(&host, "libexec", "");
    let explicit = inputs(&[
        ("OBS_QT_TARGET_PREFIX", &target),
        ("OBS_QT_HOST_PREFIX", &host),
    ]);
    let resolved = Tools::resolve(LINUX, cross, &explicit).unwrap();
    assert_eq!([resolved.moc, resolved.uic, resolved.rcc], expected[..]);
    // A single-tool override still wins over the host SDK.
    let custom = directory.path().join("custom-moc");
    write(&custom, b"");
    let explicit = inputs(&[("OBS_QT_HOST_PREFIX", &host), ("OBS_QT_MOC", &custom)]);
    assert_eq!(Tools::resolve(LINUX, cross, &explicit).unwrap().moc, custom);

    let windows = directory.path().join("windows-host");
    let expected = tools(&windows, "bin", ".exe");
    let explicit = inputs(&[("OBS_QT_HOST_PREFIX", &windows)]);
    let resolved = Tools::resolve(WINDOWS, WINDOWS, &explicit).unwrap();
    assert_eq!(resolved.rcc, expected[2]);
    // A native explicit target SDK is also the host SDK.
    let explicit = inputs(&[("OBS_QT_TARGET_PREFIX", &windows)]);
    assert_eq!(
        Tools::resolve(WINDOWS, WINDOWS, &explicit).unwrap().uic,
        expected[1]
    );
    let error =
        Tools::resolve(LINUX, cross, &inputs(&[("OBS_QT_HOST_PREFIX", &target)])).unwrap_err();
    assert!(error.contains("libexec"), "{error}");
}

/// The published obs-studio-libs SDKs, extracted. Supplying a prefix makes a
/// broken SDK a failure; unset prefixes are reported and skipped. The Windows
/// and macOS Rust lanes set their own prefix when a Qt SDK path changes
/// (`ci/ci_scope.py` `QT_SDK_PATHS`, #1878).
#[test]
fn qt_sdk_resolves_published_sdks() {
    for (variable, targets) in [
        ("OBS_QT_TEST_LINUX_PREFIX", &[LINUX][..]),
        ("OBS_QT_TEST_WINDOWS_PREFIX", &[WINDOWS][..]),
        (
            "OBS_QT_TEST_MACOS_PREFIX",
            &[MACOS, "x86_64-apple-darwin"][..],
        ),
    ] {
        let Some(prefix) = std::env::var_os(variable) else {
            eprintln!("{variable} unset: published Qt SDK not checked");
            continue;
        };
        let prefix = PathBuf::from(prefix);
        for target in targets {
            let sdk = Sdk::open(target, &prefix, false).unwrap();
            assert!(sdk.version().starts_with("6."));
            for module in ["Core", "Gui", "Widgets"] {
                let interface = sdk.interface(module).unwrap();
                assert!(
                    interface
                        .definitions
                        .contains(&(format!("QT_{}_LIB", module.to_ascii_uppercase()), None))
                );
                assert!(interface.link_options.iter().any(|option| {
                    option.contains(&format!("Qt6{module}"))
                        || option.contains(&format!("Qt{module}"))
                }));
                for directory in &interface.include_dirs {
                    assert!(directory.is_dir(), "{}", directory.display());
                }
            }
            eprintln!(
                "{target}: Qt {} {} from {}",
                sdk.version(),
                sdk.configuration(),
                prefix.display()
            );
        }
    }
}
