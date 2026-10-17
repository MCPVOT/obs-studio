#include "frontend/build/frontend-components.pch.hpp"

BrowserToolbar::BrowserToolbar(QWidget *parent, OBSSource source)
	: SourceToolbar(parent, source),
	  ui(new Ui_BrowserSourceToolbar)
{
	ui->setupUi(this);
}

BrowserToolbar::~BrowserToolbar() {}

void BrowserToolbar::on_refresh_clicked()
{
	OBSSource source = GetSource();
	if (!source) {
		return;
	}

	obs_property_t *p = obs_properties_get(props, "refreshnocache");
	obs_property_button_clicked(p, source.Get());
}
