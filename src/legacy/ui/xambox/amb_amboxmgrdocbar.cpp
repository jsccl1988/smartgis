// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "StdAfx.h"
#include "legacy/ui/xambox/amb_amboxmgrdocbar.h"

#include "legacy/ui/xambox/amb_xambox.h"

#include <string>

namespace ui {
namespace {

// Leftover AM / dock titles are CP936 narrow strings (/execution-charset:.936).
// When the process ACP is UTF-8 (Windows Beta), Outlook tabs must receive UTF-8
// or Chinese glyphs become '?'.
CString ambox_title_for_display(const char* name) {
  if (!name || !name[0]) {
    return CString();
  }
  if (::GetACP() != 65001) {
    return CString(name);
  }
  const int wlen = ::MultiByteToWideChar(936, 0, name, -1, nullptr, 0);
  if (wlen <= 0) {
    return CString(name);
  }
  std::wstring wide(static_cast<size_t>(wlen), L'\0');
  ::MultiByteToWideChar(936, 0, name, -1, &wide[0], wlen);
  const int u8len = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr,
                                          0, nullptr, nullptr);
  if (u8len <= 0) {
    return CString(name);
  }
  std::string utf8(static_cast<size_t>(u8len), '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], u8len, nullptr,
                        nullptr);
  return CString(utf8.c_str());
}

}  // namespace

/////////////////////////////////////////////////////////////////////////////
// SmtAMBoxMgrDocBar

BEGIN_MESSAGE_MAP(SmtAMBoxMgrDocBar, CBCGPOutlookBar)
//{{AFX_MSG_MAP(SmtAMBoxMgrDocBar)
//}}AFX_MSG_MAP
//	ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCatalogDockBar construction/destruction

SmtAMBoxMgrDocBar::SmtAMBoxMgrDocBar() {
  // TODO: add one-time construction code here
  m_nToolBoxPage = -1;
}

SmtAMBoxMgrDocBar::~SmtAMBoxMgrDocBar() {
  vector<CWnd*>::iterator iter = m_vWndPtrs.begin();
  while (iter != m_vWndPtrs.end()) {
    if (*iter) {
      (*iter)->DestroyWindow();
      SMT_SAFE_DELETE(*iter);
    }
    iter++;
  }

  m_vWndPtrs.clear();
}

void SmtAMBoxMgrDocBar::OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/) {
  // TODO: Add your message handler code here
}

/////////////////////////////////////////////////////////////////////////////
// SmtAMBoxMgrDocBar message handlers
bool SmtAMBoxMgrDocBar::UpdateAMBoxs(void) {
  SmtAModuleManager* pAModuleMgr = SmtAModuleManager::get_singleton_ptr();
  if (pAModuleMgr) {
    for (int i = 0; i < pAModuleMgr->get_a_module_count(); i++) {
      SmtAuxModule* pAModule = pAModuleMgr->get_a_module(i);
      CreateAMBox(pAModule, i + 1);
    }
  }

  return true;
}

bool SmtAMBoxMgrDocBar::CreateAMBox(SmtAuxModule* pAModule, int nID) {
  if (NULL == pAModule) return false;

  SmtXAMBox* pXAMBox = new SmtXAMBox(pAModule);
  if (!pXAMBox->Create(WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS |
                           TVS_LINESATROOT | TVS_SHOWSELALWAYS,
                       CRect(0, 0, 0, 0), this, nID)) {
    TRACE0("Failed to create amtree");
    return false;
  }

  // pXAMBox->InitCreate();

  pXAMBox->ModifyStyleEx(0, WS_EX_CLIENTEDGE);
  pXAMBox->UpdateAMBoxTree();

  AddWnd(pXAMBox, pAModule->get_name());

  return true;
}

//////////////////////////////////////////////////////////////////////////
bool SmtAMBoxMgrDocBar::AddWnd(CWnd* pWnd, CString strTitle) {
  if (pWnd == NULL) {
    return false;
  }

  m_vWndPtrs.push_back(pWnd);

  CBCGPOutlookWnd* pContainer =
      DYNAMIC_DOWNCAST(CBCGPOutlookWnd, GetUnderlyingWindow());

  if (pContainer == NULL) {
    TRACE0("Cannot get outlook bar container\n");
    return false;
  }

  // main_frame passes CP936 literals; normalize when ACP is UTF-8.
  const CString title = ambox_title_for_display(strTitle.GetString());

  pContainer->AddControl(
      pWnd, title, 0, TRUE,
      CBRS_BCGP_FLOAT | CBRS_BCGP_AUTOHIDE | CBRS_BCGP_RESIZE);
  pWnd->ShowWindow(SW_SHOW);

  return true;
}

}  // namespace ui
