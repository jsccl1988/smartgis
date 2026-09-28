// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _GT_BASE3DTOOL_H
#define _GT_BASE3DTOOL_H

#include "legacy/core/bas_struct.h"
#include "legacy/core/core.h"
#include "legacy/core/msg.h"
#include "legacy/render/rhi3d/public/device/3drenderdevice.h"
#include "legacy/render/scene3d/scene/scene.h"
#include "legacy/tool/group/base/basetool.h"
#include "legacy/tool/group/defs.h"
#include "legacy/tool/iatool/t_iatool.h"
#include "legacy/tool/iatool/t_msg.h"
#include "tool/draft/draft.h"

using namespace render;
using namespace base;
using namespace tool;

namespace tool {
class SmtBase3DTool : public SmtIATool {
 public:
  SmtBase3DTool();
  virtual ~SmtBase3DTool();

 public:
  virtual int Init(LP3DRENDERDEVICE p3DRenderDevice, SmtScene *pScene,
                   HWND hWnd, pfnToolCallBack pfnCallBack = NULL,
                   void *pToFollow = NULL);

 public:
  LP3DRENDERDEVICE GetRenderDevice(void) { return m_p3DRenderDevice; }
  SmtScene *GetScene(void) { return m_pScene; }

  virtual void apply_draft(const tool::Draft &draft) { (void)draft; }

 protected:
  LP3DRENDERDEVICE m_p3DRenderDevice;
  SmtScene *m_pScene;
};
}  // namespace tool

#endif  //_GT_BASE3DTOOL_H