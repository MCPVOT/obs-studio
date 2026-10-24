#pragma once

/* Keeps the relative sizes of docks that share a row in the main window
 * when the window shrinks below the docks' minimum sizes and grows again
 * (obs-rust/obs-studio#2225). Header-only, Qt Widgets only, so it can be
 * unit tested without the frontend.
 *
 * Qt scales docks in proportion while the window resizes, until a dock
 * reaches its minimum size. From then on the docks at their minimum stay
 * there, and when the window grows again Qt shares the new space out
 * equally: a layout the user sized by hand comes back with every dock the
 * same size. This helper remembers each row's shares while no dock in it is
 * at its minimum and puts them back when the window grows.
 *
 * A row whose docks change size but whose total stays the same was resized
 * from inside (a dragged separator): its shares are recorded. A row whose
 * total changed was resized with the window: its saved shares are put back
 * once they fit. */

#include <QDockWidget>
#include <QEvent>
#include <QList>
#include <QMainWindow>
#include <QObject>

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

class DockProportions : public QObject {
public:
	explicit DockProportions(QMainWindow *window) : QObject(window), window(window)
	{
		window->installEventFilter(this);
		for (QDockWidget *dock : window->findChildren<QDockWidget *>(Qt::FindDirectChildrenOnly)) {
			dock->installEventFilter(this);
		}
	}

protected:
	bool eventFilter(QObject *obj, QEvent *event) override
	{
		if (obj == window && event->type() == QEvent::ChildAdded) {
			auto *child = static_cast<QChildEvent *>(event)->child();
			if (auto *dock = qobject_cast<QDockWidget *>(child)) {
				dock->installEventFilter(this);
			}
		} else if (obj != window && event->type() == QEvent::Resize && !updateQueued) {
			/* The layout resizes the docks of a row one at a time:
			 * look at the row once it has placed all of them. */
			updateQueued = true;
			QMetaObject::invokeMethod(this, [this] { Update(); }, Qt::QueuedConnection);
		}
		return false;
	}

private:
	struct Row {
		Qt::Orientation orientation;
		QList<QDockWidget *> docks;
	};

	struct Saved {
		int total;
		QList<double> shares;
	};

	QMainWindow *window;
	std::map<QList<QDockWidget *>, Saved> saved;
	bool updateQueued = false;

	/* Docks side by side in the same area: along the top or bottom they
	 * share a top edge and split the width; on the left or right they share
	 * a left edge and split the height. */
	QList<Row> Rows() const
	{
		std::map<std::pair<int, int>, Row> rows;
		for (QDockWidget *dock : window->findChildren<QDockWidget *>(Qt::FindDirectChildrenOnly)) {
			if (dock->isFloating() || !dock->isVisible()) {
				continue;
			}
			Qt::DockWidgetArea area = window->dockWidgetArea(dock);
			bool horizontal = area == Qt::TopDockWidgetArea || area == Qt::BottomDockWidgetArea;
			if (!horizontal && area != Qt::LeftDockWidgetArea && area != Qt::RightDockWidgetArea) {
				continue;
			}
			int edge = horizontal ? dock->y() : dock->x();
			Row &row = rows[{int(area), edge}];
			row.orientation = horizontal ? Qt::Horizontal : Qt::Vertical;
			row.docks.append(dock);
		}

		QList<Row> result;
		for (auto &entry : rows) {
			Row &row = entry.second;
			if (row.docks.size() < 2) {
				continue;
			}
			const bool horizontal = row.orientation == Qt::Horizontal;
			std::sort(row.docks.begin(), row.docks.end(), [horizontal](QDockWidget *a, QDockWidget *b) {
				return horizontal ? a->x() < b->x() : a->y() < b->y();
			});
			result.append(row);
		}
		return result;
	}

	static int Size(const QDockWidget *dock, Qt::Orientation orientation)
	{
		return orientation == Qt::Horizontal ? dock->width() : dock->height();
	}

	static int Minimum(const QDockWidget *dock, Qt::Orientation orientation)
	{
		QSize min = dock->minimumSizeHint().expandedTo(dock->minimumSize());
		return orientation == Qt::Horizontal ? min.width() : min.height();
	}

	void Update()
	{
		updateQueued = false;
		for (const Row &row : Rows()) {
			int total = 0;
			bool clamped = false;
			for (QDockWidget *dock : row.docks) {
				total += Size(dock, row.orientation);
				clamped = clamped || Size(dock, row.orientation) <= Minimum(dock, row.orientation) + 1;
			}
			if (total <= 0) {
				continue;
			}

			auto it = saved.find(row.docks);
			if (it == saved.end() || it->second.total == total) {
				/* New row, or resized from inside. A dock pushed to
				 * its minimum says nothing about the shares wanted. */
				if (!clamped) {
					saved[row.docks] = {total, Shares(row, total)};
				}
				continue;
			}

			/* Resized with the window. Keep the shares; the next
			 * change at this total comes from inside the row. */
			it->second.total = total;
			const QList<double> &shares = it->second.shares;
			bool differs = false, fits = true;
			QList<int> sizes;
			for (qsizetype i = 0; i < row.docks.size(); i++) {
				double share = double(Size(row.docks[i], row.orientation)) / total;
				differs = differs || std::abs(share - shares[i]) > 0.01;
				sizes.append(int(std::lround(shares[i] * total)));
				fits = fits && sizes.last() > Minimum(row.docks[i], row.orientation) + 1;
			}
			/* Too small for the saved shares: wait for the window to
			 * grow again. */
			if (differs && fits) {
				window->resizeDocks(row.docks, sizes, row.orientation);
			}
		}
	}

	static QList<double> Shares(const Row &row, int total)
	{
		QList<double> shares;
		for (QDockWidget *dock : row.docks) {
			shares.append(double(Size(dock, row.orientation)) / total);
		}
		return shares;
	}
};
