#include "frontend/build/frontend-components.pch.hpp"

AudioCaptureToolbar::AudioCaptureToolbar(QWidget *parent, OBSSource source) : ComboSelectToolbar(parent, source) {}

void AudioCaptureToolbar::Init()
{
	delete ui->activateButton;
	ui->activateButton = nullptr;

	obs_module_t *mod = obs_get_module(GetPlatformCaptureModuleId(PlatformCaptureKind::Audio));
	if (!mod) {
		return;
	}

	const char *device_str =
		obs_module_get_locale_text(mod, GetPlatformCaptureLabelKey(PlatformCaptureKind::Audio));
	ui->deviceLabel->setText(device_str);

	prop_name = "device_id";

	ComboSelectToolbar::Init();
}
