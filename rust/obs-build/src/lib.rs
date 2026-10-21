//! Native compilation and dynamic linking for the Cargo-driven OBS build.
//!
//! This engine owns native artifacts. Cargo remains responsible for compiling
//! Rust crates; native compiles go through cc::Build and inherit soldr's tools.
#![forbid(unsafe_code)]

use std::{fs, path::PathBuf};

mod deps;
pub mod frontend;
pub mod graphics;
mod groups;
pub mod libobs;
mod pch;
mod platform;
pub mod plugins;
pub mod qt;
pub mod scripting;
pub mod validation;

#[derive(Clone, Debug)]
pub struct PrecompiledHeader {
    pub header: PathBuf,
    pub sources: Vec<PathBuf>,
}

#[derive(Clone, Debug, Default)]
pub struct SharedLibrary {
    pub name: String,
    pub sources: Vec<PathBuf>,
    /// Objects compiled once and reused by multiple shared-library variants.
    pub precompiled_objects: Vec<PathBuf>,
    pub include_dirs: Vec<PathBuf>,
    pub dependencies: Vec<PathBuf>,
    pub unity: bool,
    /// Explicit source-to-header assignments; empty disables compiled PCH.
    pub precompiled_headers: Vec<PrecompiledHeader>,
    pub definitions: Vec<(String, Option<String>)>,
    pub compile_options: Vec<String>,
    pub c_compile_options: Vec<String>,
    pub cpp_compile_options: Vec<String>,
    /// Extra flags for Objective-C, in addition to the C-family options.
    pub objc_compile_options: Vec<String>,
    /// Extra flags for Objective-C++, in addition to the C++-family options.
    pub objcpp_compile_options: Vec<String>,
    pub link_options: Vec<String>,
    pub file_name: Option<String>,
}

#[derive(Clone, Debug)]
pub struct NativeBuild {
    pub host: String,
    pub target: String,
    pub output: PathBuf,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(crate) enum Language {
    C,
    Cpp,
    ObjectiveC,
    ObjectiveCpp,
}

impl Language {
    fn cpp(self) -> bool {
        matches!(self, Self::Cpp | Self::ObjectiveCpp)
    }

    fn extension(self) -> &'static str {
        match self {
            Self::C => "c",
            Self::Cpp => "cpp",
            Self::ObjectiveC => "m",
            Self::ObjectiveCpp => "mm",
        }
    }

    pub(crate) fn header_language(self) -> &'static str {
        match self {
            Self::C => "c-header",
            Self::Cpp => "c++-header",
            Self::ObjectiveC => "objective-c-header",
            Self::ObjectiveCpp => "objective-c++-header",
        }
    }
}

struct CompilationBatch {
    language: Language,
    header: Option<PathBuf>,
    sources: Vec<PathBuf>,
}

impl NativeBuild {
    /// Expand explicit BUILD.md groups into individual translation units.
    pub fn separate_group_sources(
        &self,
        root: &std::path::Path,
        sources: &[PathBuf],
    ) -> Result<Vec<PathBuf>, String> {
        groups::separate(root, &self.output.join("individual-sources"), sources)
    }

    /// Compile through cc::Build, then link a native shared library.
    pub fn shared_library(&self, library: &SharedLibrary) -> Result<PathBuf, String> {
        let (output, cpp_link, mut objects) = self.objects(library)?;
        objects.extend(library.precompiled_objects.iter().cloned());
        let artifact = output.join(
            library
                .file_name
                .clone()
                .unwrap_or_else(|| platform::shared_filename(&self.target, &library.name)),
        );
        let compiler = self.compiler(library, &output, cpp_link).get_compiler();
        let mut command = compiler.to_command();
        platform::configure_shared_link(
            &self.target,
            &mut command,
            &artifact,
            &objects,
            &library.dependencies,
            compiler.is_like_msvc(),
        )?;
        command.args(&library.link_options);
        let result = command
            .output()
            .map_err(|e| format!("execute linker {command:?}: {e}"))?;
        if !result.status.success() {
            return Err(format!(
                "link {} failed ({})\n{}\n{}",
                library.name,
                result.status,
                String::from_utf8_lossy(&result.stdout),
                String::from_utf8_lossy(&result.stderr)
            ));
        }
        Ok(artifact)
    }

    /// Compile and link an executable with the native shared dependencies.
    pub fn executable(&self, target: &SharedLibrary) -> Result<PathBuf, String> {
        let (output, cpp, mut objects) = self.objects(target)?;
        objects.extend(target.precompiled_objects.iter().cloned());
        let artifact = output.join(
            target
                .file_name
                .clone()
                .unwrap_or_else(|| platform::executable_filename(&self.target, &target.name)),
        );
        let compiler = self.compiler(target, &output, cpp).get_compiler();
        let mut command = compiler.to_command();
        platform::configure_executable_link(
            &self.target,
            &mut command,
            &artifact,
            &objects,
            &target.dependencies,
            compiler.is_like_msvc(),
        )?;
        command.args(&target.link_options);
        let result = command
            .output()
            .map_err(|e| format!("execute linker {command:?}: {e}"))?;
        if !result.status.success() {
            return Err(format!(
                "link executable {} failed ({})\n{}\n{}",
                target.name,
                result.status,
                String::from_utf8_lossy(&result.stdout),
                String::from_utf8_lossy(&result.stderr)
            ));
        }
        Ok(artifact)
    }

    /// Build an archive with the same compiler, flags and source grouping.
    pub fn static_library(&self, library: &SharedLibrary) -> Result<PathBuf, String> {
        let (output, cpp, objects) = self.objects(library)?;
        self.compiler(library, &output, cpp)
            .try_create_archive(&library.name, objects)
            .map_err(|e| e.to_string())
    }

    fn objects(&self, library: &SharedLibrary) -> Result<(PathBuf, bool, Vec<PathBuf>), String> {
        if library.name.is_empty()
            || !library
                .name
                .bytes()
                .all(|c| c.is_ascii_alphanumeric() || c == b'-' || c == b'_')
        {
            return Err(format!("invalid native target name {:?}", library.name));
        }
        if library.sources.is_empty() {
            return Err(format!("{} has no sources", library.name));
        }
        let output = self.output.join(&library.name);
        fs::create_dir_all(&output).map_err(|e| format!("create {}: {e}", output.display()))?;
        let output = dunce::canonicalize(&output).map_err(|e| e.to_string())?;
        let mut assignments = std::collections::BTreeMap::new();
        for assignment in &library.precompiled_headers {
            let header = dunce::canonicalize(&assignment.header)
                .map_err(|e| format!("{}: {e}", assignment.header.display()))?;
            for source in &assignment.sources {
                let source = dunce::canonicalize(source).map_err(|e| e.to_string())?;
                if assignments.insert(source.clone(), header.clone()).is_some() {
                    return Err(format!(
                        "duplicate PCH source assignment: {}",
                        source.display()
                    ));
                }
            }
        }
        let mut batches: Vec<CompilationBatch> = Vec::new();
        for source in &library.sources {
            let absolute =
                dunce::canonicalize(source).map_err(|e| format!("{}: {e}", source.display()))?;
            let language = match source.extension().and_then(|e| e.to_str()) {
                Some("c") => Language::C,
                Some("cpp" | "cc" | "cxx") => Language::Cpp,
                Some("m") => Language::ObjectiveC,
                Some("mm") => Language::ObjectiveCpp,
                other => {
                    return Err(format!(
                        "unsupported native source language {other:?}: {}",
                        source.display()
                    ));
                }
            };
            let header = assignments.remove(&absolute);
            // Keep translation-unit and constructor order. Combining all users
            // of the same header would move interleaved source files around.
            match batches.last_mut() {
                Some(previous) if previous.language == language && previous.header == header => {
                    previous.sources.push(absolute);
                }
                _ => batches.push(CompilationBatch {
                    language,
                    header,
                    sources: vec![absolute],
                }),
            }
        }
        if !assignments.is_empty() {
            return Err("PCH assignment refers to sources outside this target".into());
        }
        let cpp_link = batches.iter().any(|batch| batch.language.cpp());
        let mut objects = Vec::new();
        for (index, batch) in batches.into_iter().enumerate() {
            let CompilationBatch {
                language,
                header,
                sources,
            } = batch;
            let mut compiler = self.compiler(library, &output, language.cpp());
            compiler.flags(match language {
                Language::ObjectiveC => library.objc_compile_options.as_slice(),
                Language::ObjectiveCpp => library.objcpp_compile_options.as_slice(),
                Language::C | Language::Cpp => &[],
            });
            if let Some(header) = header {
                pch::apply(self, &mut compiler, language, &header)?;
            }
            if library.unity {
                let group = output.join(format!("unity-{index}.{}", language.extension()));
                let mut content = String::from("/* Generated by obs-build. */\n");
                for source in &sources {
                    let path = source.to_str().ok_or("non-UTF8 native source path")?;
                    if path.contains(['\n', '\r', '"']) {
                        return Err(format!(
                            "source path cannot be represented in an include: {path:?}"
                        ));
                    }
                    content.push_str(&format!("#include \"{}\"\n", path.replace('\\', "/")));
                }
                // Do not invalidate a warm compiler cache by touching unchanged groups.
                if fs::read_to_string(&group).ok().as_deref() != Some(&content) {
                    fs::write(&group, content).map_err(|e| e.to_string())?;
                }
                compiler.file(group);
            } else {
                compiler.files(sources);
            }
            objects.extend(
                compiler
                    .try_compile_intermediates()
                    .map_err(|e| e.to_string())?,
            );
        }
        Ok((output, cpp_link, objects))
    }

    fn compiler(&self, library: &SharedLibrary, output: &std::path::Path, cpp: bool) -> cc::Build {
        let mut compiler = cc::Build::new();
        compiler
            .host(&self.host)
            .target(&self.target)
            .out_dir(output)
            .opt_level(2)
            .debug(true)
            .pic(true)
            .cpp(cpp)
            // Intermediate compilation and archive creation do not emit link
            // directives. Keep cc's compiler/archiver environment tracking so
            // target-qualified flags invalidate Cargo's bundled native build.
            .cargo_metadata(true);
        let mut includes = std::collections::HashSet::new();
        for directory in &library.include_dirs {
            if includes.insert(directory) {
                compiler.include(dunce::simplified(directory));
            }
        }
        let mut definitions = std::collections::HashSet::new();
        for (name, value) in &library.definitions {
            if definitions.insert((name, value)) {
                compiler.define(name, value.as_deref());
            }
        }
        compiler.flags(&library.compile_options);
        compiler.flags(if cpp {
            &library.cpp_compile_options
        } else {
            &library.c_compile_options
        });
        compiler
    }
}

/// Host triple used to compile this build engine.
pub const BUILD_HOST: &str = env!("OBS_BUILD_HOST");
/// Target triple used to compile this build engine.
pub const BUILD_TARGET: &str = env!("OBS_BUILD_TARGET");
