// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_PLUGIN_RUNTIME_BRIDGE_CMD_H_
#define LEGACY_PLUGIN_RUNTIME_BRIDGE_CMD_H_

#include "legacy/tool/msg/msg.h"

namespace plugin {

// Header-only AM_MSG_* → CommandCatalog ids. Physical home:
// legacy/plugin/runtime/bridge/ (GN //src/legacy/plugin/runtime:bridge).

// Leftover AppendFuncItems / Notify longs. Keep in sync with each *plug.cpp
// AM_MSG_CMD_* table (SMT_MSG_USER_BEGIN = 0x4001). Not a third int bus;
// these only exist so leftover menus resolve to CommandCatalog ids.
enum : long {
  kAmMsgUserBegin = 0x4001,
  kAmMsgModel3dAddPointcloud = 0x4002,
  kAmMsgModel3dSphere = 0x4003,
  kAmMsgModel3dWater = 0x4004,
  kAmMsgModel3dTerrainGrid = 0x4005,
  kAmMsgModel3dTerrainTin = 0x4006,
  kAmMsgModel3dCreateTin = 0x4007,
  kAmMsgModel3dLayerPoints = 0x4008,
  kAmMsgModel3dLayerLines = 0x4009,
  kAmMsgModel3dLayerPolygons = 0x400A,
  // Leftover AM_MSG_* in *plug.cpp; catalog ids are baogrid.* (plugin-host
  // spec).
  kAmMsgOrthogridInputBoundary0 = 0x4034,
  kAmMsgOrthogridInputBoundary2 = 0x4035,
  kAmMsgOrthogridSaveBoundary = 0x4036,
  kAmMsgOrthogridLoadBoundary = 0x4037,
  kAmMsgDemLoadTin = 0x4066,
  kAmMsgDemLoadGrid = 0x4067,
  kAmMsgDemAbout = 0x4068,
  kAmMsgProjDoPrj = 0x4098,
  kAmMsgPrintPreview = 0x40CA,
};

// Maps leftover plugin Notify / GT_MSG_* (via command_id_from_gt_msg) to a
// catalog id. nullptr if unknown.
inline const char* command_id_from_am_msg(long msg) {
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

#endif  // LEGACY_PLUGIN_RUNTIME_BRIDGE_CMD_H_
