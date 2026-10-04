// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _GT_BASETOOL_H
#define _GT_BASETOOL_H

#include <cstring>

#include "gis/geo/ops/geometry_traits.h"
#include "gis/map/map.h"
#include "legacy/core/macros/macros.h"
#include "legacy/core/msg/msg_def.h"
#include "legacy/gis/present/carto/style_api.h"
#include "legacy/render/rhi2d/public/device/renderdevice.h"
#include "legacy/sys/sysmanager.h"
#include "legacy/tool/defs.h"
#include "legacy/tool/abi/t_iatool.h"
#include "legacy/tool/abi/t_msg.h"
#include "tool/draft/draft.h"

using namespace gis;
using namespace render;
using namespace base;
using namespace tool;
using namespace sys;

#define SMT_IATOOL_MSG_KEY(lMsg) (SMT_MSG_KEY(lMsg, m_hWnd))

#define SMT_IATOOL_APPEND_MSG(lMsg)       \
  {                                       \
    append_msg(SMT_IATOOL_MSG_KEY(lMsg)); \
  }

namespace tool {
extern HINSTANCE g_hInstance;

// Shared 2D IA tool base: render device + oper map + keyboard zoom/pan.
class SmtBaseTool : public SmtIATool {
 public:
  SmtBaseTool();
  ~SmtBaseTool() override;

  virtual int Init(LPRENDERDEVICE pMrdRenderDevice, Map* pOperSmtMap,
                   HWND hWnd, pfnToolCallBack pfnCallBack = nullptr,
                   void* pToFollow = nullptr);

  LPRENDERDEVICE GetRenderDevice(void) { return m_pRenderDevice; }

  virtual void SetOperMap(Map* pOperSmtMap) { m_pOperMap = pOperSmtMap; }
  virtual void GetOperMap(Map*& pOperSmtMap) { pOperSmtMap = m_pOperMap; }

  void SetToolStyleName(const char* name) {
    std::strcpy(m_szStyleName, name);
  }
  const char* GetToolStyleName(void) { return m_szStyleName; }

  int KeyDown(uint nChar, uint nRepCnt, uint nFlags) override;

  // Workspace owns the Interaction; leftover applies the completed draft.
  virtual void apply_draft(const tool::Draft& draft) { (void)draft; }

 protected:
  LPRENDERDEVICE m_pRenderDevice;
  Map* m_pOperMap;

  char m_szStyleName[MAX_STYLENAME_LENGTH];

  double m_fScaleDelt;
};
}  // namespace tool

#endif  //_GT_BASETOOL_H
