#include "frontend/build/frontend-utility-replay-buffer-estimate.pch.hpp"

void SetReplayBufferEstimate(QLabel *label, const QString &text, bool warning)
{
	label->setText(text);
	label->setProperty("class", warning ? "text-warning" : "");
	label->style()->polish(label);
}
