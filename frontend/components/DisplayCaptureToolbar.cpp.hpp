#include "frontend/build/frontend-components.pch.hpp"

DisplayCaptureToolbar::DisplayCaptureToolbar(QWidget *parent, OBSSource source) : ComboSelectToolbar(parent, source) {}

void DisplayCaptureToolbar::Init()
{
	delete ui->activateButton;
	ui->activateButton = nullptr;

	obs_module_t *mod = obs_get_module(GetPlatformCaptureModuleId(PlatformCaptureKind::Display));
	if (!mod) {
		return;
	}

	const char *device_str =
		obs_module_get_locale_text(mod, GetPlatformCaptureLabelKey(PlatformCaptureKind::Display));
	ui->deviceLabel->setText(device_str);

	prop_name = GetPlatformCaptureProperty(PlatformCaptureKind::Display, is_int);

	ComboSelectToolbar::Init();
}
