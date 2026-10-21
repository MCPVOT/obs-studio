//! Build-system integration contracts; these compile and load actual C artifacts.
use obs_build::{NativeBuild, SharedLibrary};
use std::{fs, path::Path};

fn target(name: &str, source: &Path, dependencies: Vec<std::path::PathBuf>) -> SharedLibrary {
    SharedLibrary {
        name: name.into(),
        sources: vec![source.into()],
        include_dirs: vec![],
        dependencies,
        unity: true,
        ..SharedLibrary::default()
    }
}

unsafe fn answer(library: &Path, symbol: &[u8], dependencies: &[&Path]) -> i32 {
    // SAFETY: the fixture below defines this symbol with the C ABI and no args;
    // its owning library stays loaded for the entire call.
    unsafe {
        // Load dependencies explicitly as well: Windows does not search sibling
        // target directories for DLLs. Keep their handles alive through the call.
        let _dependencies: Vec<_> = dependencies
            .iter()
            .map(|path| libloading::Library::new(path).unwrap())
            .collect();
        let lib = libloading::Library::new(library).unwrap();
        let function = lib.get::<unsafe extern "C" fn() -> i32>(symbol).unwrap();
        function()
    }
}

#[test]
fn native_plugin_links_shared_core_and_observes_rebuilt_dependency() {
    let dir = tempfile::tempdir().unwrap();
    let core_source = dir.path().join("core.c");
    let plugin_source = dir.path().join("plugin.c");
    fs::write(
        &core_source,
        "__declspec_placeholder__ int obs_fixture_answer(void) { return 42; }"
            .replace("__declspec_placeholder__", EXPORT),
    )
    .unwrap();
    fs::write(&plugin_source, format!("extern int obs_fixture_answer(void); {EXPORT} int obs_fixture_plugin(void) {{ return obs_fixture_answer() + 1; }}")).unwrap();
    let build = NativeBuild {
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: dir.path().join("artifacts"),
    };
    let core = build
        .shared_library(&target("fixture-core", &core_source, vec![]))
        .unwrap();
    let plugin = build
        .shared_library(&target(
            "fixture-plugin",
            &plugin_source,
            vec![core.clone()],
        ))
        .unwrap();
    // SAFETY: both sources above define the expected no-argument C functions.
    assert_eq!(
        unsafe { answer(&plugin, b"obs_fixture_plugin", &[&core]) },
        43
    );
    fs::write(
        &core_source,
        format!("{EXPORT} int obs_fixture_answer(void) {{ return 84; }}"),
    )
    .unwrap();
    build
        .shared_library(&target("fixture-core", &core_source, vec![]))
        .unwrap();
    // SAFETY: the rebuilt core preserves the function ABI; it changes its result.
    assert_eq!(
        unsafe { answer(&plugin, b"obs_fixture_plugin", &[&core]) },
        85
    );
}

#[test]
fn unity_toggle_preserves_exports_and_explicit_generated_source_wins() {
    let dir = tempfile::tempdir().unwrap();
    let source_dir = dir.path().join("source");
    let generated_dir = dir.path().join("generated");
    fs::create_dir_all(&source_dir).unwrap();
    fs::create_dir_all(&generated_dir).unwrap();
    fs::write(
        source_dir.join("manifest.c"),
        "#error stale source tree manifest selected\n",
    )
    .unwrap();
    let generated = generated_dir.join("manifest.c");
    fs::write(
        &generated,
        format!("{EXPORT} int obs_fixture_count(void) {{ return 17; }}"),
    )
    .unwrap();
    let extra = source_dir.join("extra.c");
    fs::write(&extra, "int obs_fixture_other(void) { return 23; }").unwrap();
    for unity in [false, true] {
        let build = NativeBuild {
            host: obs_build::BUILD_HOST.into(),
            target: obs_build::BUILD_TARGET.into(),
            output: dir.path().join(format!("output-{unity}")),
        };
        let mut library = target("fixture-manifest", &generated, vec![]);
        library.sources.push(extra.clone());
        library.include_dirs.push(source_dir.clone());
        library.unity = unity;
        let artifact = build.shared_library(&library).unwrap();
        // SAFETY: the generated fixture defines this no-argument C function.
        assert_eq!(unsafe { answer(&artifact, b"obs_fixture_count", &[]) }, 17);
    }
}

#[test]
fn executable_runs_against_its_shared_core_dependency() {
    let dir = tempfile::tempdir().unwrap();
    let core_source = dir.path().join("core.c");
    let executable_source = dir.path().join("main.c");
    let helper_source = dir.path().join("helper.c");
    fs::write(
        &helper_source,
        "int fixture_private_value(void) { return 42; }",
    )
    .unwrap();
    fs::write(
        &core_source,
        format!("extern int fixture_private_value(void); {EXPORT} int fixture_value(void) {{ return fixture_private_value(); }}"),
    )
    .unwrap();
    fs::write(
        &executable_source,
        "extern int fixture_value(void); int main(void) { return fixture_value() == 42 ? 0 : 7; }",
    )
    .unwrap();
    let build = NativeBuild {
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: dir.path().join("artifacts"),
    };
    let helper = build
        .static_library(&target("fixture-helper", &helper_source, vec![]))
        .unwrap();
    let core = build
        .shared_library(&target("fixture-core", &core_source, vec![helper]))
        .unwrap();
    let executable = build
        .executable(&target(
            "fixture-app",
            &executable_source,
            vec![core.clone()],
        ))
        .unwrap();
    let old_path = std::env::var_os("PATH").unwrap_or_default();
    let search = std::env::join_paths(
        std::iter::once(core.parent().unwrap().to_path_buf())
            .chain(std::env::split_paths(&old_path)),
    )
    .unwrap();
    assert!(
        std::process::Command::new(executable)
            .env("PATH", search)
            .status()
            .unwrap()
            .success()
    );
}

const EXPORT: &str = "\n#if defined(_WIN32)\n__declspec(dllexport)\n#endif\n";

#[test]
fn pch_preserves_mixed_languages_unity_and_header_flag_invalidation() {
    if !obs_build::BUILD_TARGET.contains("linux") {
        eprintln!("compiled PCH integration currently covers Linux GCC and Clang");
        return;
    }
    let directory = tempfile::tempdir().unwrap();
    let root = directory.path();
    let header = root.join("common.h");
    let transitive = root.join("value.h");
    let write_header = |value| {
        fs::write(&header, format!("#pragma once\n#include \"value.h\"\n#define HEADER_VALUE {value}\n#ifdef __cplusplus\n#include <vector>\n#else\n#include <stdint.h>\n#endif\n")).unwrap()
    };
    let c = root.join("part.c");
    let cpp = root.join("part.cpp");
    fs::write(&c, "#include \"common.h\"\n#ifdef FIXTURE_CPP\n#error C++ flags leaked into C\n#endif\nint fixture_c(void) { return HEADER_VALUE + FIXTURE_C + TRANSITIVE_VALUE; }\n").unwrap();
    fs::write(&cpp, "#include \"common.h\"\n#ifdef FIXTURE_C\n#error C flags leaked into C++\n#endif\nextern \"C\" int fixture_c(void);\nextern \"C\" int obs_fixture_pch(void) { std::vector<int> values{HEADER_VALUE + TRANSITIVE_VALUE, FIXTURE_CPP}; return fixture_c() + values[0] + values[1]; }\n").unwrap();
    for unity in [false, true] {
        write_header(17);
        fs::write(&transitive, "#define TRANSITIVE_VALUE 0\n").unwrap();
        let build = NativeBuild {
            host: obs_build::BUILD_HOST.into(),
            target: obs_build::BUILD_TARGET.into(),
            output: root.join(format!("output-{unity}")),
        };
        let mut library = target("fixture-pch", &c, vec![]);
        library.sources.push(cpp.clone());
        library.include_dirs.push(root.into());
        library.unity = unity;
        library.c_compile_options.push("-DFIXTURE_C=3".into());
        library.cpp_compile_options.push("-DFIXTURE_CPP=5".into());
        library
            .precompiled_headers
            .push(obs_build::PrecompiledHeader {
                header: header.clone(),
                sources: vec![c.clone(), cpp.clone()],
            });
        let artifact = build.shared_library(&library).unwrap();
        // SAFETY: the fixture defines this no-argument C ABI function.
        assert_eq!(unsafe { answer(&artifact, b"obs_fixture_pch", &[]) }, 42);
        write_header(37);
        let artifact = build.shared_library(&library).unwrap();
        // SAFETY: the edited header preserves the function ABI.
        assert_eq!(unsafe { answer(&artifact, b"obs_fixture_pch", &[]) }, 82);
        fs::write(&transitive, "#define TRANSITIVE_VALUE 11\n").unwrap();
        let artifact = build.shared_library(&library).unwrap();
        // SAFETY: the edited transitive header preserves the function ABI.
        assert_eq!(unsafe { answer(&artifact, b"obs_fixture_pch", &[]) }, 104);
        library.c_compile_options = vec!["-DFIXTURE_C=7".into()];
        let artifact = build.shared_library(&library).unwrap();
        // SAFETY: changing the compiler macro preserves the function ABI.
        assert_eq!(unsafe { answer(&artifact, b"obs_fixture_pch", &[]) }, 108);
    }
}

#[test]
fn explicit_groups_compile_as_individual_sources_and_observe_member_edits() {
    let directory = tempfile::tempdir().unwrap();
    let root = directory.path();
    fs::create_dir(root.join("build")).unwrap();
    fs::write(root.join("build/fixture.pch.h"), "#define VALUE 17\n").unwrap();
    let first = root.join("first.c.h");
    fs::write(
        &first,
        "#include \"build/fixture.pch.h\"\nint first(void) { return VALUE; }\n",
    )
    .unwrap();
    fs::write(root.join("second.c.h"), format!("#include \"build/fixture.pch.h\"\nextern int first(void); {EXPORT} int obs_fixture_separate(void) {{ return first() + VALUE; }}\n")).unwrap();
    let group = root.join("build/fixture+.h.c");
    fs::write(&group, "/* explicit group */\n#include \"build/fixture.pch.h\"\n#include \"first.c.h\"\n#include \"second.c.h\"\n").unwrap();
    let build = NativeBuild {
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: root.join("artifacts"),
    };
    let mut library = target("fixture-separate", &group, vec![]);
    library.unity = false;
    library.include_dirs.push(root.into());
    library.sources = build
        .separate_group_sources(root, &library.sources)
        .unwrap();
    assert_eq!(library.sources.len(), 2);
    assert!(!library.sources.contains(&group));
    let artifact = build.shared_library(&library).unwrap();
    // SAFETY: the fixture defines this no-argument C function.
    assert_eq!(
        unsafe { answer(&artifact, b"obs_fixture_separate", &[]) },
        34
    );
    fs::write(
        &first,
        "#include \"build/fixture.pch.h\"\nint first(void) { return VALUE + 1; }\n",
    )
    .unwrap();
    fs::write(root.join("build/fixture.pch.h"), "#define VALUE 21\n").unwrap();
    let artifact = build.shared_library(&library).unwrap();
    // SAFETY: the rebuilt fixture preserves its function ABI.
    assert_eq!(
        unsafe { answer(&artifact, b"obs_fixture_separate", &[]) },
        43
    );
}

#[test]
fn individual_compilation_rejects_conditional_group_selection() {
    let directory = tempfile::tempdir().unwrap();
    let group = directory.path().join("conditional+.h.c");
    fs::write(
        &group,
        "#if ENABLE_OPTION\n#include \"optional.c.h\"\n#endif\n",
    )
    .unwrap();
    let build = NativeBuild {
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: directory.path().join("artifacts"),
    };
    let error = build
        .separate_group_sources(directory.path(), &[group])
        .unwrap_err();
    assert!(error.contains("non-include group line"), "{error}");
}

#[test]
fn deploying_mux_helper_in_place_preserves_its_contents() {
    let directory = tempfile::tempdir().unwrap();
    let helper = directory.path().join("obs-ffmpeg-mux");
    std::fs::write(&helper, b"existing executable").unwrap();
    let artifacts = obs_build::plugins::FfmpegArtifacts {
        module: obs_build::plugins::Module {
            library: std::path::PathBuf::new(),
            data: std::path::PathBuf::new(),
        },
        muxer: helper.clone(),
    };
    assert_eq!(artifacts.deploy_muxer(directory.path()).unwrap(), helper);
    assert_eq!(std::fs::read(helper).unwrap(), b"existing executable");
}

#[test]
fn mixed_c_and_cpp_use_independent_flags_and_the_cpp_linker() {
    let directory = tempfile::tempdir().unwrap();
    let c_source = directory.path().join("helper.c");
    let cpp_source = directory.path().join("main.cpp");
    std::fs::write(&c_source, "#if !defined(C_ONLY) || defined(CPP_ONLY)\n#error wrong C flags\n#endif\nint helper(void) { return 42; }\n").unwrap();
    std::fs::write(&cpp_source, "#include <vector>\n#if !defined(CPP_ONLY) || defined(C_ONLY)\n#error wrong C++ flags\n#endif\nextern \"C\" int helper(void);\nint main() { std::vector<int> values{helper()}; return values.at(0) == 42 ? 0 : 1; }\n").unwrap();
    let target = SharedLibrary {
        name: "mixed-native".into(),
        sources: vec![c_source, cpp_source],
        c_compile_options: vec!["-DC_ONLY".into()],
        cpp_compile_options: vec!["-DCPP_ONLY".into()],
        ..SharedLibrary::default()
    };
    let build = NativeBuild {
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: directory.path().join("out"),
    };
    let artifact = build.executable(&target).unwrap();
    assert!(
        std::process::Command::new(artifact)
            .status()
            .unwrap()
            .success()
    );
}

#[test]
fn pch_batches_preserve_constructor_order_with_interleaved_headers() {
    if !obs_build::BUILD_TARGET.contains("linux") {
        eprintln!("compiled PCH integration currently covers Linux GCC and Clang");
        return;
    }
    let directory = tempfile::tempdir().unwrap();
    let root = directory.path();
    let a = root.join("a-header.h");
    let z = root.join("z-header.h");
    for header in [&a, &z] {
        fs::write(header, "#pragma once\nextern int fixture_order;\n").unwrap();
    }
    let mut sources = Vec::new();
    for index in 1..=3 {
        let path = root.join(format!("part-{index}.cpp"));
        let header = if index == 2 {
            "a-header.h"
        } else {
            "z-header.h"
        };
        fs::write(&path, format!("#include \"{header}\"\nstruct Init{index} {{ Init{index}() {{ fixture_order = fixture_order * 10 + {index}; }} }};\nstatic Init{index} init{index};\n")).unwrap();
        sources.push(path);
    }
    let entry = root.join("entry.cpp");
    fs::write(&entry, "int fixture_order = 0;\nextern \"C\" int obs_fixture_order(void) { return fixture_order; }\n").unwrap();
    for unity in [false, true] {
        let build = NativeBuild {
            host: obs_build::BUILD_HOST.into(),
            target: obs_build::BUILD_TARGET.into(),
            output: root.join(format!("artifacts-{unity}")),
        };
        let mut library = target("fixture-order", &entry, vec![]);
        library.sources = sources.clone();
        library.sources.push(entry.clone());
        library.include_dirs.push(root.into());
        library.unity = unity;
        library.precompiled_headers = vec![
            obs_build::PrecompiledHeader {
                header: a.clone(),
                sources: vec![sources[1].clone()],
            },
            obs_build::PrecompiledHeader {
                header: z.clone(),
                sources: vec![sources[0].clone(), sources[2].clone()],
            },
        ];
        let artifact = build.shared_library(&library).unwrap();
        // SAFETY: the fixture defines this no-argument C ABI function.
        assert_eq!(unsafe { answer(&artifact, b"obs_fixture_order", &[]) }, 123);
    }
}

#[test]
fn production_hybrid_requires_a_real_rust_archive_before_native_compilation() {
    let directory = tempfile::tempdir().unwrap();
    let build = NativeBuild {
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: directory.path().join("build"),
    };
    let error = obs_build::libobs::build_hybrid_with_options(
        &build,
        directory.path(),
        &obs_build::libobs::Options::default(),
        &directory.path().join("missing-rust.a"),
    )
    .unwrap_err();
    assert!(error.contains("Rust archive does not exist"), "{error}");
    assert!(!build.output.exists(), "must not silently start a C build");
}

#[test]
fn objective_groups_expand_to_individual_language_correct_sources() {
    let directory = tempfile::tempdir().unwrap();
    let root = directory.path();
    let build = NativeBuild {
        host: obs_build::BUILD_HOST.into(),
        target: obs_build::BUILD_TARGET.into(),
        output: root.join("artifacts"),
    };
    for (suffix, implementation, extension) in
        [("+.h.m", ".m.h", "m"), ("+.hpp.mm", ".mm.hpp", "mm")]
    {
        let member = format!("member{implementation}");
        fs::write(root.join(&member), "/* fixture */\n").unwrap();
        let group = root.join(format!("fixture{suffix}"));
        fs::write(&group, format!("#include \"{member}\"\n")).unwrap();
        let sources = build
            .separate_group_sources(root, std::slice::from_ref(&group))
            .unwrap();
        assert_eq!(sources.len(), 1);
        assert_ne!(
            sources[0], group,
            "unity-off must expand Objective-C groups"
        );
        assert_eq!(sources[0].extension().unwrap(), extension);
        assert!(fs::read_to_string(&sources[0]).unwrap().contains(&member));
    }
}

#[test]
fn mixed_objective_languages_compile_load_and_rebuild_with_unity_and_pch() {
    if !obs_build::BUILD_TARGET.contains("linux") && !obs_build::BUILD_TARGET.contains("apple") {
        return; // The fixture requires an Objective-C-capable Clang toolchain.
    }
    let selector = format!(
        "{}::mixed_objective_languages_compile_load_and_rebuild_with_unity_and_pch",
        module_path!().split_once("::").unwrap().1
    );
    if std::env::var_os("OBS_BUILD_OBJC_CHILD").is_none() {
        let result = std::process::Command::new(std::env::current_exe().unwrap())
            .args(["--exact", &selector, "--nocapture"])
            .env("OBS_BUILD_OBJC_CHILD", "1")
            .env("CC", "clang")
            .env("CXX", "clang++")
            .output()
            .unwrap();
        assert!(
            result.status.success(),
            "{}\n{}",
            String::from_utf8_lossy(&result.stdout),
            String::from_utf8_lossy(&result.stderr)
        );
        assert!(String::from_utf8_lossy(&result.stdout).contains("1 passed"));
        return;
    }
    let directory = tempfile::tempdir().unwrap();
    let root = directory.path();
    let header = root.join("fixture.pch.h");
    fs::write(&header, "#define VALUE 1\n").unwrap();
    let c = root.join("part.c");
    let cpp = root.join("part.cpp");
    let objc = root.join("part.m");
    let objcpp = root.join("part.mm");
    fs::write(&c, "#include \"fixture.pch.h\"\n#if defined(__OBJC__) || defined(__cplusplus)\n#error wrong C language\n#endif\nint fixture_c(void) { return VALUE; }\n").unwrap();
    fs::write(&objc, "#include \"fixture.pch.h\"\n#if !defined(__OBJC__) || defined(__cplusplus)\n#error wrong Objective-C language\n#endif\nint fixture_objc(void) { return 4 * VALUE * (@encode(int)[0] == 'i'); }\n").unwrap();
    fs::write(&objcpp, "#include \"fixture.pch.h\"\n#if !defined(__OBJC__) || !defined(__cplusplus)\n#error wrong Objective-C++ language\n#endif\ntemplate<int N> constexpr int value() { return N; }\nextern \"C\" int fixture_objcpp(void) { return value<8>() * VALUE * (@encode(int)[0] == 'i'); }\n").unwrap();
    fs::write(&cpp, "#include \"fixture.pch.h\"\n#if defined(__OBJC__) || !defined(__cplusplus)\n#error wrong C++ language\n#endif\nextern \"C\" int fixture_c(void);\nextern \"C\" int fixture_objc(void);\nextern \"C\" int fixture_objcpp(void);\nextern \"C\" int obs_fixture_languages(void) { return fixture_c() + fixture_objc() + fixture_objcpp() + 2 * VALUE; }\n").unwrap();
    for unity in [false, true] {
        for pch in [false, true] {
            // Compiled PCH currently supports Linux; macOS still exercises both unity modes.
            if pch && !obs_build::BUILD_TARGET.contains("linux") {
                continue;
            }
            fs::write(&header, "#define VALUE 1\n").unwrap();
            let build = NativeBuild {
                host: obs_build::BUILD_HOST.into(),
                target: obs_build::BUILD_TARGET.into(),
                output: root.join(format!("output-{unity}-{pch}")),
            };
            let mut library = target("fixture-languages", &c, vec![]);
            library
                .sources
                .extend([objc.clone(), cpp.clone(), objcpp.clone()]);
            library.include_dirs.push(root.into());
            library.unity = unity;
            library.cpp_compile_options.push("-fno-exceptions".into());
            if pch {
                library
                    .precompiled_headers
                    .push(obs_build::PrecompiledHeader {
                        header: header.clone(),
                        sources: library.sources.clone(),
                    });
            }
            let artifact = build.shared_library(&library).unwrap();
            // SAFETY: the fixture defines this no-argument C ABI function.
            assert_eq!(
                unsafe { answer(&artifact, b"obs_fixture_languages", &[]) },
                15
            );
            fs::write(&header, "#define VALUE 2\n").unwrap();
            let artifact = build.shared_library(&library).unwrap();
            // SAFETY: changing the header macro preserves the fixture ABI.
            assert_eq!(
                unsafe { answer(&artifact, b"obs_fixture_languages", &[]) },
                30
            );
        }
    }
}
