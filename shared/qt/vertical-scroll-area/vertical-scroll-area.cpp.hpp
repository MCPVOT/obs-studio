#include "shared/qt/vertical-scroll-area/build/qt-vertical-scroll-area.pch.hpp"

void VScrollArea::resizeEvent(QResizeEvent *event)
{
	if (!!widget()) {
		widget()->setMaximumWidth(event->size().width());
	}

	QScrollArea::resizeEvent(event);
}
