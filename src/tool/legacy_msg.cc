// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/legacy_msg.h"

namespace tool {

const char* command_id_from_gt_msg(long msg) {
  switch (msg) {
    case kGtMsgViewZoomIn:
    case kGtMsgWsViewZoomIn:
      return "view.zoom_in";
    case kGtMsgViewZoomOut:
    case kGtMsgWsViewZoomOut:
      return "view.zoom_out";
    case kGtMsgViewPan:
    case kGtMsgWsViewPan:
      return "view.pan";
    case kGtMsgViewFull:
    case kGtMsgWsViewFull:
      return "view.full";
    case kGtMsgViewRefresh:
    case kGtMsgWsViewRefresh:
      return "view.refresh";
    case kGtMsgSelectPoint:
      return "selection.point";
    case kGtMsgSelectRect:
      return "selection.rect";
    case kGtMsgSelectPolygon:
      return "selection.polygon";
    case kGtMsgSelectClear:
      return "selection.clear";
    case kGtMsgAppendPointChildImage:
    case kGtMsgAppendPointAnno:
    case kGtMsgAppendPointDot:
      return "edit.append.point";
    case kGtMsgAppendLineSplineLag:
    case kGtMsgAppendLineSplineBzer:
    case kGtMsgAppendLineSplineB:
    case kGtMsgAppendLineSpline3:
    case kGtMsgAppendLineString:
    case kGtMsgAppendLineRect:
    case kGtMsgAppendLineArc:
    case kGtMsgAppendLineRing:
      return "edit.append.linestring";
    case kGtMsgAppendSurfFan:
    case kGtMsgAppendSurfRect:
    case kGtMsgAppendPolygon:
      return "edit.append.polygon";
    case kGtMsgFlashStart:
      return "flash.start";
    case kGtMsgFlashStop:
      return "flash.stop";
    case kGtMsg3dViewTrackball:
      return "view3d.trackball";
    case kGtMsg3dViewSphere:
      return "view3d.sphere";
    case kGtMsg3dViewFps:
      return "view3d.fps";
    case kGtMsg3dViewFull:
      return "view3d.full";
    default: {
      // Leftover plugins post SMT_MSG_KEY(GT_MSG_*, HWND) = MAKELONG(msg, hwnd).
      const long low = static_cast<long>(static_cast<unsigned long>(msg) & 0xffffu);
      if (low != msg) {
        return command_id_from_gt_msg(low);
      }
      return nullptr;
    }
  }
}

}  // namespace tool
