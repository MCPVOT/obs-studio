#pragma once

#include <QCheckBox>
#include <QColorDialog>
#include <QFontDialog>
#include <QWidget>
#include <cstdint>

namespace qt_platform {
void tweak_checkbox(QCheckBox *checkbox);
void raise_after_dialog(QWidget *widget);
QColorDialog::ColorDialogOptions color_dialog_options();
QFontDialog::FontDialogOptions font_dialog_options();
uint32_t translate_modifiers(Qt::KeyboardModifiers mods);
} // namespace qt_platform
