/* Dock sizes after the main window is narrowed and widened again
 * (obs-rust/obs-studio#2225). OBS's default layout puts the Scenes, Sources,
 * Audio Mixer, Transitions and Controls docks in one row along the bottom.
 * Narrowing the window below the docks' minimum widths shrank each of them to
 * its minimum, and widening it again shared the width out equally, so a
 * layout the user had sized by hand came back with every dock the same width.
 *
 * Builds a QMainWindow with such a row of docks, plus the helper OBSBasic
 * installs (frontend/utility/dock-proportions.hpp), on Qt's offscreen
 * platform. */

#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QTest>

#include "dock-proportions.hpp"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
extern "C" {
#include <cmocka.h>
}

static const int dock_min_width = 150;
static const QList<int> hand_sized = {720, 224, 256, 200, 200};

struct Fixture {
	QMainWindow *window = nullptr;
	QList<QDockWidget *> docks;
};

static int setup(void **state)
{
	auto *f = new Fixture;
	f->window = new QMainWindow;
	f->window->setCentralWidget(new QLabel(QStringLiteral("preview")));
	new DockProportions(f->window);
	for (int i = 0; i < hand_sized.size(); i++) {
		auto *dock = new QDockWidget(QString::number(i));
		dock->setObjectName(QStringLiteral("dock%1").arg(i));
		dock->setWidget(new QLabel(QStringLiteral("contents")));
		dock->widget()->setMinimumWidth(dock_min_width);
		f->window->addDockWidget(Qt::BottomDockWidgetArea, dock);
		if (i) {
			f->window->splitDockWidget(f->docks.last(), dock, Qt::Horizontal);
		}
		f->docks.append(dock);
	}
	f->window->resize(1600, 900);
	f->window->show();
	QTest::qWait(50);
	f->window->resizeDocks(f->docks, hand_sized, Qt::Horizontal);
	QTest::qWait(50);
	*state = f;
	return 0;
}

static int teardown(void **state)
{
	auto *f = static_cast<Fixture *>(*state);
	delete f->window;
	delete f;
	return 0;
}

static QList<int> widths(const Fixture *f)
{
	QList<int> result;
	for (QDockWidget *dock : f->docks) {
		result.append(dock->width());
	}
	return result;
}

static void print_widths(const char *when, const QList<int> &w)
{
	printf("%s:", when);
	for (int width : w) {
		printf(" %d", width);
	}
	printf("\n");
}

/* Each dock's share of the row is within 2 percentage points of before. */
static void assert_same_shares(const QList<int> &before, const QList<int> &after)
{
	int total_before = 0, total_after = 0;
	for (int i = 0; i < before.size(); i++) {
		total_before += before[i];
		total_after += after[i];
	}
	for (int i = 0; i < before.size(); i++) {
		int share_before = before[i] * 1000 / total_before;
		int share_after = after[i] * 1000 / total_after;
		if (abs(share_before - share_after) > 20) {
			print_error("dock %d: %d.%d%% of the row before, %d.%d%% after\n", i, share_before / 10,
				    share_before % 10, share_after / 10, share_after % 10);
			fail();
		}
	}
}

static void narrow_then_wide_keeps_shares(void **state)
{
	auto *f = static_cast<Fixture *>(*state);
	const QList<int> before = widths(f);
	print_widths("before", before);

	f->window->resize(700, 900);
	QTest::qWait(50);
	print_widths("narrow", widths(f));
	/* The narrow window really clamps the docks to their minimum. */
	for (int width : widths(f)) {
		assert_true(width <= dock_min_width + 10);
	}

	f->window->resize(1600, 900);
	QTest::qWait(50);
	const QList<int> after = widths(f);
	print_widths("after", after);
	assert_same_shares(before, after);
}

static void narrow_in_steps_then_wide_keeps_shares(void **state)
{
	auto *f = static_cast<Fixture *>(*state);
	const QList<int> before = widths(f);

	/* Dragging the window edge: many small resizes. */
	for (int w = 1600; w >= 700; w -= 50) {
		f->window->resize(w, 900);
		QTest::qWait(10);
	}
	for (int w = 700; w <= 1600; w += 50) {
		f->window->resize(w, 900);
		QTest::qWait(10);
	}
	QTest::qWait(50);
	const QList<int> after = widths(f);
	print_widths("after steps", after);
	assert_same_shares(before, after);
}

static void dragged_separator_is_kept(void **state)
{
	auto *f = static_cast<Fixture *>(*state);
	/* The user re-sizes the docks by hand after the first layout. */
	const QList<int> dragged = {400, 400, 300, 200, 200};
	f->window->resizeDocks(f->docks, dragged, Qt::Horizontal);
	QTest::qWait(50);
	const QList<int> before = widths(f);

	f->window->resize(700, 900);
	QTest::qWait(50);
	f->window->resize(1600, 900);
	QTest::qWait(50);
	assert_same_shares(before, widths(f));
}

static void unclamped_resize_is_untouched(void **state)
{
	auto *f = static_cast<Fixture *>(*state);
	const QList<int> before = widths(f);

	/* Wide enough that no dock reaches its minimum: Qt keeps the shares. */
	f->window->resize(1400, 900);
	QTest::qWait(50);
	f->window->resize(1600, 900);
	QTest::qWait(50);
	assert_same_shares(before, widths(f));
}

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QApplication app(argc, argv);

	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(narrow_then_wide_keeps_shares, setup, teardown),
		cmocka_unit_test_setup_teardown(narrow_in_steps_then_wide_keeps_shares, setup, teardown),
		cmocka_unit_test_setup_teardown(dragged_separator_is_kept, setup, teardown),
		cmocka_unit_test_setup_teardown(unclamped_resize_is_untouched, setup, teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
