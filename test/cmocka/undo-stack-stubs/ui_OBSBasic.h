/* Test stand-in for the uic header of frontend/forms/OBSBasic.ui, for
 * test_undo_stack_enable: only the two actions undo_stack drives. */

#pragma once

#include <QAction>

namespace Ui {
class OBSBasic {
public:
	QAction *actionMainUndo = new QAction();
	QAction *actionMainRedo = new QAction();
	~OBSBasic()
	{
		delete actionMainUndo;
		delete actionMainRedo;
	}
};
} // namespace Ui
