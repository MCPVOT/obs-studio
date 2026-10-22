//! Fresh-process graphics module/core identity checks, without requiring a GPU.
use obs_build::{NativeBuild, SharedLibrary, libobs::Options};
use std::path::{Path, PathBuf};

pub(super) fn build_probe(
    build: &NativeBuild,
    root: &Path,
    options: &Options,
    core: &Path,
) -> PathBuf {
    let source = build.output.join("config/macos-renderer-probe.c");
    std::fs::write(
        &source,
        r#"
#include <obs.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

static int same_file(const char *left, const char *right) {
    struct stat a, b;
    return !stat(left, &a) && !stat(right, &b) && a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}

int main(int argc, char **argv) {
    if (argc != 4) return 10;
    if (obs_get_version() != LIBOBS_API_VER) return 11;
    void *version = dlsym(RTLD_DEFAULT, "obs_get_version");
    Dl_info core;
    if (!version || !dladdr(version, &core) || !same_file(core.dli_fname, argv[3])) return 12;
    bool metal = !strcmp(argv[2], "METAL");
    if (!metal && strcmp(argv[2], "OPENGL")) return 13;
    void *renderer = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!renderer) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 15; }
    typedef const char *(*name_fn)(void);
    typedef int (*type_fn)(void);
    void *name_export = dlsym(renderer, "device_get_name");
    name_fn name = (name_fn)name_export;
    type_fn type = (type_fn)dlsym(renderer, "device_get_type");
    if (!name || !type || strcmp(name(), metal ? "Metal" : "OpenGL") ||
        type() != (metal ? GS_DEVICE_METAL : GS_DEVICE_OPENGL)) return 14;
    Dl_info owner;
    if (!dladdr(name_export, &owner) || !same_file(owner.dli_fname, argv[1])) return 16;
    printf("core: %s; renderer: %s; API: %s/%d\n", core.dli_fname, owner.dli_fname, name(), type());
    dlclose(renderer);
    return 0;
}
"#,
    )
    .unwrap();
    build
        .executable(&SharedLibrary {
            name: "macos-renderer-probe".into(),
            sources: vec![source],
            include_dirs: vec![
                root.into(),
                root.join("libobs"),
                build.output.join("config"),
                options
                    .native_dependencies
                    .as_ref()
                    .unwrap()
                    .join("include"),
            ],
            dependencies: vec![core.into()],
            c_compile_options: vec!["-std=gnu17".into()],
            compile_options: vec!["-mmacosx-version-min=13.0".into()],
            ..SharedLibrary::default()
        })
        .unwrap()
}
