#include "libobs/build/libobs-graphics-ffmpeg-host-windows.pch.h"

/* The image loader's Windows host (obs-rust/obs-studio#872): JPEG XR files
 * FFmpeg cannot decode go through WIC, with PQ HDR converted to scRGB. */

static float pq_to_linear(float u)
{
	const float common = powf(u, 1.f / 78.84375f);
	return powf(fabsf(max(common - 0.8359375f, 0.f) / (18.8515625f - 18.6875f * common)), 1.f / 0.1593017578f);
}

static void convert_pq_to_cccs(const BYTE *intermediate, const UINT intermediate_size, BYTE *bytes)
{
	const BYTE *src_cursor = intermediate;
	const BYTE *src_cursor_end = src_cursor + intermediate_size;
	BYTE *dst_cursor = bytes;
	uint32_t rgb10;
	struct half rgba16[4];
	rgba16[3].u = 0x3c00;
	while (src_cursor < src_cursor_end) {
		memcpy(&rgb10, src_cursor, sizeof(rgb10));
		const float blue = (float)(rgb10 & 0x3ff) / 1023.f;
		const float green = (float)((rgb10 >> 10) & 0x3ff) / 1023.f;
		const float red = (float)((rgb10 >> 20) & 0x3ff) / 1023.f;
		const float red2020 = pq_to_linear(red);
		const float green2020 = pq_to_linear(green);
		const float blue2020 = pq_to_linear(blue);
		const float red709 = 1.6604910021084345f * red2020 - 0.58764113878854951f * green2020 -
				     0.072849863319884883f * blue2020;
		const float green709 = -0.12455047452159074f * red2020 + 1.1328998971259603f * green2020 -
				       0.0083494226043694768f * blue2020;
		const float blue709 = -0.018150763354905303f * red2020 - 0.10057889800800739f * green2020 +
				      1.1187296613629127f * blue2020;
		rgba16[0] = half_from_float(red709 * 125.f);
		rgba16[1] = half_from_float(green709 * 125.f);
		rgba16[2] = half_from_float(blue709 * 125.f);
		memcpy(dst_cursor, &rgba16, sizeof(rgba16));
		src_cursor += 4;
		dst_cursor += 8;
	}
}

static void *wic_image_init_internal(const char *file, IWICBitmapFrameDecode *pFrame, enum gs_color_format *format,
				     uint32_t *cx_out, uint32_t *cy_out, enum gs_color_space *space)
{
	BYTE *bytes = NULL;

	WICPixelFormatGUID pixelFormat;
	HRESULT hr = pFrame->lpVtbl->GetPixelFormat(pFrame, &pixelFormat);
	if (SUCCEEDED(hr)) {
		const bool scrgb = memcmp(&pixelFormat, &GUID_WICPixelFormat64bppRGBAHalf, sizeof(pixelFormat)) == 0;
		const bool pq10 = memcmp(&pixelFormat, &GUID_WICPixelFormat32bppBGR101010, sizeof(pixelFormat)) == 0;
		if (scrgb || pq10) {
			UINT width, height;
			hr = pFrame->lpVtbl->GetSize(pFrame, &width, &height);
			if (SUCCEEDED(hr)) {
				const UINT pitch = 8 * width;
				const UINT size = pitch * height;
				bytes = bmalloc(size);
				if (bytes) {
					bool success = false;
					if (pq10) {
						const UINT intermediate_pitch = 4 * width;
						const UINT intermediate_size = intermediate_pitch * height;
						BYTE *intermediate = bmalloc(intermediate_size);
						if (intermediate) {
							hr = pFrame->lpVtbl->CopyPixels(pFrame, NULL,
											intermediate_pitch,
											intermediate_size,
											intermediate);
							success = SUCCEEDED(hr);
							if (success) {
								convert_pq_to_cccs(intermediate, intermediate_size,
										   bytes);
							} else {
								blog(LOG_WARNING,
								     "WIC: Failed to CopyPixels intermediate for file: %s",
								     file);
							}

							bfree(intermediate);
						} else {
							blog(LOG_WARNING,
							     "WIC: Failed to allocate intermediate for file: %s", file);
						}
					} else {
						hr = pFrame->lpVtbl->CopyPixels(pFrame, NULL, pitch, size, bytes);
						success = SUCCEEDED(hr);
						if (!success) {
							blog(LOG_WARNING, "WIC: Failed to CopyPixels for file: %s",
							     file);
						}
					}

					if (success) {
						*format = GS_RGBA16F;
						*cx_out = width;
						*cy_out = height;
						*space = GS_CS_709_SCRGB;
					} else {
						bfree(bytes);
						bytes = NULL;
					}
				} else {
					blog(LOG_WARNING, "WIC: Failed to allocate for file: %s", file);
				}
			} else {
				blog(LOG_WARNING, "WIC: Failed to GetSize of frame for file: %s", file);
			}
		} else {
			blog(LOG_WARNING,
			     "WIC: Only handle GUID_WICPixelFormat32bppBGR101010 and GUID_WICPixelFormat64bppRGBAHalf for now");
		}
	} else {
		blog(LOG_WARNING, "WIC: Failed to GetPixelFormat for file: %s", file);
	}

	return bytes;
}

uint8_t *gs_image_host_fallback_load(const char *file, enum gs_color_format *format, uint32_t *cx_out, uint32_t *cy_out,
				     enum gs_color_space *space)
{
	const size_t len = strlen(file);
	if (len <= 4 && astrcmpi(file + len - 4, ".jxr") != 0) {
		blog(LOG_WARNING, "WIC: Only handle JXR for WIC images for now");
		return NULL;
	}

	BYTE *bytes = NULL;

	wchar_t *file_w = NULL;
	os_utf8_to_wcs_ptr(file, 0, &file_w);
	if (file_w) {
		IWICImagingFactory *pFactory = NULL;
		HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
					      &IID_IWICImagingFactory, &pFactory);
		if (SUCCEEDED(hr)) {
			IWICBitmapDecoder *pDecoder = NULL;
			hr = pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, file_w, NULL, GENERIC_READ,
									 WICDecodeMetadataCacheOnDemand, &pDecoder);
			if (SUCCEEDED(hr)) {
				IWICBitmapFrameDecode *pFrame = NULL;
				hr = pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFrame);
				if (SUCCEEDED(hr)) {
					bytes = wic_image_init_internal(file, pFrame, format, cx_out, cy_out, space);

					pFrame->lpVtbl->Release(pFrame);
				} else {
					blog(LOG_WARNING, "WIC: Failed to create IWICBitmapFrameDecode from file: %s",
					     file);
				}

				pDecoder->lpVtbl->Release(pDecoder);
			} else {
				blog(LOG_WARNING, "WIC: Failed to create IWICBitmapDecoder from file: %s", file);
			}

			pFactory->lpVtbl->Release(pFactory);
		} else {
			blog(LOG_WARNING, "WIC: Failed to create IWICImagingFactory");
		}

		bfree(file_w);
	} else {
		blog(LOG_WARNING, "WIC: Failed to widen file name: %s", file);
	}

	return bytes;
}
