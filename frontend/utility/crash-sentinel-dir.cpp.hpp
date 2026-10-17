#include "frontend/build/frontend-utility-crash-sentinel-dir.pch.hpp"

bool ensure_crash_sentinel_dir(const std::filesystem::path &crashSentinelPath)
{
	if (!std::filesystem::exists(crashSentinelPath)) {
		try {
			std::filesystem::create_directories(crashSentinelPath);
		} catch (const std::filesystem::filesystem_error &error) {
			blog(LOG_ERROR,
			     "Crash sentinel location '%s' does not exist and unable to create directory:\n%s.",
			     crashSentinelPath.u8string().c_str(), error.what());
			return false;
		}
	}

	return true;
}
