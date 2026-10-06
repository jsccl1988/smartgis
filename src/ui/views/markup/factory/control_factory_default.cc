// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/markup/factory/control_factory.h"

#include "ui/views/markup/factory/register_markup_tags.h"
#include "ui/views/primitives/register_markup_controls.h"

namespace ui {
namespace views {

// Aggregation TU: wires layout + primitives + GIS *placeholder* registrations.
// Real GIS panels live in //src/ui/gis; placeholders stay here so the toolkit
// DLL does not link product GIS horizon. Kept out of control_factory.cc so the
// registry core stays free of concrete control includes (and out of views_kernel).
ControlFactory ControlFactory::make_default() {
  ControlFactory f;
  register_markup_layout_tags(&f);
  register_primitive_markup_tags(&f);
  register_gis_placeholder_markup_tags(&f);
  return f;
}

}  // namespace views
}  // namespace ui
