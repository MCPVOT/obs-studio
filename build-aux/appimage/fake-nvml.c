/* Stand-in for the NVIDIA driver's libnvidia-ml.so.1, for the AppImage tests
 * (obs-rust/obs-studio#1028). CI runners have no NVIDIA GPU, so the tests
 * install this where a driver puts NVML (/run/opengl-driver/lib on NixOS, the
 * host library directory elsewhere) and check that OBS's GPU usage sampler
 * (rust/obs-gpu-status, crate::nvml) finds and initializes it: nvmlInit_v2
 * appends this library's own path to the file named by OBS_TEST_NVML_MARKER.
 * It exports the functions crate::nvml requires; no device is ever found.
 * Build: gcc -shared -fPIC -o libnvidia-ml.so.1 fake-nvml.c -ldl */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#define NVML_SUCCESS 0
#define NVML_ERROR_NOT_FOUND 6

int nvmlInit_v2(void)
{
	const char *marker = getenv("OBS_TEST_NVML_MARKER");
	Dl_info info;

	if (marker && dladdr((void *)nvmlInit_v2, &info) && info.dli_fname) {
		FILE *f = fopen(marker, "a");
		if (f) {
			fprintf(f, "%s\n", info.dli_fname);
			fclose(f);
		}
	}
	return NVML_SUCCESS;
}

int nvmlDeviceGetHandleByPciBusId_v2(const char *bus_id, void **device)
{
	(void)bus_id;
	(void)device;
	return NVML_ERROR_NOT_FOUND;
}

int nvmlDeviceGetUtilizationRates(void *device, void *rates)
{
	(void)device;
	(void)rates;
	return NVML_ERROR_NOT_FOUND;
}
