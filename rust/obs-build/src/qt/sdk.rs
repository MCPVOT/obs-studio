//! Reads a Qt SDK's exported CMake targets as data. CMake is never run: the
//! SDK's own interface declarations are evaluated for one Cargo target.
use crate::validation::graph::Dependency;
use std::{
    collections::{BTreeMap, BTreeSet},
    fs,
    path::{Path, PathBuf},
};

type Properties = BTreeMap<String, String>;

/// A validated target Qt SDK.
#[derive(Clone, Debug)]
pub struct Sdk {
    target: String,
    prefix: PathBuf,
    version: String,
    configuration: String,
}

impl Sdk {
    /// Open and validate an SDK prefix for one target: CRT, configuration,
    /// architecture and Qt major version.
    pub fn open(target: &str, prefix: &Path, crt_static: bool) -> Result<Self, String> {
        crate::platform::qt_check_target(target, crt_static)?;
        let platform = prefix.join("lib/cmake/Qt6/Qt6Targets.cmake");
        if !platform.is_file() {
            return Err(format!(
                "Qt SDK {} has no {}",
                prefix.display(),
                platform.display()
            ));
        }
        let configurations = configurations(prefix, "Core")?;
        let configuration = ["RELEASE", "RELWITHDEBINFO", "MINSIZEREL", "DEBUG"]
            .into_iter()
            .chain(configurations.iter().map(String::as_str))
            .find(|configuration| {
                configurations.iter().any(|c| c == configuration)
                    && crate::platform::qt_configuration_usable(target, configuration)
            })
            .ok_or_else(|| {
                format!(
                    "Qt SDK {} provides configurations {configurations:?}; the DEBUG configuration links the debug CRT, which Cargo's MSVC targets cannot use",
                    prefix.display()
                )
            })?
            .to_owned();
        let mut sdk = Self {
            target: target.into(),
            prefix: prefix.into(),
            version: String::new(),
            configuration,
        };
        let core = sdk.module("Core")?;
        sdk.check_architecture(&sdk.location("Core")?.0)?;
        let context = sdk.context("CXX");
        let includes = evaluate(property(&core, "INTERFACE_INCLUDE_DIRECTORIES"), &context)?;
        sdk.version = includes
            .split(';')
            .map(|directory| Path::new(directory).join("qconfig.h"))
            .find_map(|header| fs::read_to_string(header).ok())
            .and_then(|text| {
                text.lines().find_map(|line| {
                    line.strip_prefix("#define QT_VERSION_STR \"")
                        .and_then(|rest| rest.strip_suffix('"'))
                        .map(str::to_owned)
                })
            })
            .ok_or_else(|| format!("Qt SDK {} has no QtCore qconfig.h", prefix.display()))?;
        if !sdk.version.starts_with("6.") {
            return Err(format!(
                "Qt SDK {} is version {}; OBS requires Qt 6",
                prefix.display(),
                sdk.version
            ));
        }
        Ok(sdk)
    }

    pub fn version(&self) -> &str {
        &self.version
    }

    pub fn configuration(&self) -> &str {
        &self.configuration
    }

    pub fn prefix(&self) -> &Path {
        &self.prefix
    }

    /// The compile, link and runtime interface of one module (`Widgets`) and
    /// its Qt dependencies, in shared-library link order.
    pub fn interface(&self, module: &str) -> Result<Dependency, String> {
        let mut order = Vec::new();
        self.visit(module, &mut BTreeSet::new(), &mut order)?;
        order.reverse();
        let mut dependency = Dependency::default();
        let mut link = Vec::<Vec<String>>::new();
        for name in order.iter().chain(std::iter::once(&"Platform".to_owned())) {
            let properties = self.module(name)?;
            let cxx = self.context("CXX");
            for directory in list(property(&properties, "INTERFACE_INCLUDE_DIRECTORIES"), &cxx)? {
                let directory = native(&directory);
                if !directory.is_dir() {
                    return Err(format!(
                        "Qt6::{name} include directory {} is missing",
                        directory.display()
                    ));
                }
                if !dependency.include_dirs.contains(&directory) {
                    dependency.include_dirs.push(directory);
                }
            }
            for definition in list(property(&properties, "INTERFACE_COMPILE_DEFINITIONS"), &cxx)? {
                let definition = match definition.split_once('=') {
                    Some((name, value)) => (name.to_owned(), Some(value.to_owned())),
                    None => (definition, None),
                };
                if !dependency.definitions.contains(&definition) {
                    dependency.definitions.push(definition);
                }
            }
            let options = property(&properties, "INTERFACE_COMPILE_OPTIONS");
            let c = list(options, &self.context("C"))?;
            for option in list(options, &cxx)? {
                let destination = if c.contains(&option) {
                    &mut dependency.compile_options
                } else {
                    &mut dependency.cpp_compile_options
                };
                if !destination.contains(&option) {
                    destination.push(option);
                }
            }
            for option in c {
                if !dependency.compile_options.contains(&option)
                    && !dependency.c_compile_options.contains(&option)
                {
                    dependency.c_compile_options.push(option);
                }
            }
            if name != "Platform" {
                let (location, import_library) = self.location(name)?;
                self.check_architecture(&location)?;
                let artifact = crate::platform::qt_link_artifact(
                    &self.target,
                    &location,
                    import_library.as_deref(),
                )?;
                link.push(vec![artifact.display().to_string()]);
                crate::platform::qt_runtime_directory(
                    &self.target,
                    &location,
                    &mut dependency.link_options,
                    &mut dependency.runtime_path_prepend,
                )?;
            }
            for entry in list(property(&properties, "INTERFACE_LINK_LIBRARIES"), &cxx)? {
                let group = if entry.starts_with("Qt6::") {
                    continue;
                } else if let Some(framework) = entry.strip_prefix("-framework ") {
                    vec!["-framework".into(), framework.trim().into()]
                } else if entry.contains("::") {
                    crate::platform::qt_imported_target(&self.target, &entry)?
                } else if entry.starts_with('-') {
                    vec![entry]
                } else {
                    vec![crate::platform::qt_system_library(&self.target, &entry)]
                };
                if !group.is_empty() {
                    link.push(group);
                }
            }
        }
        let mut seen = BTreeSet::new();
        let rpaths = std::mem::take(&mut dependency.link_options);
        dependency.link_options = link
            .into_iter()
            .filter(|group| seen.insert(group.clone()))
            .flatten()
            .chain(rpaths)
            .collect();
        Ok(dependency)
    }

    /// Depth-first over `Qt6::` link edges; dependencies precede dependents.
    fn visit(
        &self,
        module: &str,
        visited: &mut BTreeSet<String>,
        order: &mut Vec<String>,
    ) -> Result<(), String> {
        if !visited.insert(module.into()) {
            return Ok(());
        }
        let properties = self.module(module)?;
        for entry in list(
            property(&properties, "INTERFACE_LINK_LIBRARIES"),
            &self.context("CXX"),
        )? {
            match entry.strip_prefix("Qt6::") {
                Some("Platform") | None => {}
                Some(name) if name.ends_with("Private") || name.ends_with("Internal") => {
                    return Err(format!(
                        "Qt6::{module} requires unsupported private target Qt6::{name}"
                    ));
                }
                Some(name) => self.visit(name, visited, order)?,
            }
        }
        order.push(module.into());
        Ok(())
    }

    fn module(&self, module: &str) -> Result<Properties, String> {
        let (path, name) = if module == "Platform" {
            (
                self.prefix.join("lib/cmake/Qt6/Qt6Targets.cmake"),
                "Qt6::Platform".to_owned(),
            )
        } else {
            (
                self.prefix
                    .join(format!("lib/cmake/Qt6{module}/Qt6{module}Targets.cmake")),
                format!("Qt6::{module}"),
            )
        };
        let text = fs::read_to_string(&path).map_err(|error| {
            format!(
                "Qt6{module} not found in Qt SDK {} ({}: {error})",
                self.prefix.display(),
                path.display()
            )
        })?;
        properties(&text, &name, &self.prefix)
            .ok_or_else(|| format!("{} does not define {name}", path.display()))
    }

    /// The module's runtime binary and, on Windows, its import library.
    fn location(&self, module: &str) -> Result<(PathBuf, Option<PathBuf>), String> {
        let path = self.prefix.join(format!(
            "lib/cmake/Qt6{module}/Qt6{module}Targets-{}.cmake",
            self.configuration.to_ascii_lowercase()
        ));
        let text = fs::read_to_string(&path).map_err(|error| {
            format!(
                "Qt6::{module} has no {} configuration ({}: {error})",
                self.configuration,
                path.display()
            )
        })?;
        let properties = properties(&text, &format!("Qt6::{module}"), &self.prefix)
            .ok_or_else(|| format!("{} does not configure Qt6::{module}", path.display()))?;
        let file = |key: &str| {
            properties
                .get(&format!("{key}_{}", self.configuration))
                .map(|path| native(path))
        };
        let location = file("IMPORTED_LOCATION")
            .ok_or_else(|| format!("{} has no IMPORTED_LOCATION", path.display()))?;
        let import_library = file("IMPORTED_IMPLIB");
        for artifact in std::iter::once(&location).chain(&import_library) {
            if !artifact.is_file() {
                return Err(format!(
                    "Qt6::{module} artifact {} is missing",
                    artifact.display()
                ));
            }
        }
        Ok((location, import_library))
    }

    fn check_architecture(&self, binary: &Path) -> Result<(), String> {
        let expected = crate::platform::qt_architecture(&self.target)?;
        let bytes = fs::read(binary).map_err(|error| format!("{}: {error}", binary.display()))?;
        let found = crate::platform::qt_binary_architectures(&self.target, &bytes)
            .map_err(|error| format!("{}: {error}", binary.display()))?;
        if found.contains(&expected) {
            return Ok(());
        }
        Err(format!(
            "Qt SDK binary {} is built for {}, but target {} needs {expected}",
            binary.display(),
            found.join(", "),
            self.target
        ))
    }

    fn context(&self, language: &'static str) -> Context<'_> {
        Context {
            compiler: crate::platform::qt_compiler_id(&self.target),
            language,
            configuration: &self.configuration,
        }
    }
}

/// Configurations with artifacts: those of the `Qt6<M>Targets-<config>.cmake`
/// files. Qt's AdditionalTargetInfo aliases other configurations to these.
fn configurations(prefix: &Path, module: &str) -> Result<Vec<String>, String> {
    let directory = prefix.join(format!("lib/cmake/Qt6{module}"));
    let marker =
        format!("set_property(TARGET Qt6::{module} APPEND PROPERTY IMPORTED_CONFIGURATIONS ");
    let exports = format!("Qt6{module}Targets-");
    let mut found = Vec::new();
    let entries = fs::read_dir(&directory)
        .map_err(|error| format!("Qt6{module} exports {}: {error}", directory.display()))?;
    let mut paths: Vec<_> = entries
        .filter_map(|entry| entry.ok().map(|e| e.path()))
        .filter(|path| {
            path.file_name()
                .and_then(|name| name.to_str())
                .is_some_and(|name| name.starts_with(&exports) && name.ends_with(".cmake"))
        })
        .collect();
    paths.sort();
    for path in paths {
        let text = fs::read_to_string(&path).unwrap_or_default();
        for line in text.lines() {
            if let Some(name) = line
                .strip_prefix(&marker)
                .and_then(|rest| rest.strip_suffix(')'))
            {
                found.push(name.trim().to_owned());
            }
        }
    }
    if found.is_empty() {
        return Err(format!(
            "Qt6{module} in {} has no imported configurations",
            directory.display()
        ));
    }
    Ok(found)
}

/// Properties of one `set_target_properties(<target> PROPERTIES ...)` block.
fn properties(text: &str, target: &str, prefix: &Path) -> Option<Properties> {
    let header = format!("set_target_properties({target} PROPERTIES");
    let mut lines = text.lines().skip_while(|line| line.trim() != header);
    lines.next()?;
    let prefix = prefix.display().to_string();
    let mut properties = Properties::new();
    for line in lines.take_while(|line| line.trim() != ")") {
        let Some((key, value)) = line.trim().split_once(' ') else {
            continue;
        };
        let value = value.trim().trim_matches('"');
        let value = value
            .replace("\\\"", "\"")
            .replace("\\$", "$")
            .replace("${_IMPORT_PREFIX}", &prefix);
        properties.insert(key.into(), value);
    }
    Some(properties)
}

fn property<'a>(properties: &'a Properties, key: &str) -> &'a str {
    properties.get(key).map_or("", String::as_str)
}

struct Context<'a> {
    compiler: &'a str,
    language: &'a str,
    configuration: &'a str,
}

fn list(text: &str, context: &Context<'_>) -> Result<Vec<String>, String> {
    Ok(evaluate(text, context)?
        .split(';')
        .filter(|entry| !entry.is_empty())
        .map(str::to_owned)
        .collect())
}

/// Evaluate the generator expressions Qt exports for a consumer target with
/// no extra properties. Anything else fails rather than being guessed.
fn evaluate(text: &str, context: &Context<'_>) -> Result<String, String> {
    let mut output = String::new();
    let mut rest = text;
    while let Some(start) = rest.find("$<") {
        output.push_str(&rest[..start]);
        let end = closing(rest, start)?;
        output.push_str(&expression(&rest[start + 2..end], context)?);
        rest = &rest[end + 1..];
    }
    output.push_str(rest);
    Ok(output)
}

fn closing(text: &str, start: usize) -> Result<usize, String> {
    let bytes = text.as_bytes();
    let mut depth = 0;
    let mut index = start;
    while index < bytes.len() {
        if bytes[index] == b'$' && bytes.get(index + 1) == Some(&b'<') {
            depth += 1;
            index += 2;
            continue;
        }
        if bytes[index] == b'>' {
            depth -= 1;
            if depth == 0 {
                return Ok(index);
            }
        }
        index += 1;
    }
    Err(format!("unterminated generator expression in {text:?}"))
}

/// Split at top-level separators, outside nested `$<...>`.
fn split(text: &str, separator: u8, limit: usize) -> Vec<&str> {
    let bytes = text.as_bytes();
    let (mut depth, mut start, mut parts) = (0, 0, Vec::new());
    let mut index = 0;
    while index < bytes.len() {
        if bytes[index] == b'$' && bytes.get(index + 1) == Some(&b'<') {
            depth += 1;
            index += 2;
            continue;
        }
        if bytes[index] == b'>' && depth > 0 {
            depth -= 1;
        } else if bytes[index] == separator && depth == 0 && parts.len() + 1 < limit {
            parts.push(&text[start..index]);
            start = index + 1;
        }
        index += 1;
    }
    parts.push(&text[start..]);
    parts
}

fn truth(value: &str) -> Result<bool, String> {
    match value {
        "1" => Ok(true),
        "0" => Ok(false),
        _ => Err(format!(
            "generator expression condition {value:?} is not 0 or 1"
        )),
    }
}

fn boolean(value: bool) -> String {
    if value { "1" } else { "0" }.into()
}

fn expression(body: &str, context: &Context<'_>) -> Result<String, String> {
    let parts = split(body, b':', 2);
    let (head, argument) = (parts[0], parts.get(1).copied().unwrap_or(""));
    if head.contains("$<") || head == "0" || head == "1" {
        return if truth(&evaluate(head, context)?)? {
            evaluate(argument, context)
        } else {
            Ok(String::new())
        };
    }
    let arguments = || -> Result<Vec<String>, String> {
        split(argument, b',', usize::MAX)
            .into_iter()
            .map(|part| evaluate(part, context))
            .collect()
    };
    let member = |value: &str| -> Result<String, String> {
        Ok(boolean(arguments()?.iter().any(|item| item == value)))
    };
    match head {
        "SEMICOLON" => Ok(";".into()),
        // A consumer sets none of Qt's opt-out properties.
        "TARGET_PROPERTY" => Ok(String::new()),
        "BOOL" => {
            let value = evaluate(argument, context)?.to_ascii_uppercase();
            let false_values = ["", "0", "OFF", "NO", "FALSE", "N", "IGNORE", "NOTFOUND"];
            Ok(boolean(
                !false_values.contains(&value.as_str()) && !value.ends_with("-NOTFOUND"),
            ))
        }
        "NOT" => Ok(boolean(!truth(&evaluate(argument, context)?)?)),
        "AND" => Ok(boolean(
            arguments()?
                .iter()
                .map(|value| truth(value))
                .collect::<Result<Vec<_>, _>>()?
                .into_iter()
                .all(|value| value),
        )),
        "OR" => Ok(boolean(
            arguments()?
                .iter()
                .map(|value| truth(value))
                .collect::<Result<Vec<_>, _>>()?
                .into_iter()
                .any(|value| value),
        )),
        "STREQUAL" => match arguments()?.as_slice() {
            [left, right] => Ok(boolean(left == right)),
            _ => Err("STREQUAL takes two arguments".into()),
        },
        "IF" => match arguments()?.as_slice() {
            [condition, yes, no] => Ok(if truth(condition)? { yes } else { no }.clone()),
            _ => Err("IF takes three arguments".into()),
        },
        "CXX_COMPILER_ID" => member(context.compiler),
        "COMPILE_LANGUAGE" => member(context.language),
        "CONFIG" => {
            Ok(boolean(arguments()?.iter().any(|value| {
                value.eq_ignore_ascii_case(context.configuration)
            })))
        }
        _ => Err(format!("unsupported Qt generator expression $<{head}>")),
    }
}

/// `${_IMPORT_PREFIX}/lib/...` mixes separators on Windows; rebuild the path
/// so linker arguments and runtime paths use the host's own form.
fn native(path: &str) -> PathBuf {
    Path::new(path).components().collect()
}
