#include "shared/qt/plain-text-edit/build/qt-plain-text-edit.pch.hpp"

OBSPlainTextEdit::OBSPlainTextEdit(QWidget *parent, bool monospace) : QPlainTextEdit(parent)
{
	// Fix display of tabs & multiple spaces
	document()->setDefaultStyleSheet("font { white-space: pre; }");

	if (monospace) {
		const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

		setStyleSheet(QString("font-family: %1; font-size: %2pt;")
				      .arg(fixedFont.family(), QString::number(fixedFont.pointSize())));
	}
}
