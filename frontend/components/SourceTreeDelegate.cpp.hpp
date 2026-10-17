#include "frontend/build/frontend-components.pch.hpp"

SourceTreeDelegate::SourceTreeDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

QSize SourceTreeDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	SourceTree *tree = qobject_cast<SourceTree *>(parent());
	QWidget *item = tree->indexWidget(index);

	if (!item) {
		return QStyledItemDelegate::sizeHint(option, index);
	}

	return (QSize(item->sizeHint()));
}
