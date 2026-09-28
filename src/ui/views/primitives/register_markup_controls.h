// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_REGISTER_MARKUP_CONTROLS_H_
#define UI_VIEWS_PRIMITIVES_REGISTER_MARKUP_CONTROLS_H_

#include "ui/ui_export.h"

namespace ui {
namespace views {

class ControlFactory;

// Registers README primitive tags (label, button, textfield, …) on |factory|.
// Owned by the primitives layer so ControlFactory stays type-agnostic.
UI_EXPORT void register_primitive_markup_tags(ControlFactory* factory);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_REGISTER_MARKUP_CONTROLS_H_
