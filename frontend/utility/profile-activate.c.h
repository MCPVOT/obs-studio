#include "frontend/build/frontend-utility-profile-activate.pch.h"

void profile_activate_config(const struct profile_activate_ops *ops, bool reset)
{
	ops->init_defaults(ops->param);

	/* Defaults live in the loaded configuration only, and a renamed or
	 * duplicated profile is a new copy of basic.ini: set the encoder
	 * defaults on every activation, or keys the profile never saved read
	 * as NULL. */
	ops->init_encoder_defaults(ops->param);

	if (reset) {
		ops->check_encoders(ops->param);
		ops->reset_data(ops->param);
	}
}

void profile_set_encoder_defaults(config_t *config, const char *simple_encoder, const char *aac_encoder)
{
	config_set_default_string(config, "SimpleOutput", "StreamEncoder", simple_encoder);
	config_set_default_string(config, "SimpleOutput", "RecEncoder", simple_encoder);

	config_set_default_string(config, "AdvOut", "AudioEncoder", aac_encoder);
	config_set_default_string(config, "AdvOut", "RecAudioEncoder", aac_encoder);
}

static bool profile_encoder_id_in(const char *id, const char *const *ids)
{
	for (; *ids; ids++) {
		const char *a = id;
		const char *b = *ids;
		while (*a && *a == *b) {
			a++;
			b++;
		}
		if (*a == '\0' && *b == '\0') {
			return true;
		}
	}
	return false;
}

/* The encoders plugins/obs-qsv11 and plugins/obs-ffmpeg's VA-API encoder
 * register, by id. */
static const char *const profile_qsv_encoder_ids[] = {
	"obs_qsv11",         "obs_qsv11_v2",       "obs_qsv11_soft",
	"obs_qsv11_soft_v2", "obs_qsv11_hevc",     "obs_qsv11_hevc_soft",
	"obs_qsv11_av1",     "obs_qsv11_av1_soft", NULL,
};
static const char *const profile_vaapi_encoder_ids[] = {
	"ffmpeg_vaapi",
	"ffmpeg_vaapi_tex",
	"hevc_ffmpeg_vaapi",
	"hevc_ffmpeg_vaapi_tex",
	"av1_ffmpeg_vaapi",
	"av1_ffmpeg_vaapi_tex",
	NULL,
};

enum profile_missing_encoder
profile_missing_encoder_reason(const char *encoder_id, bool (*module_loaded)(const char *name), const char **module)
{
	const char *name = NULL;
	enum profile_missing_encoder no_device = PROFILE_MISSING_ENCODER_UNKNOWN;

	if (encoder_id && profile_encoder_id_in(encoder_id, profile_qsv_encoder_ids)) {
		name = "obs-qsv11";
		no_device = PROFILE_MISSING_ENCODER_NO_QSV_DEVICE;
	} else if (encoder_id && profile_encoder_id_in(encoder_id, profile_vaapi_encoder_ids)) {
		name = "obs-ffmpeg";
		no_device = PROFILE_MISSING_ENCODER_NO_VAAPI_DEVICE;
	}

	if (module) {
		*module = name;
	}
	if (!name) {
		return PROFILE_MISSING_ENCODER_UNKNOWN;
	}
	return module_loaded(name) ? no_device : PROFILE_MISSING_ENCODER_MODULE_NOT_LOADED;
}

static bool profile_vaapi_read_file(void *param, const char *path, char *buf, size_t size)
{
	(void)param;
	FILE *f = fopen(path, "rb");
	size_t n;
	if (!f) {
		return false;
	}
	n = fread(buf, 1, size - 1, f);
	buf[n] = '\0';
	fclose(f);
	return true;
}

static bool profile_vaapi_exists(void *param, const char *path)
{
	(void)param;
	FILE *f = fopen(path, "rb");
	if (!f) {
		return false;
	}
	fclose(f);
	return true;
}

static bool profile_vaapi_can_open_rw(void *param, const char *path)
{
	(void)param;
	FILE *f = fopen(path, "r+b");
	if (!f) {
		return false;
	}
	fclose(f);
	return true;
}

const struct profile_vaapi_host profile_vaapi_system_host = {
	NULL,
	profile_vaapi_read_file,
	profile_vaapi_exists,
	profile_vaapi_can_open_rw,
};

#define PROFILE_PCI_VENDOR_INTEL 0x8086u
#define PROFILE_PCI_VENDOR_AMD 0x1002u
#define PROFILE_PCI_VENDOR_NVIDIA 0x10deu

/* libva's render device range, and where distributions install VA-API
 * drivers (hostlib's search dirs, build-aux/appimage/hostlib/hostlib.c). */
#define PROFILE_RENDER_NODE_FIRST 128
#define PROFILE_RENDER_NODE_COUNT 8

static const char *const profile_vaapi_driver_dirs[] = {
	"/run/opengl-driver/lib/dri",
	"/usr/lib/x86_64-linux-gnu/dri",
	"/usr/lib/aarch64-linux-gnu/dri",
	"/usr/lib64/dri",
	"/usr/lib/dri",
	NULL,
};

enum profile_distro {
	PROFILE_DISTRO_UNKNOWN,
	PROFILE_DISTRO_DEBIAN,
	PROFILE_DISTRO_FEDORA,
	PROFILE_DISTRO_ARCH,
};

struct profile_vaapi_vendor {
	unsigned int vendor;
	const char *const drivers[3];
	/* By enum profile_distro; [PROFILE_DISTRO_UNKNOWN] is the name shown
	 * when the distribution is not recognized. */
	const char *packages[4];
};

static const struct profile_vaapi_vendor profile_vaapi_vendors[] = {
	{PROFILE_PCI_VENDOR_INTEL,
	 {"iHD", "i965", NULL},
	 {"intel-media-driver", "intel-media-va-driver-non-free", "intel-media-driver", "intel-media-driver"}},
	{PROFILE_PCI_VENDOR_AMD,
	 {"radeonsi", NULL, NULL},
	 {"mesa-va-drivers", "mesa-va-drivers", "mesa-va-drivers-freeworld", "mesa"}},
};

static const char *const profile_install_prefix[] = {NULL, "sudo apt install ", "sudo dnf install ", "sudo pacman -S "};

/* Whether the space-separated, optionally quoted value of key (ID or
 * ID_LIKE) in os-release text names id. */
static bool profile_os_release_has(const char *text, const char *key, const char *id)
{
	size_t key_len = strlen(key);
	size_t id_len = strlen(id);
	const char *line = text;

	while (line && *line) {
		if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
			const char *p = line + key_len + 1;
			while (*p && *p != '\n') {
				while (*p == ' ' || *p == '"' || *p == '\'') {
					p++;
				}
				const char *word = p;
				while (*p && *p != '\n' && *p != ' ' && *p != '"' && *p != '\'') {
					p++;
				}
				if ((size_t)(p - word) == id_len && strncmp(word, id, id_len) == 0) {
					return true;
				}
				if (*p == '"' || *p == '\'') {
					p++;
				}
			}
		}
		line = strchr(line, '\n');
		if (line) {
			line++;
		}
	}
	return false;
}

static enum profile_distro profile_detect_distro(const struct profile_vaapi_host *host)
{
	static const struct {
		const char *id;
		enum profile_distro distro;
	} ids[] = {
		{"debian", PROFILE_DISTRO_DEBIAN}, {"ubuntu", PROFILE_DISTRO_DEBIAN}, {"fedora", PROFILE_DISTRO_FEDORA},
		{"rhel", PROFILE_DISTRO_FEDORA},   {"arch", PROFILE_DISTRO_ARCH},
	};
	char text[2048];

	if (!host->read_file(host->param, "/etc/os-release", text, sizeof(text))) {
		return PROFILE_DISTRO_UNKNOWN;
	}
	for (size_t k = 0; k < 2; k++) {
		const char *key = k == 0 ? "ID" : "ID_LIKE";
		for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
			if (profile_os_release_has(text, key, ids[i].id)) {
				return ids[i].distro;
			}
		}
	}
	return PROFILE_DISTRO_UNKNOWN;
}

static unsigned int profile_read_vendor(const struct profile_vaapi_host *host, int node)
{
	char path[64];
	char text[32];

	snprintf(path, sizeof(path), "/sys/class/drm/renderD%d/device/vendor", node);
	if (!host->read_file(host->param, path, text, sizeof(text))) {
		return 0;
	}
	return (unsigned int)strtoul(text, NULL, 16);
}

static bool profile_vaapi_driver_installed(const struct profile_vaapi_host *host, const struct profile_vaapi_vendor *v)
{
	char path[128];

	for (const char *const *d = v->drivers; *d; d++) {
		for (const char *const *dir = profile_vaapi_driver_dirs; *dir; dir++) {
			snprintf(path, sizeof(path), "%s/%s_drv_video.so", *dir, *d);
			if (host->exists(host->param, path)) {
				return true;
			}
		}
	}
	return false;
}

void profile_vaapi_diagnose(const struct profile_vaapi_host *host, struct profile_vaapi_diagnosis *out)
{
	static char install_command[96];
	bool found_nvidia = false;
	int node = -1;

	memset(out, 0, sizeof(*out));

	/* The first render device that is not an NVIDIA GPU: libva's DRM
	 * path and obs-ffmpeg try those, NVIDIA has no VA-API encoder. */
	for (int i = 0; i < PROFILE_RENDER_NODE_COUNT; i++) {
		char path[32];
		unsigned int vendor;

		snprintf(path, sizeof(path), "/dev/dri/renderD%d", PROFILE_RENDER_NODE_FIRST + i);
		if (!host->exists(host->param, path)) {
			continue;
		}
		vendor = profile_read_vendor(host, PROFILE_RENDER_NODE_FIRST + i);
		if (vendor == PROFILE_PCI_VENDOR_NVIDIA) {
			if (!found_nvidia) {
				snprintf(out->device, sizeof(out->device), "%s", path);
				out->vendor = vendor;
			}
			found_nvidia = true;
			continue;
		}
		snprintf(out->device, sizeof(out->device), "%s", path);
		out->vendor = vendor;
		node = PROFILE_RENDER_NODE_FIRST + i;
		break;
	}

	if (node < 0) {
		out->cause = found_nvidia ? PROFILE_VAAPI_NVIDIA : PROFILE_VAAPI_NO_RENDER_NODE;
		return;
	}
	if (!host->can_open_rw(host->param, out->device)) {
		out->cause = PROFILE_VAAPI_NO_ACCESS;
		return;
	}

	out->cause = PROFILE_VAAPI_NO_ENCODE;
	for (size_t i = 0; i < sizeof(profile_vaapi_vendors) / sizeof(profile_vaapi_vendors[0]); i++) {
		const struct profile_vaapi_vendor *v = &profile_vaapi_vendors[i];
		enum profile_distro distro;

		if (v->vendor != out->vendor || profile_vaapi_driver_installed(host, v)) {
			continue;
		}
		distro = profile_detect_distro(host);
		out->cause = PROFILE_VAAPI_NO_DRIVER;
		out->package = v->packages[distro];
		if (profile_install_prefix[distro]) {
			snprintf(install_command, sizeof(install_command), "%s%s", profile_install_prefix[distro],
				 out->package);
			out->install_command = install_command;
		}
		break;
	}
}

/* Intel GPU generations by PCI device id, from Mesa's
 * include/pci_ids/iris_pci_ids.h and crocus_pci_ids.h at
 * f6bda4756e55e2bc55058cb20f8993f46effb9cd (sorted, for bsearch). The Media
 * SDK runtime drives Broadwell through Ice Lake / Elkhart Lake (iris bdw..ehl);
 * Tiger Lake, Rocket Lake and DG1 work with either runtime; the oneVPL GPU
 * runtime drives every later GPU, so an id in none of these tables is taken to
 * be newer than the table. crocus GPUs (Gen4 to Haswell, Bay Trail, Cherry
 * View) have no Linux Quick Sync runtime. */
static const unsigned short profile_qsv_media_sdk_ids[] = {
	0x0a84, 0x1602, 0x1606, 0x160a, 0x160b, 0x160d, 0x160e, 0x1612, 0x1616, 0x161a, 0x161b, 0x161d, 0x161e, 0x1622,
	0x1626, 0x162a, 0x162b, 0x162d, 0x162e, 0x1902, 0x1906, 0x190a, 0x190b, 0x190e, 0x1912, 0x1913, 0x1915, 0x1916,
	0x1917, 0x191a, 0x191b, 0x191d, 0x191e, 0x1921, 0x1923, 0x1926, 0x1927, 0x192a, 0x192b, 0x192d, 0x1932, 0x193a,
	0x193b, 0x193d, 0x1a84, 0x1a85, 0x3184, 0x3185, 0x3e90, 0x3e91, 0x3e92, 0x3e93, 0x3e94, 0x3e96, 0x3e98, 0x3e99,
	0x3e9a, 0x3e9b, 0x3e9c, 0x3ea0, 0x3ea1, 0x3ea2, 0x3ea3, 0x3ea4, 0x3ea5, 0x3ea6, 0x3ea7, 0x3ea8, 0x3ea9, 0x4541,
	0x4551, 0x4555, 0x4557, 0x4570, 0x4571, 0x4e51, 0x4e55, 0x4e57, 0x4e61, 0x4e71, 0x5902, 0x5906, 0x5908, 0x590a,
	0x590b, 0x590e, 0x5912, 0x5913, 0x5915, 0x5916, 0x5917, 0x591a, 0x591b, 0x591c, 0x591d, 0x591e, 0x5921, 0x5923,
	0x5926, 0x5927, 0x593b, 0x5a84, 0x5a85, 0x87c0, 0x87ca, 0x8a50, 0x8a51, 0x8a52, 0x8a53, 0x8a54, 0x8a56, 0x8a57,
	0x8a58, 0x8a59, 0x8a5a, 0x8a5b, 0x8a5c, 0x8a5d, 0x8a71, 0x9b21, 0x9b41, 0x9ba0, 0x9ba2, 0x9ba4, 0x9ba5, 0x9ba8,
	0x9baa, 0x9bab, 0x9bac, 0x9bc0, 0x9bc2, 0x9bc4, 0x9bc5, 0x9bc6, 0x9bc8, 0x9bca, 0x9bcb, 0x9bcc, 0x9be6, 0x9bf6,
};

static const unsigned short profile_qsv_either_ids[] = {
	0x4905, 0x4906, 0x4907, 0x4908, 0x4909, 0x4c8a, 0x4c8b, 0x4c8c, 0x4c90, 0x4c9a, 0x9a40,
	0x9a49, 0x9a59, 0x9a60, 0x9a68, 0x9a70, 0x9a78, 0x9ac0, 0x9ac9, 0x9ad9, 0x9af8,
};

static const unsigned short profile_qsv_unsupported_ids[] = {
	0x0042, 0x0046, 0x0102, 0x0106, 0x010a, 0x0112, 0x0116, 0x0122, 0x0126, 0x0152, 0x0155, 0x0156, 0x0157, 0x015a,
	0x0162, 0x0166, 0x016a, 0x0402, 0x0406, 0x040a, 0x040b, 0x040e, 0x0412, 0x0416, 0x041a, 0x041b, 0x041e, 0x0422,
	0x0426, 0x042a, 0x042b, 0x042e, 0x0a02, 0x0a06, 0x0a0a, 0x0a0b, 0x0a0e, 0x0a12, 0x0a16, 0x0a1a, 0x0a1b, 0x0a1e,
	0x0a22, 0x0a26, 0x0a2a, 0x0a2b, 0x0a2e, 0x0c02, 0x0c06, 0x0c0a, 0x0c0b, 0x0c0e, 0x0c12, 0x0c16, 0x0c1a, 0x0c1b,
	0x0c1e, 0x0c22, 0x0c26, 0x0c2a, 0x0c2b, 0x0c2e, 0x0d02, 0x0d06, 0x0d0a, 0x0d0b, 0x0d0e, 0x0d12, 0x0d16, 0x0d1a,
	0x0d1b, 0x0d1e, 0x0d22, 0x0d26, 0x0d2a, 0x0d2b, 0x0d2e, 0x0f31, 0x0f32, 0x0f33, 0x22b0, 0x22b1, 0x22b2, 0x22b3,
	0x2972, 0x2982, 0x2992, 0x29a2, 0x2a02, 0x2a12, 0x2a42, 0x2e02, 0x2e12, 0x2e22, 0x2e32, 0x2e42, 0x2e92,
};

/* By enum profile_distro, like profile_vaapi_vendor.packages. */
static const char *const profile_qsv_runtime_packages[][4] = {
	[PROFILE_QSV_RUNTIME_MEDIA_SDK] = {"Intel Media SDK", "libmfx1", "intel-mediasdk", "intel-media-sdk"},
	[PROFILE_QSV_RUNTIME_ONEVPL] = {"Intel oneVPL GPU runtime", "libmfx-gen1.2", "intel-vpl-gpu-rt", "vpl-gpu-rt"},
};

static const char *const profile_qsv_runtime_libraries[] = {
	[PROFILE_QSV_RUNTIME_MEDIA_SDK] = "libmfxhw64.so.1",
	[PROFILE_QSV_RUNTIME_ONEVPL] = "libmfx-gen.so.1.2",
};

static const char *const profile_qsv_library_dirs[] = {
	"/run/opengl-driver/lib", "/usr/lib/x86_64-linux-gnu", "/usr/lib64", "/usr/lib", NULL,
};

static int profile_qsv_compare_id(const void *a, const void *b)
{
	return (int)*(const unsigned short *)a - (int)*(const unsigned short *)b;
}

static bool profile_qsv_id_in(const unsigned short *ids, size_t count, unsigned int id)
{
	unsigned short key = (unsigned short)id;
	return id <= 0xffff && bsearch(&key, ids, count, sizeof(*ids), profile_qsv_compare_id) != NULL;
}

#define PROFILE_QSV_ID_IN(table, id) profile_qsv_id_in(table, sizeof(table) / sizeof(table[0]), id)

static bool profile_qsv_runtime_installed(const struct profile_vaapi_host *host, enum profile_qsv_runtime runtime)
{
	char path[128];

	for (const char *const *dir = profile_qsv_library_dirs; *dir; dir++) {
		snprintf(path, sizeof(path), "%s/%s", *dir, profile_qsv_runtime_libraries[runtime]);
		if (host->exists(host->param, path)) {
			return true;
		}
	}
	return false;
}

static void profile_qsv_set_package(const struct profile_vaapi_host *host, struct profile_qsv_diagnosis *out,
				    const char *const packages[4])
{
	static char install_command[96];
	enum profile_distro distro = profile_detect_distro(host);

	out->package = packages[distro];
	if (profile_install_prefix[distro]) {
		snprintf(install_command, sizeof(install_command), "%s%s", profile_install_prefix[distro],
			 out->package);
		out->install_command = install_command;
	}
}

void profile_qsv_diagnose(const struct profile_vaapi_host *host, struct profile_qsv_diagnosis *out)
{
	const struct profile_vaapi_vendor *intel = &profile_vaapi_vendors[0];
	int node = -1;
	char path[64];
	char text[32];
	bool either;

	memset(out, 0, sizeof(*out));

	for (int i = 0; i < PROFILE_RENDER_NODE_COUNT; i++) {
		snprintf(out->device, sizeof(out->device), "/dev/dri/renderD%d", PROFILE_RENDER_NODE_FIRST + i);
		if (host->exists(host->param, out->device) &&
		    profile_read_vendor(host, PROFILE_RENDER_NODE_FIRST + i) == PROFILE_PCI_VENDOR_INTEL) {
			node = PROFILE_RENDER_NODE_FIRST + i;
			break;
		}
	}
	if (node < 0) {
		out->device[0] = '\0';
		out->cause = PROFILE_QSV_NO_INTEL_GPU;
		return;
	}

	snprintf(path, sizeof(path), "/sys/class/drm/renderD%d/device/device", node);
	if (host->read_file(host->param, path, text, sizeof(text))) {
		out->device_id = (unsigned int)strtoul(text, NULL, 16);
	}
	if (PROFILE_QSV_ID_IN(profile_qsv_unsupported_ids, out->device_id)) {
		out->cause = PROFILE_QSV_UNSUPPORTED_GPU;
		return;
	}
	either = PROFILE_QSV_ID_IN(profile_qsv_either_ids, out->device_id);
	out->runtime = PROFILE_QSV_ID_IN(profile_qsv_media_sdk_ids, out->device_id) ? PROFILE_QSV_RUNTIME_MEDIA_SDK
										    : PROFILE_QSV_RUNTIME_ONEVPL;

	if (!host->can_open_rw(host->param, out->device)) {
		out->cause = PROFILE_QSV_NO_ACCESS;
		return;
	}
	if (!profile_qsv_runtime_installed(host, out->runtime) &&
	    !(either && profile_qsv_runtime_installed(host, PROFILE_QSV_RUNTIME_MEDIA_SDK))) {
		out->cause = PROFILE_QSV_NO_RUNTIME;
		profile_qsv_set_package(host, out, profile_qsv_runtime_packages[out->runtime]);
		return;
	}
	if (!profile_vaapi_driver_installed(host, intel)) {
		out->cause = PROFILE_QSV_NO_DRIVER;
		profile_qsv_set_package(host, out, intel->packages);
		return;
	}
	out->cause = PROFILE_QSV_AVAILABLE;
}

#undef PROFILE_QSV_ID_IN

#undef PROFILE_PCI_VENDOR_INTEL
#undef PROFILE_PCI_VENDOR_AMD
#undef PROFILE_PCI_VENDOR_NVIDIA
#undef PROFILE_RENDER_NODE_FIRST
#undef PROFILE_RENDER_NODE_COUNT
