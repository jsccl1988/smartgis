// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_TOOL_GROUP_INPUT_DRAFT_TO_OGR_H_
#define LEGACY_TOOL_GROUP_INPUT_DRAFT_TO_OGR_H_

#include "legacy/core/types/scalars.h"
#include "legacy/render/rhi2d/public/device/renderdevice.h"
#include "tool/draft/draft.h"

class OGRGeometry;
class OGRPoint;

namespace tool {

// Shared Draft (view px) → OGR geometry for leftover Input* / Append tools.
// Caller owns the returned pointer (delete / SMT_SAFE_DELETE).
OGRPoint* ogr_point_from_draft(render::LPRENDERDEVICE device,
                               const Draft& draft);
OGRGeometry* ogr_line_from_draft(render::LPRENDERDEVICE device,
                                 const Draft& draft, ushort line_type);
OGRGeometry* ogr_region_from_draft(render::LPRENDERDEVICE device,
                                   const Draft& draft);

}  // namespace tool

#endif  // LEGACY_TOOL_GROUP_INPUT_DRAFT_TO_OGR_H_
