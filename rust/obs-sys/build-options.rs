//! Options shared by the build script and its configuration contracts.
use std::ffi::OsString;

pub const VARIABLES: [&str; 2] = ["OBS_SYS_UNITY", "OBS_SYS_PCH"];

#[derive(Debug, PartialEq, Eq)]
pub struct BuildModes {
    pub unity: bool,
    pub pch: bool,
}

impl BuildModes {
    pub fn from_lookup(mut lookup: impl FnMut(&str) -> Option<OsString>) -> Result<Self, String> {
        Ok(Self {
            unity: flag(VARIABLES[0], lookup(VARIABLES[0]), true)?,
            pch: flag(VARIABLES[1], lookup(VARIABLES[1]), false)?,
        })
    }
}

fn flag(name: &str, value: Option<OsString>, default: bool) -> Result<bool, String> {
    match value.as_ref().map(|value| value.to_str()) {
        None => Ok(default),
        Some(Some("0")) => Ok(false),
        Some(Some("1")) => Ok(true),
        _ => Err(format!("{name} must be 0 or 1")),
    }
}
