// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/legacy_cmd.h"

#include "tool/legacy_msg.h"

namespace plugin {

const char* command_id_from_am_msg(long msg) {
  if (const char* id = tool::command_id_from_gt_msg(msg)) {
    return id;
  }
  switch (msg) {
    case kAmMsgDemLoadTin:
      return "dem.load_tin";
    case kAmMsgDemLoadGrid:
      return "dem.load_grid";
    case kAmMsgDemAbout:
      return "dem.about";
    case kAmMsgProjDoPrj:
      return "proj.do_prj";
    case kAmMsgPrintPreview:
      return "print.preview";
    case kAmMsgModel3dAddPointcloud:
      return "model3d.add_pointcloud";
    case kAmMsgModel3dSphere:
      return "model3d.add_sphere";
    case kAmMsgModel3dWater:
      return "model3d.add_water";
    case kAmMsgModel3dTerrainGrid:
      return "model3d.add_terrain_grid";
    case kAmMsgModel3dTerrainTin:
      return "model3d.add_terrain_tin";
    case kAmMsgModel3dCreateTin:
      return "model3d.create_tin";
    case kAmMsgModel3dLayerPoints:
      return "model3d.layer_points_to_3d";
    case kAmMsgModel3dLayerLines:
      return "model3d.layer_lines_to_3d";
    case kAmMsgModel3dLayerPolygons:
      return "model3d.layer_polygons_to_3d";
    case kAmMsgOrthogridInputBoundary0:
      return "baogrid.input_boundary_0";
    case kAmMsgOrthogridInputBoundary2:
      return "baogrid.input_boundary_2";
    case kAmMsgOrthogridSaveBoundary:
      return "baogrid.save_boundary";
    case kAmMsgOrthogridLoadBoundary:
      return "baogrid.load_boundary";
    default:
      return nullptr;
  }
}

}  // namespace plugin
