#[cfg(feature = "bundled")]
#[path = "build-options.rs"]
mod build_options;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=build-options.rs");
    #[cfg(feature = "bundled")]
    {
        for variable in build_options::VARIABLES {
            println!("cargo:rerun-if-env-changed={variable}");
        }
        let modes = build_options::BuildModes::from_lookup(|name| std::env::var_os(name))
            .unwrap_or_else(|error| panic!("configure bundled libobs: {error}"));
        let root = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
        for directory in ["libobs", "deps/libcaption"] {
            println!("cargo:rerun-if-changed={}", root.join(directory).display());
        }
        for variable in [
            "CPATH",
            "C_INCLUDE_PATH",
            "CPLUS_INCLUDE_PATH",
            "LIBRARY_PATH",
            "CFLAGS",
            "CXXFLAGS",
            "CC",
            "CXX",
            "PKG_CONFIG_PATH",
        ] {
            println!("cargo:rerun-if-env-changed={variable}");
        }
        let build = obs_build::NativeBuild {
            host: std::env::var("HOST").unwrap(),
            target: std::env::var("TARGET").unwrap(),
            output: std::path::PathBuf::from(std::env::var_os("OUT_DIR").unwrap()).join("native"),
        };
        let options = obs_build::libobs::Options {
            unity: modes.unity,
            pch: modes.pch,
            ..obs_build::libobs::Options::default()
        };
        println!("cargo:rerun-if-env-changed=OBS_BUILD_RUST_ARCHIVE");
        println!("cargo:rerun-if-env-changed=OBS_SYS_C_ROLLBACK");
        if let Some(archive) = std::env::var_os("OBS_BUILD_RUST_ARCHIVE") {
            println!(
                "cargo:rerun-if-changed={}",
                std::path::Path::new(&archive).display()
            );
        }
        let library = match std::env::var("OBS_SYS_C_ROLLBACK").as_deref() {
            Ok("1") => obs_build::libobs::build_c_reference_with_options(&build, &root, &options),
            Ok("0") | Err(_) => obs_build::libobs::build_with_options(&build, &root, &options),
            _ => panic!("OBS_SYS_C_ROLLBACK must be 0 or 1"),
        }
        .unwrap_or_else(|error| panic!("build bundled libobs: {error}"));
        for (name, enabled) in [("UNITY", modes.unity), ("PCH", modes.pch)] {
            println!(
                "cargo:rustc-env=OBS_SYS_NATIVE_{name}={}",
                u8::from(enabled)
            );
        }
        println!(
            "cargo:rustc-link-search=native={}",
            library.parent().unwrap().display()
        );
        println!("cargo:rustc-link-lib=dylib=obs");
        println!("cargo:rustc-env=OBS_SYS_LIBRARY={}", library.display());
        println!("cargo:library={}", library.display());
    }
}
