// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_LEGACY_MSG_H_
#define TOOL_LEGACY_MSG_H_

namespace tool {

// Leftover GT_MSG_CMD_* values. Keep in sync with legacy/tool/group/defs.h.
enum : long {
  kGtMsgViewZoomIn = 0x3002,
  kGtMsgViewZoomOut = 0x3003,
  kGtMsgViewPan = 0x3004,
  kGtMsgViewFull = 0x3005,
  kGtMsgViewRefresh = 0x3006,
  kGtMsgWsViewZoomIn = 0x3016,
  kGtMsgWsViewZoomOut = 0x3017,
  kGtMsgWsViewPan = 0x3018,
  kGtMsgWsViewFull = 0x3019,
  kGtMsgWsViewRefresh = 0x301A,
  kGtMsgSelectPoint = 0x302B,
  kGtMsgSelectRect = 0x302C,
  kGtMsgSelectPolygon = 0x302D,
  kGtMsgSelectClear = 0x302E,
  kGtMsgFlashStart = 0x3040,
  kGtMsgFlashStop = 0x3041,
  kGtMsgAppendPointChildImage = 0x3055,
  kGtMsgAppendPointAnno = 0x3056,
  kGtMsgAppendPointDot = 0x3057,
  kGtMsgAppendLineSplineLag = 0x3058,
  kGtMsgAppendLineSplineBzer = 0x3059,
  kGtMsgAppendLineSplineB = 0x305A,
  kGtMsgAppendLineSpline3 = 0x305B,
  kGtMsgAppendLineString = 0x305C,
  kGtMsgAppendLineRect = 0x305D,
  kGtMsgAppendLineArc = 0x305E,
  kGtMsgAppendLineRing = 0x305F,
  kGtMsgAppendSurfFan = 0x3060,
  kGtMsgAppendSurfRect = 0x3061,
  kGtMsgAppendPolygon = 0x3062,
  kGtMsg3dViewTrackball = 0x3074,
  kGtMsg3dViewSphere = 0x3075,
  kGtMsg3dViewFps = 0x3076,
  kGtMsg3dViewFull = 0x3079,
};

// Maps leftover GT_MSG_CMD_* to v1 command ids. nullptr if unknown.
const char* command_id_from_gt_msg(long msg);

}  // namespace tool

#endif  // TOOL_LEGACY_MSG_H_
