// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _GT_BASE3DTOOL_H
#define _GT_BASE3DTOOL_H

#include "legacy/core/macros/macros.h"
#include "legacy/core/msg/msg_def.h"
#include "legacy/core/types/types.h"
#include "legacy/render/rhi3d/public/device/render_device.h"
#include "legacy/render/scene3d/scene/scene.h"
#include "legacy/tool/base/basetool.h"
#include "legacy/tool/defs.h"
#include "legacy/tool/abi/t_iatool.h"
#include "legacy/tool/abi/t_msg.h"
#include "tool/draft/draft.h"

using namespace render;
using namespace base;
using namespace tool;

namespace tool {

// Shared 3D IA tool base: 3D render device + scene.
class SmtBase3DTool : public SmtIATool {
 public:
  SmtBase3DTool();
  ~SmtBase3DTool() override;

  virtual int Init(LP3DRENDERDEVICE p3DRenderDevice, SmtScene* pScene,
                   HWND hWnd, pfnToolCallBack pfnCallBack = nullptr,
                   void* pToFollow = nullptr);

  LP3DRENDERDEVICE GetRenderDevice(void) { return m_p3DRenderDevice; }
  SmtScene* GetScene(void) { return m_pScene; }

  virtual void apply_draft(const tool::Draft& draft) { (void)draft; }

 protected:
  LP3DRENDERDEVICE m_p3DRenderDevice;
  SmtScene* m_pScene;
};
}  // namespace tool

#endif  //_GT_BASE3DTOOL_H
