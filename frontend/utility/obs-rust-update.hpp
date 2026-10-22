#pragma once

/* OBS-Studio-Rust release versions (<MAJOR>.<MINOR>.<PATCH>[-<pre>], as
 * cmake/common/versionconfig.cmake accepts them), compared with SemVer
 * precedence: 0.1.0-beta.9 < 0.1.0-beta.10 < 0.1.0. The Windows updater uses
 * this to decide whether the certified release in manifest.json is newer than
 * OBS_RUST_VERSION. ci/test_update_feed.py compiles and checks it. */

#include <cstddef>
#include <string>
#include <vector>

namespace obs_rust_update {

struct Version {
	unsigned long long core[3] = {0, 0, 0};
	std::vector<std::string> pre;
};

inline bool IsDigits(const std::string &s)
{
	if (s.empty() || s.size() > 18) {
		return false;
	}
	for (char c : s) {
		if (c < '0' || c > '9') {
			return false;
		}
	}
	return true;
}

inline std::vector<std::string> Split(const std::string &s, char sep)
{
	std::vector<std::string> parts;
	size_t start = 0;
	for (;;) {
		size_t end = s.find(sep, start);
		parts.push_back(s.substr(start, end == std::string::npos ? std::string::npos : end - start));
		if (end == std::string::npos) {
			return parts;
		}
		start = end + 1;
	}
}

inline bool Parse(const std::string &text, Version &out)
{
	size_t dash = text.find('-');
	std::vector<std::string> core = Split(text.substr(0, dash), '.');
	if (core.size() != 3) {
		return false;
	}
	for (size_t i = 0; i < 3; i++) {
		if (!IsDigits(core[i])) {
			return false;
		}
		out.core[i] = std::stoull(core[i]);
	}
	out.pre.clear();
	if (dash != std::string::npos) {
		out.pre = Split(text.substr(dash + 1), '.');
		for (const std::string &id : out.pre) {
			if (id.empty()) {
				return false;
			}
			for (char c : id) {
				if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
					return false;
				}
			}
		}
	}
	return true;
}

inline int CompareIdentifiers(const std::string &a, const std::string &b)
{
	bool an = IsDigits(a), bn = IsDigits(b);
	if (an && bn) {
		unsigned long long x = std::stoull(a), y = std::stoull(b);
		return x < y ? -1 : (x > y ? 1 : 0);
	}
	if (an != bn) {
		return an ? -1 : 1;
	}
	int c = a.compare(b);
	return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

} // namespace obs_rust_update

inline bool ObsRustVersionIsValid(const std::string &version)
{
	obs_rust_update::Version v;
	return obs_rust_update::Parse(version, v);
}

/* <0, 0 or >0 as a is older than, the same as or newer than b. An invalid
 * version is older than any valid one. */
inline int CompareObsRustVersions(const std::string &a, const std::string &b)
{
	using namespace obs_rust_update;
	Version va, vb;
	bool oka = Parse(a, va), okb = Parse(b, vb);
	if (!oka || !okb) {
		return (int)oka - (int)okb;
	}
	for (size_t i = 0; i < 3; i++) {
		if (va.core[i] != vb.core[i]) {
			return va.core[i] < vb.core[i] ? -1 : 1;
		}
	}
	if (va.pre.empty() || vb.pre.empty()) {
		return (int)va.pre.empty() - (int)vb.pre.empty();
	}
	for (size_t i = 0; i < va.pre.size() && i < vb.pre.size(); i++) {
		int c = CompareIdentifiers(va.pre[i], vb.pre[i]);
		if (c) {
			return c;
		}
	}
	return va.pre.size() < vb.pre.size() ? -1 : (va.pre.size() > vb.pre.size() ? 1 : 0);
}

/* Update channels (#1759). "stable" reads the certified feed on the Latest
 * release; "nightly" reads the feed the daily run writes onto the rolling
 * "continuous" pre-release. Any other UpdateBranch falls back to stable. */
#define OBS_RUST_STABLE_FEED_URL "https://github.com/obs-rust/obs-studio/releases/latest/download/"
#define OBS_RUST_NIGHTLY_FEED_URL "https://github.com/obs-rust/obs-studio/releases/download/continuous/"
#define OBS_RUST_STABLE_BRANCH "stable"
#define OBS_RUST_NIGHTLY_BRANCH "nightly"

inline bool ObsRustUpdateBranchIsKnown(const std::string &branch)
{
	return branch == OBS_RUST_STABLE_BRANCH || branch == OBS_RUST_NIGHTLY_BRANCH;
}

/* The feed URL for branch, given the stable (certified) feed URL of a file:
 * a nightly branch reads the same file from the continuous release. A URL
 * outside obs-rust's certified feed is returned unchanged. */
inline std::string ObsRustFeedUrl(const std::string &stableUrl, const std::string &branch)
{
	static const std::string stablePrefix = OBS_RUST_STABLE_FEED_URL;
	if (branch != OBS_RUST_NIGHTLY_BRANCH || stableUrl.compare(0, stablePrefix.size(), stablePrefix) != 0) {
		return stableUrl;
	}
	return OBS_RUST_NIGHTLY_FEED_URL + stableUrl.substr(stablePrefix.size());
}
