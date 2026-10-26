/*
 * The Accept-Encoding of OBS's outgoing HTTP calls (file-updater, the
 * frontend's RemoteTextThread). Header-only so the C and C++ call sites share
 * it without a new link dependency.
 */

#pragma once

#include <curl/curl.h>

/* The CURLOPT_ACCEPT_ENCODING value for the libcurl described by info:
 * gzip and deflate when it can decode them, else identity.
 *
 * Never "": that advertises every encoding libcurl was built with, br
 * included, and the linked Brotli decoder has a public out-of-bounds write
 * (CVE-2026-111497, GHSA-c2vc-wwm8-6gx9); #2211 is the restore.
 *
 * Never gzip on a libcurl without zlib (Windows' Schannel build): it cannot
 * decode the answer and fails the transfer with CURLE_BAD_CONTENT_ENCODING
 * (#2453). identity, not NULL: with no Accept-Encoding at all a server may
 * pick any coding. */
static inline const char *obs_curl_accept_encoding(const curl_version_info_data *info)
{
	return info && (info->features & CURL_VERSION_LIBZ) ? "gzip, deflate" : "identity";
}
