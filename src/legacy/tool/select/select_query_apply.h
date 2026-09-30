// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_TOOL_GROUP_SELECT_QUERY_APPLY_H_
#define LEGACY_TOOL_GROUP_SELECT_QUERY_APPLY_H_

#include "gis/model/layer/layer.h"
#include "gis/model/map/map.h"
#include "legacy/core/macros/macros.h"
#include "legacy/gis/datasource/datasource_mgr.h"
#include "legacy/render/rhi2d/public/device/renderdevice.h"
#include "tool/draft/draft.h"

class OGRGeometry;

namespace tool {

// Builds map-CRS query geometry from a select Draft. Caller owns the result
// (delete via SMT_SAFE_DELETE). Returns nullptr if draft cannot form a geom.
OGRGeometry* query_geom_from_select_draft(render::LPRENDERDEVICE device,
                                          const Draft& draft, bool as_circle);

// Shared select side-effects used by SmtSelectTool notify / apply_draft.
void clear_select_scratch(gis::ScratchLayer& scratch);
float select_query_margin_lp(render::LPRENDERDEVICE device, double dp_margin);
void refresh_select_fea_type(SmtMap* map, int& fea_type);
void post_select_flash_data(HWND hwnd, gis::ScratchLayer* scratch,
                            int* fea_type);
void post_select_flash_start(HWND hwnd);

// Runs QueryFeature into |scratch| using |gq| (must already hold pQueryGeom),
// then posts flash. |point_query| selects GT_MSG_RET_INPUT_POINT vs LINE
// (multi-fid dialog only on line).
void run_select_query(render::LPRENDERDEVICE device, SmtMap* map,
                      gis::ScratchLayer& scratch, SmtGQueryDesc& gq,
                      SmtPQueryDesc& pq, int& fea_type, double dp_margin,
                      HWND hwnd, bool point_query);

}  // namespace tool

#endif  // LEGACY_TOOL_GROUP_SELECT_QUERY_APPLY_H_
