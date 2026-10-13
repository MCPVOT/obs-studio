#pragma once

#include <filesystem>

/* The crash sentinel directory step of CrashHandler::checkCrashState, kept
 * free of Qt so it can be unit tested. */

/* Makes sure the crash sentinel directory exists. Returns false (and logs)
 * if it can't be created. */
bool ensure_crash_sentinel_dir(const std::filesystem::path &crashSentinelPath);
