#include "frontend/build/frontend-widgets.pch.hpp"

StatusBarWidget::StatusBarWidget(QWidget *parent) : QWidget(parent), ui(new Ui::StatusBarWidget)
{
	ui->setupUi(this);
}

StatusBarWidget::~StatusBarWidget() {}
