//! Shared-dependency resolution and its private regression check.

use std::path::PathBuf;

/// Prefer a shared dependency even when a .pc file lists static paths first.
fn package_shared_library_name(target: &str, name: &str) -> Option<String> {
    if target.contains("linux") {
        Some(format!("lib{name}.so"))
    } else if target.contains("apple") {
        Some(format!("lib{name}.dylib"))
    } else {
        // Windows requires import-library handling rather than linking the DLL.
        None
    }
}

pub fn package_shared_library(target: &str, paths: &[PathBuf], name: &str) -> Option<PathBuf> {
    let filename = package_shared_library_name(target, name)?;
    paths
        .iter()
        .map(|path| path.join(&filename))
        .find(|path| path.is_file())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn shared_dependency_wins_over_an_earlier_static_archive() {
        let dir = tempfile::tempdir().unwrap();
        let archive_dir = dir.path().join("static");
        let shared_dir = dir.path().join("shared");
        std::fs::create_dir_all(&archive_dir).unwrap();
        std::fs::create_dir_all(&shared_dir).unwrap();
        std::fs::write(archive_dir.join("libz.a"), []).unwrap();
        let shared = shared_dir.join("libz.so");
        std::fs::write(&shared, []).unwrap();
        let paths = [archive_dir, shared_dir];
        assert_eq!(
            package_shared_library("x86_64-unknown-linux-gnu", &paths, "z"),
            Some(shared)
        );
        assert_eq!(
            package_shared_library("x86_64-unknown-linux-gnu", &paths, "missing"),
            None
        );
    }
}
