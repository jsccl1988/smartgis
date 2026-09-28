// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_GIS_H_
#define UI_GIS_GIS_H_

// Product GIS chrome on //src/ui/views (panels + GIS dialogs).
// Includes are "ui/gis/<area>/…". Namespace stays ui::views for now.
// See docs/build/ui-views-skia.md and
// docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md.

#include "ui/ui_export.h"

// Dialogs (GIS product)
#include "ui/gis/dialogs/add_basemap_dialog.h"
#include "ui/gis/dialogs/att_struct_dialog.h"
#include "ui/gis/dialogs/create_datasource_dialog.h"
#include "ui/gis/dialogs/create_layer_dialog.h"
#include "ui/gis/dialogs/create_map_dialog.h"

// Panels
#include "ui/gis/shell/ambox_view.h"
#include "ui/gis/shell/atmosphere_panel.h"
#include "ui/gis/debug/debug_console_panel.h"
#include "ui/gis/debug/diagnostic_tools_panel.h"
#include "ui/gis/debug/render_trace_panel.h"
#include "ui/gis/inspect/attribute_table.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/shell/chart_view.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/gis/catalog/layer_tree.h"
#include "ui/gis/analysis/processing_panel.h"
#include "ui/gis/inspect/measure_panel.h"
#include "ui/gis/inspect/selection_panel.h"
#include "ui/gis/style/symbology_panel.h"
#include "ui/gis/style/legend_panel.h"
#include "ui/gis/style/layer_properties_panel.h"
#include "ui/gis/analysis/geoprocessing_history_panel.h"
#include "ui/gis/analysis/spatial_analysis_panel.h"
#include "ui/gis/shell/status_bar.h"

#endif  // UI_GIS_GIS_H_
