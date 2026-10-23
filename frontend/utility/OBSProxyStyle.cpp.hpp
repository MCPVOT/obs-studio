#include "frontend/build/frontend-utility.pch.hpp"

int OBSProxyStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
			     QStyleHintReturn *returnData) const
{
	if (hint == SH_ComboBox_AllowWheelScrolling) {
		return 0;
	}
	if (hint == SH_ComboBox_UseNativePopup && UseNativeComboBoxPopup()) {
		return 1;
	}

	return QProxyStyle::styleHint(hint, option, widget, returnData);
}

int OBSInvisibleCursorProxyStyle::pixelMetric(PixelMetric metric, const QStyleOption *option,
					      const QWidget *widget) const
{

	if (metric == PM_TextCursorWidth) {
		return 0;
	}

	return QProxyStyle::pixelMetric(metric, option, widget);
}
