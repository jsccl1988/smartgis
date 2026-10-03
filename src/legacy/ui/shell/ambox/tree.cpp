// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/ui/shell/ambox/tree.h"

#include "legacy/core/msg/msg_def.h"
#include "legacy/ui/shell/ambox/resource.h"
#include "legacy/ui/shell/ambox/title.h"

// SmtXAMBox
namespace ui {
IMPLEMENT_DYNAMIC(SmtXAMBox, CTreeCtrl)

SmtXAMBox::SmtXAMBox(SmtAuxModule* pAModule)
    : m_pAModule(pAModule), m_hContexMenu(NULL), m_hRoot(NULL) {}

SmtXAMBox::~SmtXAMBox() { m_pAModule = NULL; }

BEGIN_MESSAGE_MAP(SmtXAMBox, CTreeCtrl)
ON_WM_CREATE()
ON_WM_DESTROY()
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

// SmtXAMBox ��Ϣ��������
BOOL SmtXAMBox::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd,
                       UINT nID) {
  // TODO: �ڴ�����ר�ô����/����û���

  return CTreeCtrl::Create(dwStyle, rect, pParentWnd, nID);
}

int SmtXAMBox::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CTreeCtrl::OnCreate(lpCreateStruct) == -1) return -1;

  if (!InitCreate()) {
    return -1;
  }

  return 0;
}

void SmtXAMBox::OnDestroy() {
  // Drop plugin pointer before HWND teardown / FreeLibrary on close.
  m_pAModule = NULL;
  EndDestory();
  CTreeCtrl::OnDestroy();
}

bool SmtXAMBox::InitCreate(void) {
  // AFX_MANAGE_STATE(AfxGetStaticModuleState());

#ifdef _DEBUG
  HINSTANCE hInstance = ::GetModuleHandle("ui_legacy_d.dll");
#else
  HINSTANCE hInstance = ::GetModuleHandle("ui_legacy.dll");
#endif

  m_imgList.Create(16, 16, ILC_COLOR16 | ILC_MASK, 1, 0);

  m_imgList.SetBkColor(RGB(255, 255, 255));

  m_imgList.Add(::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_AMBOX)));
  m_imgList.Add(::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_AMBOX_ITEM)));

  SetImageList(&m_imgList, TVSIL_NORMAL);

  if (!m_pAModule) {
    return false;
  }
  m_vFuncItems = m_pAModule->get_func_items(FIM_AUXMODULEBOX);

  return (CreateContexMenu() && UpdateAMBoxTree());
}

bool SmtXAMBox::EndDestory(void) {
  if (m_hContexMenu) {
    ::DestroyMenu(m_hContexMenu);
    m_hContexMenu = NULL;
  }
  return true;
}

bool SmtXAMBox::CreateContexMenu(void) {
  CMenu menuDSMgr;
  menuDSMgr.LoadMenu(IDR_MENU_XMBOXMGR);
  m_hContexMenu = menuDSMgr.GetSafeHmenu();

  return true;
}

void SmtXAMBox::OnRButtonDown(UINT nFlags, CPoint point) {
  // TODO: �ڴ�������Ϣ������������/�����Ĭ��ֵ

  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  HTREEITEM hItem = HitTest(point, &nFlags);
  if ((hItem != NULL) && (TVHT_ONITEM & nFlags)) {
    SelectItem(hItem);
  } else
    return;

  CString strFuncItem;
  strFuncItem = GetItemText(hItem);

  CMenu menuMapMgr;
  menuMapMgr.LoadMenu(IDR_MENU_XMBOXMGR);

  CMenu* pMenu = NULL;
  pMenu = menuMapMgr.GetSubMenu(0);

  if (pMenu) {
    CPoint menuPos;
    GetCursorPos(&menuPos);
    pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
                          menuPos.x, menuPos.y, this);
    menuMapMgr.Detach();
  }
}

void SmtXAMBox::OnLButtonDown(UINT nFlags, CPoint point) {
  // TODO: �ڴ�������Ϣ������������/�����Ĭ��ֵ
}

void SmtXAMBox::OnLButtonUp(UINT nFlags, CPoint point) {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());

  if (!m_pAModule) {
    return;
  }

  static SmtListenerMsg param;
  param.hSrcWnd = m_hWnd;

  HTREEITEM hItem = HitTest(point, &nFlags);
  if ((hItem != NULL) && (TVHT_ONITEM & nFlags)) {
    SelectItem(hItem);
  } else
    return;

  CString strFuncItem;
  strFuncItem = GetItemText(hItem);

  vSmtFuncItems::iterator iter = m_vFuncItems.begin();
  while (iter != m_vFuncItems.end()) {
    // Tree labels are decoded for display; match against the same form.
    if (strFuncItem == ambox_title_for_display((*iter).szName)) {
      m_pAModule->notify((*iter).lMsg, param);
      return;
    }

    ++iter;
  }
}

bool SmtXAMBox::UpdateAMBoxTree(void) {
  if (NULL == m_pAModule) return false;

  SetRedraw(FALSE);
  DeleteAllItems();
  SetTextColor(RGB(0, 0, 255));

  m_hRoot = InsertItem(ambox_outlook_caption(m_pAModule->get_name()), 0, 0,
                       TVI_ROOT);

  vSmtFuncItems::iterator iter = m_vFuncItems.begin();
  while (iter != m_vFuncItems.end()) {
    HTREEITEM hItem =
        InsertItem(ambox_title_for_display((*iter).szName), 1, 1, m_hRoot);
    ++iter;
  }

  Expand(m_hRoot, TVE_EXPAND);

  SetRedraw(TRUE);

  RedrawWindow();

  return true;
}
}  // namespace ui
