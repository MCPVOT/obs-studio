/* Test stand-in for the frontend.utility precompiled header, for
 * test_undo_stack_enable: what frontend/utility/undo_stack.cpp.hpp uses. */

#pragma once

#include <QString>

#include <functional>
#include <memory>

#include "undo_stack.hpp"

/* The real QTStr() translates; the test checks enabled state, not text. */
static inline QString QTStr(const char *lookup)
{
	return QString::fromUtf8(lookup);
}
