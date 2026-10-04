// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_DIALOGS_DETAIL_FEATURE_INFO_GRID_H_
#define LEGACY_UI_DIALOGS_DETAIL_FEATURE_INFO_GRID_H_

#include "gis/feature/feature.h"
#include "legacy/gis/feature/model_aliases.h"
#include "legacy/ui/widgets/feature_pack/feature_pack.h"

namespace ui {
namespace detail {

// Rebuild CMFCPropertyGrid groups (geometry + filtered attributes) for a
// read-only leftover feature-info dialog.
void rebuild_feature_info_grid(CMFCPropertyGridCtrl* grid, gis::Feature* feature,
                               const CString& name_filter);

// Format a short geometry summary for the feature-info static text.
CString format_feature_geom_summary(gis::Feature* feature);

}  // namespace detail
}  // namespace ui

#endif  // LEGACY_UI_DIALOGS_DETAIL_FEATURE_INFO_GRID_H_
