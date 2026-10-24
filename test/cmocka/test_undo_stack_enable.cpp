/* Edit > Undo must stay disabled while there is nothing to undo (#2222).
 *
 * undo_stack::enable_internal() enabled actionMainUndo unconditionally, while
 * it enabled actionMainRedo only with redo entries. Any disable()/enable() or
 * push_disabled()/pop_disabled() span (loading a scene collection, the
 * Filters dialog, a drag in the preview) therefore left Undo enabled on an
 * empty stack: a live menu item, and with #2214 a toolbar button, that does
 * nothing.
 *
 * Builds the real frontend/utility/undo_stack.cpp.hpp against stand-ins for
 * the OBSBasic.ui header and the frontend's precompiled header
 * (undo-stack-stubs/), on Qt's offscreen platform: Qt Widgets only, no
 * display, no frontend build and no libobs. */

#include <QApplication>

#include "undo_stack.hpp"

#include <memory>
#include <string>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
extern "C" {
#include <cmocka.h>
}

static void noop(const std::string &) {}

static void enable_keeps_empty_undo_disabled(void **)
{
	std::unique_ptr<Ui::OBSBasic> ui = std::make_unique<Ui::OBSBasic>();
	undo_stack stack(ui);
	stack.clear();
	assert_false(ui->actionMainUndo->isEnabled());

	stack.disable();
	stack.enable();
	assert_false(ui->actionMainUndo->isEnabled());
	assert_false(ui->actionMainRedo->isEnabled());

	stack.push_disabled();
	stack.pop_disabled();
	assert_false(ui->actionMainUndo->isEnabled());
	assert_false(ui->actionMainRedo->isEnabled());
}

static void enable_restores_undo_with_entries(void **)
{
	std::unique_ptr<Ui::OBSBasic> ui = std::make_unique<Ui::OBSBasic>();
	undo_stack stack(ui);
	stack.clear();
	stack.add_action("Rename", noop, noop, "a", "b");
	assert_true(ui->actionMainUndo->isEnabled());

	stack.disable();
	assert_false(ui->actionMainUndo->isEnabled());
	stack.enable();
	assert_true(ui->actionMainUndo->isEnabled());
	assert_false(ui->actionMainRedo->isEnabled());

	/* Undo the only entry: Undo empty, Redo has it, across a span too. */
	stack.undo();
	stack.push_disabled();
	stack.pop_disabled();
	assert_false(ui->actionMainUndo->isEnabled());
	assert_true(ui->actionMainRedo->isEnabled());
}

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QApplication app(argc, argv);
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(enable_keeps_empty_undo_disabled),
		cmocka_unit_test(enable_restores_undo_with_entries),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
