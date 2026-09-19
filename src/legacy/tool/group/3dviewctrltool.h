/*
File:    gt_traceballtool.h

Desc:    Smt3DViewCtrlTool,3D view control leftover shell

Version: Version 1.0

Writter:  代大艳

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_VIEW3DTOOL_H
#define _GT_VIEW3DTOOL_H

#include "legacy/render/render3d/camera.h"
#include "legacy/render/scene3d/bl3d_object.h"
#include "legacy/tool/group/base3dtool.h"
#include "tool/gestures.h"

using namespace render;

namespace tool {
class Workspace;

enum eCursorType3D {
  CursorTraceBall,
  CursorSphereCamare,
  CursorFirstPerson,
};

// Leftover 3D chrome shell: bind + notify + apply_draft camera side effects.
// Bound pointer/wheel/key input is owned by view3d.* Interaction.
class Smt3DViewCtrlTool : public SmtBase3DTool {
 public:
  Smt3DViewCtrlTool();
  virtual ~Smt3DViewCtrlTool();

 public:
  int Init(LP3DRENDERDEVICE p3DRenderDevice, SmtScene* pScene, HWND hWnd,
           pfnToolCallBack pfnCallBack = NULL, void* pToFollow = NULL);
  int AuxDraw();
  int Timer();

 public:
  int notify(long nMsg, SmtListenerMsg& param);

  int SetCursor(void);
  int KeyDown(uint nChar, uint nRepCnt, uint nFlags);
  // Bound: pointer owned by Workspace — leftover handlers are no-ops.
  int LButtonDown(uint nFlags, lPoint point) override;
  int LButtonUp(uint nFlags, lPoint point) override;
  int RButtonDown(uint nFlags, lPoint point) override;
  int RButtonUp(uint nFlags, lPoint point) override;
  int MouseMove(uint nFlags, lPoint point) override;
  int MouseWeel(uint nFlags, short zDelta, lPoint point) override;
  void apply_draft(const tool::Draft& draft) override;

  void bind_workspace(tool::Workspace* workspace) { m_workspace = workspace; }

  // Copy the live camera ETU into the leftover restore pose (after DEM frame).
  void sync_org_pose();

 protected:
  void ReplaceCamera(render::View3dCameraKind kind, float step_div);
  void TrackballProv(lPoint pos, Vector3& vec);
  void OnSetViewMode(void);
  void ApplyCameraKey(uint nChar);
  void ApplyPick(const tool::Draft& draft);
  void ApplyModeKey(uint nChar);
  void RestoreLastTarget();

 protected:
  eView3DMode m_viewMode;
  SmtPerspCamera* m_pCamera;
  Vector3 m_vOrgTarget, m_vOrgEye, m_vOrgUp;

  lPoint m_pntOrigin;
  lPoint m_pntPre;
  lPoint m_pntCur;
  bool m_bIsDrag;

  int m_nWinWidth;
  int m_nWinHeight;
  float m_fAngle;
  Vector3 m_vAxis;
  Vector3 m_vPrePos;

  HCURSOR m_hCursors[3];

  vSmt3DObjectPtrs m_vSelTargetPtrs;
  Smt3DObject* m_pLastTarget;
  SmtMaterial m_matLastTarget;
  SmtMaterial m_matSel;

  tool::Workspace* m_workspace = nullptr;
};
}  // namespace tool

#endif  //_GT_VIEW3DTOOL_H
