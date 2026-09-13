// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_LEGACY_CMD_H_
#define PLUGIN_LEGACY_CMD_H_

namespace plugin {

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
  // Leftover AM_MSG_* in *plug.cpp; catalog ids are baogrid.* (plugin-host spec).
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
const char* command_id_from_am_msg(long msg);

}  // namespace plugin

#endif  // PLUGIN_LEGACY_CMD_H_
