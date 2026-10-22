//! Build the hybrid OBS application using the canonical native graph.
#![forbid(unsafe_code)]

fn main() -> std::process::ExitCode {
    match obs_build::cli::run(std::env::args_os().skip(1)) {
        Ok(()) => std::process::ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("obs-build: {error}");
            std::process::ExitCode::FAILURE
        }
    }
}
