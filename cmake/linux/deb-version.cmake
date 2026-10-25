# Debian package version for OBS-Studio-Rust releases (#2299). The "-" before the suffix
# becomes "~" so that 0.1.0~beta.X sorts below 0.1.0, and epoch 1 puts the
# release versions above the git-describe versions (33.0.0-beta6-...) that
# earlier packages carried.
include_guard(GLOBAL)

function(obs_deb_version version epoch)
  string(REGEX REPLACE "-" "~" deb "${OBS_RUST_RELEASE_VERSION}")
  set(${version} "${deb}" PARENT_SCOPE)
  set(${epoch} "1" PARENT_SCOPE)
endfunction()
