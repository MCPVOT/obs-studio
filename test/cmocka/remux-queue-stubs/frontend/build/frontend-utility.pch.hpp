/* Test stand-in for frontend/build/frontend-utility.pch.hpp, for
 * test_remux_queue_model: only what utility/RemuxQueueModel.cpp takes from
 * it. This directory comes first on the test's include path. */

#pragma once

#include "frontend/utility/RemuxQueueModel.hpp"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QStyle>

/* OBSApp's translation lookup; the test reads no header text. */
inline QString QTStr(const char *lookupVal)
{
	return QString::fromUtf8(lookupVal);
}
