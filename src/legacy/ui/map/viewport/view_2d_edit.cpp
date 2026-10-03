// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"

#include "legacy/ui/map/viewport/view_2d_edit.h"

#include "base/core/log.h"
#include "content/public/view_host.h"
#include "gis/model/edit/map_edit/map_edit_session.h"
#include "gis/model/map/map.h"
#include "legacy/core/util/menu.h"
#include "legacy/gis/present/carto/stylemanager.h"
#include "legacy/sys/sysmanager.h"
#include "legacy/tool/defs.h"
#include "legacy/tool/draft/appendfeaturetool.h"
#include "legacy/ui/map/tools/bind_workspace.h"
#include "legacy/ui/shell/resource.h"
#include "tool/workspace/workspace.h"

using namespace gis;
using namespace render;
using namespace tool;
using namespace sys;

namespace ui {
IMPLEMENT_DYNCREATE(Smt2DEditXView, Smt2DXView)

Smt2DEditXView::Smt2DEditXView() {
  m_pAppendFeaTool = NULL;
  m_pMapEdits = NULL;
}

Smt2DEditXView::~Smt2DEditXView() {}

BEGIN_MESSAGE_MAP(Smt2DEditXView, Smt2DXView)
END_MESSAGE_MAP()

#ifdef _DEBUG
void Smt2DEditXView::AssertValid() const { CView::AssertValid(); }
#ifndef _WIN32_WCE
void Smt2DEditXView::Dump(CDumpContext& dc) const { CView::Dump(dc); }
#endif
#endif

bool Smt2DEditXView::InitCreate(void) { return Smt2DXView::InitCreate(); }

bool Smt2DEditXView::EndDestory(void) {
  SmtGroupToolFactory::DestoryGroupTool(m_pAppendFeaTool);
  const bool ok = Smt2DXView::EndDestory();
  delete m_pMapEdits;
  m_pMapEdits = NULL;
  return ok;
}

bool Smt2DEditXView::CreateTools(void) {
  if (!Smt2DXView::CreateTools()) {
    return false;
  }

  SmtSysManager* pSysMgr = SmtSysManager::get_singleton_ptr();
  SmtStyleConfig styleConfig = pSysMgr->get_sys_style_config();

  SmtGroupToolFactory::CreateGroupTool(m_pAppendFeaTool,
                                       GroupToolType::GTT_AppendFeature);
  if (NULL == m_pAppendFeaTool) {
    return false;
  }

  m_pAppendFeaTool->SetToolStyleName(styleConfig.szAuxStyle);
  if (m_pAppendFeaTool->Init(m_pRenderDevice, m_pSmtOperMap, m_hWnd) !=
      SMT_ERR_NONE) {
    return false;
  }

  delete m_pMapEdits;
  m_pMapEdits = new gis::MapEditSession(m_pSmtOperMap);
  reset_view_host(new content::ViewHost(m_pMapEdits));
  tool::Workspace* ws = view_host() ? view_host()->workspace() : NULL;
  detail::bind_2d_tools_workspace(m_pViewCtrlTool, m_pSelectTool, m_pFlashTool,
                                  m_pAppendFeaTool, ws, m_hWnd);

  LOGGING(LOG_INFO, "Init 2DEditView GroupTools OK!");
  return true;
}

void Smt2DEditXView::SetOperMap(SmtMap* pSmtMap) {
  Smt2DXView::SetOperMap(pSmtMap);
  if (m_pAppendFeaTool) {
    m_pAppendFeaTool->SetOperMap(pSmtMap);
  }
  if (m_pMapEdits) {
    m_pMapEdits->bind_map(pSmtMap);
  }
}

bool Smt2DEditXView::CreateContexMenu() {
  Smt2DXView::CreateContexMenu();
  append_listener_menu(m_hContexMenu, m_pAppendFeaTool, FIM_2DVIEW);
  LOGGING(LOG_INFO, "Init 2DEditView ContexMenu OK!");
  return true;
}

bool Smt2DEditXView::CreateMainMenu() {
  Smt2DXView::CreateMainMenu();
  attach_listener_popup(m_hMainMenu, m_pAppendFeaTool, FIM_2DMFMENU,
                        m_pAppendFeaTool->get_name(), 0, MF_BYPOSITION);
  LOGGING(LOG_INFO, "Init 2DEditView MainMenu OK!");
  return true;
}

}  // namespace ui
