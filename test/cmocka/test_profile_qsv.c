#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>

#include "profile-activate.h"

/* obs-rust/obs-studio#1925: why obs-qsv11 found no usable Intel GPU, read
 * from a faked host. #1720's reporter had an HD Graphics 530 and got only a
 * generic "encoder missing"; each case is one cause the dialog must tell
 * apart, and the install command must match the GPU generation. Device ids
 * are from Mesa's include/pci_ids/{iris,crocus}_pci_ids.h. */
struct fake_file {
	const char *path;
	const char *text;
};

struct fake_host {
	const struct fake_file *files; /* NULL-path terminated; text NULL: exists, not readable */
	const char *const *rw;         /* devices the user can open, NULL terminated */
};

static const struct fake_file *fake_find(const struct fake_host *h, const char *path)
{
	for (const struct fake_file *f = h->files; f && f->path; f++) {
		if (strcmp(f->path, path) == 0) {
			return f;
		}
	}
	return NULL;
}

static bool fake_read_file(void *param, const char *path, char *buf, size_t size)
{
	const struct fake_file *f = fake_find(param, path);
	if (!f || !f->text) {
		return false;
	}
	snprintf(buf, size, "%s", f->text);
	return true;
}

static bool fake_exists(void *param, const char *path)
{
	return fake_find(param, path) != NULL;
}

static bool fake_can_open_rw(void *param, const char *path)
{
	const struct fake_host *h = param;
	for (const char *const *p = h->rw; p && *p; p++) {
		if (strcmp(*p, path) == 0) {
			return true;
		}
	}
	return false;
}

static void diagnose(const struct fake_file *files, const char *const *rw, struct profile_qsv_diagnosis *out)
{
	struct fake_host h = {files, rw};
	const struct profile_vaapi_host host = {&h, fake_read_file, fake_exists, fake_can_open_rw};
	memset(out, 0x5a, sizeof(*out));
	profile_qsv_diagnose(&host, out);
}

static const char ubuntu[] = "PRETTY_NAME=\"Ubuntu 24.04 LTS\"\nID=ubuntu\nID_LIKE=debian\n";
static const char fedora[] = "NAME=\"Fedora Linux\"\nID=fedora\n";
static const char arch[] = "NAME=\"Arch Linux\"\nID=arch\n";
static const char *const rw_128[] = {"/dev/dri/renderD128", NULL};

#define NODE_128 {"/dev/dri/renderD128", NULL}
#define INTEL_128 {"/sys/class/drm/renderD128/device/vendor", "0x8086\n"}
#define DEVICE_128(id) {"/sys/class/drm/renderD128/device/device", id "\n"}
#define MEDIA_SDK_DEB {"/usr/lib/x86_64-linux-gnu/libmfxhw64.so.1", NULL}
#define ONEVPL_DEB {"/usr/lib/x86_64-linux-gnu/libmfx-gen.so.1.2", NULL}
#define IHD_DEB {"/usr/lib/x86_64-linux-gnu/dri/iHD_drv_video.so", NULL}

static void test_qsv_no_intel_gpu(void **state)
{
	(void)state;
	const struct fake_file amd[] = {{"/etc/os-release", ubuntu},
					NODE_128,
					{"/sys/class/drm/renderD128/device/vendor", "0x1002\n"},
					{NULL, NULL}};
	const struct fake_file none[] = {{"/etc/os-release", ubuntu}, {NULL, NULL}};
	struct profile_qsv_diagnosis d;

	diagnose(amd, rw_128, &d);
	assert_int_equal(d.cause, PROFILE_QSV_NO_INTEL_GPU);
	assert_string_equal(d.device, "");
	assert_null(d.install_command);

	diagnose(none, NULL, &d);
	assert_int_equal(d.cause, PROFILE_QSV_NO_INTEL_GPU);
	assert_null(d.install_command);
}

/* The Intel GPU on a later render node than a discrete AMD card. */
static void test_qsv_intel_second_node(void **state)
{
	(void)state;
	const struct fake_file files[] = {{"/etc/os-release", ubuntu},
					  NODE_128,
					  {"/sys/class/drm/renderD128/device/vendor", "0x1002\n"},
					  {"/dev/dri/renderD129", NULL},
					  {"/sys/class/drm/renderD129/device/vendor", "0x8086\n"},
					  {"/sys/class/drm/renderD129/device/device", "0x1912\n"},
					  {NULL, NULL}};
	const char *const rw[] = {"/dev/dri/renderD129", NULL};
	struct profile_qsv_diagnosis d;

	diagnose(files, rw, &d);
	assert_int_equal(d.cause, PROFILE_QSV_NO_RUNTIME);
	assert_string_equal(d.device, "/dev/dri/renderD129");
	assert_int_equal(d.device_id, 0x1912);
}

/* #1720: HD Graphics 530 (Skylake) on Ubuntu without the Media SDK. */
static void test_qsv_skylake_needs_media_sdk_ubuntu(void **state)
{
	(void)state;
	const struct fake_file files[] = {{"/etc/os-release", ubuntu}, NODE_128, INTEL_128,
					  DEVICE_128("0x1912"),        IHD_DEB,  {NULL, NULL}};
	struct profile_qsv_diagnosis d;

	diagnose(files, rw_128, &d);
	assert_int_equal(d.cause, PROFILE_QSV_NO_RUNTIME);
	assert_string_equal(d.device, "/dev/dri/renderD128");
	assert_int_equal(d.device_id, 0x1912);
	assert_int_equal(d.runtime, PROFILE_QSV_RUNTIME_MEDIA_SDK);
	assert_string_equal(d.package, "libmfx1");
	assert_string_equal(d.install_command, "sudo apt install libmfx1");
}

/* The oneVPL runtime cannot drive Skylake, so it does not count. */
static void test_qsv_skylake_with_only_onevpl(void **state)
{
	(void)state;
	const struct fake_file files[] = {{"/etc/os-release", ubuntu},
					  NODE_128,
					  INTEL_128,
					  DEVICE_128("0x1912"),
					  ONEVPL_DEB,
					  IHD_DEB,
					  {NULL, NULL}};
	struct profile_qsv_diagnosis d;

	diagnose(files, rw_128, &d);
	assert_int_equal(d.cause, PROFILE_QSV_NO_RUNTIME);
	assert_int_equal(d.runtime, PROFILE_QSV_RUNTIME_MEDIA_SDK);
}

static void test_qsv_skylake_available(void **state)
{
	(void)state;
	const struct fake_file files[] = {{"/etc/os-release", ubuntu},
					  NODE_128,
					  INTEL_128,
					  DEVICE_128("0x1912"),
					  MEDIA_SDK_DEB,
					  IHD_DEB,
					  {NULL, NULL}};
	struct profile_qsv_diagnosis d;

	diagnose(files, rw_128, &d);
	assert_int_equal(d.cause, PROFILE_QSV_AVAILABLE);
	assert_int_equal(d.runtime, PROFILE_QSV_RUNTIME_MEDIA_SDK);
	assert_null(d.install_command);
}

/* Arc A770 (DG2): the oneVPL runtime, never libmfx1. */
static void test_qsv_arc_needs_onevpl_per_distro(void **state)
{
	(void)state;
	static const struct {
		const char *os_release;
		const char *package;
		const char *command;
	} cases[] = {
		{ubuntu, "libmfx-gen1.2", "sudo apt install libmfx-gen1.2"},
		{fedora, "intel-vpl-gpu-rt", "sudo dnf install intel-vpl-gpu-rt"},
		{arch, "vpl-gpu-rt", "sudo pacman -S vpl-gpu-rt"},
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		const struct fake_file files[] = {{"/etc/os-release", cases[i].os_release},
						  NODE_128,
						  INTEL_128,
						  DEVICE_128("0x56a0"),
						  MEDIA_SDK_DEB,
						  {NULL, NULL}};
		struct profile_qsv_diagnosis d;

		diagnose(files, rw_128, &d);
		assert_int_equal(d.cause, PROFILE_QSV_NO_RUNTIME);
		assert_int_equal(d.runtime, PROFILE_QSV_RUNTIME_ONEVPL);
		assert_string_equal(d.package, cases[i].package);
		assert_string_equal(d.install_command, cases[i].command);
	}
}

static void test_qsv_media_sdk_per_distro(void **state)
{
	(void)state;
	const struct fake_file fed[] = {{"/etc/os-release", fedora},
					NODE_128,
					INTEL_128,
					DEVICE_128("0x1912"),
					{NULL, NULL}};
	const struct fake_file arc[] = {{"/etc/os-release", arch},
					NODE_128,
					INTEL_128,
					DEVICE_128("0x1912"),
					{NULL, NULL}};
	const struct fake_file unknown[] = {{"/etc/os-release", "ID=gentoo\n"},
					    NODE_128,
					    INTEL_128,
					    DEVICE_128("0x1912"),
					    {NULL, NULL}};
	struct profile_qsv_diagnosis d;

	diagnose(fed, rw_128, &d);
	assert_string_equal(d.install_command, "sudo dnf install intel-mediasdk");
	diagnose(arc, rw_128, &d);
	assert_string_equal(d.install_command, "sudo pacman -S intel-media-sdk");
	diagnose(unknown, rw_128, &d);
	assert_int_equal(d.cause, PROFILE_QSV_NO_RUNTIME);
	assert_string_equal(d.package, "Intel Media SDK");
	assert_null(d.install_command);
}

/* Tiger Lake is driven by either runtime. */
static void test_qsv_tiger_lake_either_runtime(void **state)
{
	(void)state;
	const struct fake_file legacy[] = {{"/etc/os-release", ubuntu},
					   NODE_128,
					   INTEL_128,
					   DEVICE_128("0x9a49"),
					   MEDIA_SDK_DEB,
					   IHD_DEB,
					   {NULL, NULL}};
	const struct fake_file none[] = {{"/etc/os-release", ubuntu}, NODE_128, INTEL_128,
					 DEVICE_128("0x9a49"),        IHD_DEB,  {NULL, NULL}};
	struct profile_qsv_diagnosis d;

	diagnose(legacy, rw_128, &d);
	assert_int_equal(d.cause, PROFILE_QSV_AVAILABLE);
	diagnose(none, rw_128, &d);
	assert_int_equal(d.cause, PROFILE_QSV_NO_RUNTIME);
	assert_int_equal(d.runtime, PROFILE_QSV_RUNTIME_ONEVPL);
	assert_string_equal(d.package, "libmfx-gen1.2");
}

/* An Intel id newer than the table is a newer GPU: oneVPL. */
static void test_qsv_unknown_intel_id_is_onevpl(void **state)
{
	(void)state;
	const struct fake_file files[] = {{"/etc/os-release", ubuntu},
					  NODE_128,
					  INTEL_128,
					  DEVICE_128("0xfff0"),
					  {NULL, NULL}};
	struct profile_qsv_diagnosis d;

	diagnose(files, rw_128, &d);
	assert_int_equal(d.cause, PROFILE_QSV_NO_RUNTIME);
	assert_int_equal(d.runtime, PROFILE_QSV_RUNTIME_ONEVPL);
}

/* Haswell (HD 4600) and Cherry View have no Linux Quick Sync runtime:
 * never an install command. */
static void test_qsv_unsupported_gpu(void **state)
{
	(void)state;
	static const char *const ids[] = {"0x0412", "0x22b0"};
	for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
		const struct fake_file files[] = {{"/etc/os-release", ubuntu},
						  NODE_128,
						  INTEL_128,
						  {"/sys/class/drm/renderD128/device/device", ids[i]},
						  {NULL, NULL}};
		struct profile_qsv_diagnosis d;

		diagnose(files, rw_128, &d);
		assert_int_equal(d.cause, PROFILE_QSV_UNSUPPORTED_GPU);
		assert_int_equal(d.runtime, PROFILE_QSV_RUNTIME_NONE);
		assert_null(d.package);
		assert_null(d.install_command);
	}
}

static void test_qsv_no_access(void **state)
{
	(void)state;
	const struct fake_file files[] = {{"/etc/os-release", ubuntu},
					  NODE_128,
					  INTEL_128,
					  DEVICE_128("0x1912"),
					  MEDIA_SDK_DEB,
					  IHD_DEB,
					  {NULL, NULL}};
	struct profile_qsv_diagnosis d;

	diagnose(files, NULL, &d);
	assert_int_equal(d.cause, PROFILE_QSV_NO_ACCESS);
	assert_string_equal(d.device, "/dev/dri/renderD128");
	assert_null(d.install_command);
}

/* Both runtimes drive the GPU through the iHD VA-API driver. */
static void test_qsv_no_driver(void **state)
{
	(void)state;
	const struct fake_file files[] = {{"/etc/os-release", ubuntu}, NODE_128,      INTEL_128,
					  DEVICE_128("0x1912"),        MEDIA_SDK_DEB, {NULL, NULL}};
	struct profile_qsv_diagnosis d;

	diagnose(files, rw_128, &d);
	assert_int_equal(d.cause, PROFILE_QSV_NO_DRIVER);
	assert_string_equal(d.package, "intel-media-va-driver-non-free");
	assert_string_equal(d.install_command, "sudo apt install intel-media-va-driver-non-free");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_qsv_no_intel_gpu),
		cmocka_unit_test(test_qsv_intel_second_node),
		cmocka_unit_test(test_qsv_skylake_needs_media_sdk_ubuntu),
		cmocka_unit_test(test_qsv_skylake_with_only_onevpl),
		cmocka_unit_test(test_qsv_skylake_available),
		cmocka_unit_test(test_qsv_arc_needs_onevpl_per_distro),
		cmocka_unit_test(test_qsv_media_sdk_per_distro),
		cmocka_unit_test(test_qsv_tiger_lake_either_runtime),
		cmocka_unit_test(test_qsv_unknown_intel_id_is_onevpl),
		cmocka_unit_test(test_qsv_unsupported_gpu),
		cmocka_unit_test(test_qsv_no_access),
		cmocka_unit_test(test_qsv_no_driver),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
