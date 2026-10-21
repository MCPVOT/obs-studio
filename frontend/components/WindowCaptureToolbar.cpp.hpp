#include "frontend/build/frontend-components.pch.hpp"

WindowCaptureToolbar::WindowCaptureToolbar(QWidget *parent, OBSSource source) : ComboSelectToolbar(parent, source) {}

void WindowCaptureToolbar::Init()
{
	delete ui->activateButton;
	ui->activateButton = nullptr;

	obs_module_t *mod = obs_get_module(GetPlatformCaptureModuleId(PlatformCaptureKind::Window));
	if (!mod) {
		return;
	}

	const char *device_str =
		obs_module_get_locale_text(mod, GetPlatformCaptureLabelKey(PlatformCaptureKind::Window));
	ui->deviceLabel->setText(device_str);

	prop_name = GetPlatformCaptureProperty(PlatformCaptureKind::Window, is_int);

	ComboSelectToolbar::Init();
}
