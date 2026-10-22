#include "frontend/build/frontend-windows-autoupdatethread.pch.hpp"

/* OBS-Studio-Rust updates only from certified obs-rust releases (#1729, #1736).
 * release-rust.yaml's certify job attaches manifest.json (+ .sig, RSA-signed
 * with the key in data/OBSPublicRSAKey.pem) to the release it promotes to
 * "Latest"; releases/latest/download/ never resolves to a daily pre-release.
 * The manifest names each Windows ZIP with its SHA-256. The ZIP is verified
 * here and again by the installer step, which waits for OBS to exit, copies
 * the ZIP's tree over the install and restarts OBS. Upstream's per-file
 * installer in frontend/updater/ only speaks upstream's CDN. */

#ifndef WIN_UPDATE_FEED_URL
#define WIN_UPDATE_FEED_URL "https://github.com/obs-rust/obs-studio/releases/latest/download/"
#endif

#ifndef WIN_RELEASE_DOWNLOAD_URL
#define WIN_RELEASE_DOWNLOAD_URL "https://github.com/obs-rust/obs-studio/releases/download/"
#endif

#ifndef WIN_DEFAULT_BRANCH
#define WIN_DEFAULT_BRANCH "stable"
#endif

/* ------------------------------------------------------------------------ */

using namespace std;

struct ObsRustRelease {
	string version;
	string tag;
	string notes;
	string name;
	string url;
	string sha256;
	uint64_t size = 0;
};

static const char *CurrentObsRustVersion()
{
#ifdef OBS_RUST_VERSION
	return OBS_RUST_VERSION;
#else
	return "";
#endif
}

/* The ZIP for the architecture this obs64.exe was built for. */
static const char *WindowsArch()
{
	auto base = (const uint8_t *)GetModuleHandleW(nullptr);
	auto dos = (const IMAGE_DOS_HEADER *)base;
	auto nt = (const IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
	return nt->FileHeader.Machine == IMAGE_FILE_MACHINE_ARM64 ? "arm64" : "x64";
}

static bool IsHexDigest(const string &s)
{
	if (s.size() != 64) {
		return false;
	}
	for (char c : s) {
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
			return false;
		}
	}
	return true;
}

static void ParseObsRustManifest(const string &text, ObsRustRelease &out)
{
	nlohmann::json manifest = nlohmann::json::parse(text);
	out.version = manifest.at("version").get<string>();
	out.tag = manifest.at("tag").get<string>();
	out.notes = manifest.value("notes", string());
	const nlohmann::json &zip = manifest.at("windows").at(WindowsArch());
	out.name = zip.at("name").get<string>();
	out.url = zip.at("url").get<string>();
	out.sha256 = zip.at("sha256").get<string>();
	out.size = zip.at("size").get<uint64_t>();

	if (!ObsRustVersionIsValid(out.version) || out.tag != "obs-studio-rust-v" + out.version) {
		throw strprintf("Invalid version in update manifest: %s (%s)", out.version.c_str(), out.tag.c_str());
	}
	string prefix = WIN_RELEASE_DOWNLOAD_URL + out.tag + "/";
	if (out.url.compare(0, prefix.size(), prefix) != 0 || out.name.find_first_of("/\\") != string::npos ||
	    out.url != prefix + out.name) {
		throw strprintf("Update manifest names an unexpected download: %s", out.url.c_str());
	}
	if (!IsHexDigest(out.sha256) || !out.size) {
		throw string("Update manifest has no valid SHA-256 or size");
	}
}

static string PowerShellQuote(const string &s)
{
	string out = "'";
	for (char c : s) {
		out += c;
		if (c == '\'') {
			out += c;
		}
	}
	return out + "'";
}

/* Whether this user can write to the install (else the installer runs elevated). */
static bool CanWriteTo(const wstring &dir)
{
	wstring probe = dir + L"\\.obs-studio-rust-update-probe";
	HANDLE file = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
				  FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}
	CloseHandle(file);
	return true;
}

/* ------------------------------------------------------------------------ */

void AutoUpdateThread::infoMsg(const QString &title, const QString &text)
{
	OBSMessageBox::information(App()->GetMainWindow(), title, text);
}

void AutoUpdateThread::info(const QString &title, const QString &text)
{
	QMetaObject::invokeMethod(this, &AutoUpdateThread::infoMsg, Qt::BlockingQueuedConnection, title, text);
}

int AutoUpdateThread::queryUpdateSlot(bool localManualUpdate, const QString &text)
{
	OBSUpdate updateDlg(App()->GetMainWindow(), localManualUpdate, text);
	return updateDlg.exec();
}

int AutoUpdateThread::queryUpdate(bool localManualUpdate, const char *text_utf8)
{
	int ret = OBSUpdate::No;
	QString text = text_utf8;
	QMetaObject::invokeMethod(this, &AutoUpdateThread::queryUpdateSlot, Qt::BlockingQueuedConnection,
				  qReturnArg(ret), localManualUpdate, text);
	return ret;
}

bool AutoUpdateThread::queryRepairSlot()
{
	QMessageBox::StandardButton res =
		OBSMessageBox::question(App()->GetMainWindow(), QTStr("Updater.RepairConfirm.Title"),
					QTStr("Updater.RepairConfirm.Text"), QMessageBox::Yes | QMessageBox::Cancel);

	return res == QMessageBox::Yes;
}

bool AutoUpdateThread::queryRepair()
{
	bool ret = false;
	QMetaObject::invokeMethod(this, &AutoUpdateThread::queryRepairSlot, Qt::BlockingQueuedConnection,
				  Q_RETURN_ARG(bool, ret));
	return ret;
}

void AutoUpdateThread::run()
try {
	string text;
	bool updatesAvailable = false;

	struct FinishedTrigger {
		inline ~FinishedTrigger()
		{
			QMetaObject::invokeMethod(OBSBasic::Get(), &OBSBasic::updateCheckFinished);
		}
	} finishedTrigger;

	const string current = CurrentObsRustVersion();
	if (!ObsRustVersionIsValid(current)) {
		throw string("Not an OBS-Studio-Rust build; no update feed");
	}

	/* ----------------------------------- *
	 * get branches from server            */

	if (FetchAndVerifyFile("branches", "obs-studio\\updates\\branches.json", WIN_UPDATE_FEED_URL "branches.json",
			       &text)) {
		App()->SetBranchData(text);
	}

	/* Certified releases are the only channel. */
	const char *config_branch = config_get_string(App()->GetAppConfig(), "General", "UpdateBranch");
	if (config_branch && strcmp(config_branch, WIN_DEFAULT_BRANCH) != 0) {
		config_set_string(App()->GetAppConfig(), "General", "UpdateBranch", WIN_DEFAULT_BRANCH);
	}

	/* ----------------------------------- *
	 * get the latest certified release    */

	text.clear();
	if (!FetchAndVerifyFile("manifest", "obs-studio\\updates\\manifest.json", WIN_UPDATE_FEED_URL "manifest.json",
				&text)) {
		if (manualUpdate) {
			info(QTStr("Updater.NoUpdatesAvailable.Title"), QTStr("Updater.NoUpdatesAvailable.Text"));
		}
		return;
	}

	ObsRustRelease release;
	try {
		ParseObsRustManifest(text, release);
	} catch (const nlohmann::json::exception &e) {
		throw strprintf("Failed to parse manifest: %s", e.what());
	}

	int order = CompareObsRustVersions(release.version, current);
	updatesAvailable = order > 0;
	blog(LOG_INFO, "Update check: running %s, latest certified release %s", current.c_str(),
	     release.version.c_str());

	if (!updatesAvailable && !repairMode) {
		if (manualUpdate) {
			info(QTStr("Updater.NoUpdatesAvailable.Title"), QTStr("Updater.NoUpdatesAvailable.Text"));
		}
		return;
	} else if (updatesAvailable && repairMode) {
		info(QTStr("Updater.RepairButUpdatesAvailable.Title"), QTStr("Updater.RepairButUpdatesAvailable.Text"));
		return;
	} else if (repairMode && order != 0) {
		/* Never "repair" to an older release. */
		info(QTStr("Updater.NoUpdatesAvailable.Title"), QTStr("Updater.NoUpdatesAvailable.Text"));
		return;
	}

	/* ----------------------------------- *
	 * skip this version if set to skip    */

	const char *skipUpdateVer = config_get_string(App()->GetAppConfig(), "General", "SkipUpdateVersion");
	if (!manualUpdate && !repairMode && skipUpdateVer && release.version == skipUpdateVer) {
		return;
	}

	/* ----------------------------------- *
	 * query user for update               */

	if (repairMode) {
		if (!queryRepair()) {
			return;
		}
	} else {
		int queryResult = queryUpdate(manualUpdate, release.notes.c_str());

		if (queryResult == OBSUpdate::No) {
			if (!manualUpdate) {
				long long t = (long long)time(nullptr);
				config_set_int(App()->GetAppConfig(), "General", "LastUpdateCheck", t);
			}
			return;

		} else if (queryResult == OBSUpdate::Skip) {
			config_set_string(App()->GetAppConfig(), "General", "SkipUpdateVersion",
					  release.version.c_str());
			return;
		}
	}

	/* ----------------------------------- *
	 * download and verify the release ZIP */

	string zip;
	string error;
	string unusedSignature; /* follows the release asset's redirect */
	long responseCode = 0;
	if (!GetRemoteFile(release.url.c_str(), zip, error, &responseCode, nullptr, "", nullptr, {},
			   &unusedSignature) ||
	    responseCode != 200) {
		info(QTStr("Updater.FailedToLaunch"), QString::fromStdString(release.url));
		throw strprintf("Failed to download %s: %s", release.url.c_str(), error.c_str());
	}

	QByteArray digest =
		QCryptographicHash::hash(QByteArrayView(zip.data(), (qsizetype)zip.size()), QCryptographicHash::Sha256)
			.toHex();
	if (zip.size() != release.size || digest.toStdString() != release.sha256) {
		info(QTStr("Updater.FailedToLaunch"), QTStr("Updater.FailedToLaunch"));
		throw strprintf("%s does not match the signed manifest", release.name.c_str());
	}

	BPtr<char> zipPath = GetAppConfigPathPtr("obs-studio\\updates\\obs-studio-rust-update.zip");
	QFile zipFile(QString::fromUtf8(zipPath.Get()));
	if (!zipFile.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
	    zipFile.write(zip.data(), (qint64)zip.size()) != (qint64)zip.size() || !zipFile.flush()) {
		throw strprintf("Could not write '%s'", zipPath.Get());
	}
	zipFile.close();
	zip.clear();
	zip.shrink_to_fit();

	/* ----------------------------------- *
	 * install directory and restart       */

	wchar_t exePath[MAX_PATH];
	GetModuleFileNameW(nullptr, exePath, _countof(exePath) - 1);
	exePath[_countof(exePath) - 1] = 0;
	wstring baseDir = exePath;
	for (int i = 0; i < 3; i++) { /* <install>\\bin\\64bit\\obs64.exe */
		size_t slash = baseDir.find_last_of(L'\\');
		if (slash == wstring::npos) {
			throw string("Could not find the install directory");
		}
		baseDir.resize(slash);
	}

	string restartArgs;
	obs_cmdline_args obs_args = obs_get_cmdline_args();
	for (int idx = 1; idx < obs_args.argc; idx++) {
		restartArgs += (restartArgs.empty() ? "" : " ") + string(obs_args.argv[idx]);
	}
	/* Portable mode can be enabled via sentinel files, so copying the
	 * command line doesn't guarantee the flag to be there. */
	if (App()->IsPortableMode() && restartArgs.find("--portable") == string::npos) {
		restartArgs += restartArgs.empty() ? "--portable" : " --portable";
	}

	/* ----------------------------------- *
	 * run the installer step              */

	string script =
		"$ErrorActionPreference = 'Stop'\n"
		"$zip = " +
		PowerShellQuote(zipPath.Get()) +
		"\n"
		"$dest = " +
		PowerShellQuote(QString::fromStdWString(baseDir).toStdString()) +
		"\n"
		"$exe = " +
		PowerShellQuote(QString::fromWCharArray(exePath).toStdString()) +
		"\n"
		"$restartArgs = " +
		PowerShellQuote(restartArgs) +
		"\n"
		"$sha = " +
		PowerShellQuote(release.sha256) +
		"\n"
		"$obsPid = " +
		to_string(GetCurrentProcessId()) +
		"\n"
		"try { Wait-Process -Id $obsPid -Timeout 120 -ErrorAction SilentlyContinue } catch {}\n"
		"try {\n"
		"  if ((Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash -ne $sha) {\n"
		"    throw 'The downloaded update does not match its signed manifest.' }\n"
		"  $tmp = Join-Path ([IO.Path]::GetTempPath()) ('obs-studio-rust-update-' + [guid]::NewGuid())\n"
		"  Expand-Archive -LiteralPath $zip -DestinationPath $tmp\n"
		"  if (-not (Test-Path -LiteralPath (Join-Path $tmp 'bin\\64bit\\obs64.exe'))) {\n"
		"    throw 'The update has no bin\\64bit\\obs64.exe.' }\n"
		"  robocopy $tmp $dest /E /R:5 /W:2 /NFL /NDL /NJH /NJS | Out-Null\n"
		"  if ($LASTEXITCODE -ge 8) { throw \"Copying the update failed (robocopy $LASTEXITCODE).\" }\n"
		"  Remove-Item -LiteralPath $tmp -Recurse -Force\n"
		"  Remove-Item -LiteralPath $zip -Force\n"
		"} catch {\n"
		"  Add-Type -AssemblyName PresentationFramework\n"
		"  [System.Windows.MessageBox]::Show(\"OBS-Studio-Rust update failed: $_\", 'OBS-Studio-Rust') | Out-Null\n"
		"}\n"
		"$start = @{ FilePath = $exe; WorkingDirectory = (Split-Path -LiteralPath $exe) }\n"
		"if ($restartArgs) { $start.ArgumentList = $restartArgs }\n"
		"Start-Process @start\n";

	QString scriptText = QString::fromStdString(script);
	QByteArray utf16((const char *)scriptText.utf16(), (qsizetype)scriptText.size() * 2);
	QString encoded = QString::fromLatin1(utf16.toBase64());
	wstring parameters =
		L"-NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -EncodedCommand " +
		encoded.toStdWString();

	SHELLEXECUTEINFOW execInfo = {};
	execInfo.cbSize = sizeof(execInfo);
	execInfo.lpVerb = CanWriteTo(baseDir) ? nullptr : L"runas";
	execInfo.lpFile = L"powershell.exe";
	execInfo.lpParameters = parameters.c_str();
	execInfo.nShow = SW_HIDE;

	if (!ShellExecuteExW(&execInfo)) {
		QString msg = QTStr("Updater.FailedToLaunch");
		info(msg, msg);
		throw strprintf("Can't launch the update installer: %lu", GetLastError());
	}

	/* force OBS to perform another update check immediately after updating
	 * in case of issues with the new version */
	config_set_int(App()->GetAppConfig(), "General", "LastUpdateCheck", 0);
	config_set_string(App()->GetAppConfig(), "General", "SkipUpdateVersion", "0");

	QMetaObject::invokeMethod(OBSBasic::Get(), &OBSBasic::close);

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
}

/* File-local macros (BUILD.md): out of the next file in the unity group. */
#undef WIN_UPDATE_FEED_URL
#undef WIN_RELEASE_DOWNLOAD_URL
#undef WIN_DEFAULT_BRANCH
