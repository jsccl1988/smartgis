// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "StdAfx.h"
#include "legacy/ui/shell/ambox/outlook_bar.h"

#include "legacy/ui/shell/ambox/tree.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace ui {
namespace {

void apply_cjk_ui_font(CWnd* wnd) {
  if (!wnd || !::IsWindow(wnd->GetSafeHwnd())) {
    return;
  }
  LOGFONTW lf = {};
  if (!::SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0)) {
    return;
  }
  lf.lfCharSet = GB2312_CHARSET;
  wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
  HFONT hf = ::CreateFontIndirectW(&lf);
  if (!hf) {
    wcscpy_s(lf.lfFaceName, L"SimSun");
    hf = ::CreateFontIndirectW(&lf);
  }
  if (hf) {
    ::SendMessageW(wnd->GetSafeHwnd(), WM_SETFONT,
                   reinterpret_cast<WPARAM>(hf), TRUE);
  }
}

std::vector<std::pair<std::string, SmtAuxModule*>> collect_sorted_modules() {
  std::vector<std::pair<std::string, SmtAuxModule*>> modules;
  SmtAModuleManager* mgr = SmtAModuleManager::get_singleton_ptr();
  if (!mgr) {
    return modules;
  }
  const int count = mgr->get_a_module_count();
  modules.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; i++) {
    SmtAuxModule* mod = mgr->get_a_module(i);
    if (!mod) {
      continue;
    }
    const char* name = mod->get_name();
    modules.emplace_back(name ? name : "", mod);
  }
  std::sort(modules.begin(), modules.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  return modules;
}

}  // namespace

BEGIN_MESSAGE_MAP(SmtAMBoxMgrDocBar, CBCGPOutlookBar)
END_MESSAGE_MAP()

SmtAMBoxMgrDocBar::SmtAMBoxMgrDocBar() = default;

SmtAMBoxMgrDocBar::~SmtAMBoxMgrDocBar() {
  // Outlook may already tear down child HWNDs; only DestroyWindow live ones.
  for (CWnd* wnd : m_vWndPtrs) {
    if (!wnd) {
      continue;
    }
    if (::IsWindow(wnd->GetSafeHwnd())) {
      wnd->DestroyWindow();
    }
    SMT_SAFE_DELETE(wnd);
  }
  m_vWndPtrs.clear();
}

bool SmtAMBoxMgrDocBar::UpdateAMBoxs(void) {
  int id = 1;
  for (const auto& entry : collect_sorted_modules()) {
    CreateAMBox(entry.second, id++);
  }
  return true;
}

bool SmtAMBoxMgrDocBar::refresh_outlook_captions(void) {
  CBCGPOutlookWnd* pContainer =
      DYNAMIC_DOWNCAST(CBCGPOutlookWnd, GetUnderlyingWindow());
  if (!pContainer) {
    return false;
  }

  const int n = pContainer->GetTabsNum();
  if (n <= 0) {
    return true;
  }

  // Tabs 0/1 are Edit / System (InitAMBoxMgrDockBar); rest are AuxModules in
  // the same sorted order as UpdateAMBoxs.
  if (n >= 1) {
    pContainer->SetTabLabel(0, _T("Edit"));
    if (CWnd* w = pContainer->GetTabWnd(0)) {
      w->SetWindowText(_T("Edit"));
    }
  }
  if (n >= 2) {
    pContainer->SetTabLabel(1, _T("System"));
    if (CWnd* w = pContainer->GetTabWnd(1)) {
      w->SetWindowText(_T("System"));
    }
  }

  const auto modules = collect_sorted_modules();
  int idx = 2;
  for (const auto& entry : modules) {
    if (idx >= n) {
      break;
    }
    CString tab = ambox_outlook_caption(entry.first.c_str());
    pContainer->SetTabLabel(idx, tab);
    if (CWnd* w = pContainer->GetTabWnd(idx)) {
      w->SetWindowText(tab);
    }
    ++idx;
  }
  pContainer->RecalcLayout();
  pContainer->RedrawWindow();
  return true;
}

bool SmtAMBoxMgrDocBar::CreateAMBox(SmtAuxModule* pAModule, int nID) {
  if (NULL == pAModule) {
    return false;
  }

  SmtXAMBox* pXAMBox = new SmtXAMBox(pAModule);
  if (!pXAMBox->Create(WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS |
                           TVS_LINESATROOT | TVS_SHOWSELALWAYS,
                       CRect(0, 0, 0, 0), this, nID)) {
    TRACE0("Failed to create amtree");
    return false;
  }

  pXAMBox->ModifyStyleEx(0, WS_EX_CLIENTEDGE);
  apply_cjk_ui_font(pXAMBox);
  pXAMBox->UpdateAMBoxTree();

  // Outlook faces (Feature Pack) still paint raw AuxModule MBCS as '?'.
  // Plain ASCII only (no "[D] " prefix — BCGP face paint truncated it to
  // look like DEM???? when CJK leaked). Tree children stay CJK.
  CString tab = ambox_outlook_caption(pAModule->get_name());
  pXAMBox->SetWindowText(tab);
  add_wnd(pXAMBox, tab);
  return true;
}

bool SmtAMBoxMgrDocBar::add_wnd(CWnd* pWnd, CString strTitle) {
  if (pWnd == NULL) {
    return false;
  }

  // Outlook faces: ASCII only (raw AuxModule CJK paints as '?').
  CString ascii;
  for (int i = 0; i < strTitle.GetLength(); ++i) {
#ifdef _UNICODE
    const wchar_t wc = strTitle[i];
    if ((wc >= L'A' && wc <= L'Z') || (wc >= L'a' && wc <= L'z') ||
        (wc >= L'0' && wc <= L'9') || wc == L' ' || wc == L'-' || wc == L'_' ||
        wc == L'[' || wc == L']') {
      ascii += wc;
    }
#else
    const unsigned char ch = static_cast<unsigned char>(strTitle[i]);
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9') || ch == ' ' || ch == '-' || ch == '_' ||
        ch == '[' || ch == ']') {
      ascii += static_cast<TCHAR>(ch);
    }
#endif
  }
  ascii.Trim();
  if (ascii.IsEmpty()) {
    ascii = _T("AM");
  }
  strTitle = ascii;

  m_vWndPtrs.push_back(pWnd);

  CBCGPOutlookWnd* pContainer =
      DYNAMIC_DOWNCAST(CBCGPOutlookWnd, GetUnderlyingWindow());
  if (pContainer == NULL) {
    TRACE0("Cannot get outlook bar container\n");
    return false;
  }

  pContainer->AddControl(
      pWnd, strTitle, 0, TRUE,
      CBRS_BCGP_FLOAT | CBRS_BCGP_AUTOHIDE | CBRS_BCGP_RESIZE);
  pWnd->SetWindowText(strTitle);
  {
    const int idx = pContainer->GetTabsNum() - 1;
    if (idx >= 0) {
      pContainer->SetTabLabel(idx, strTitle);
      // Re-assert after Feature Pack may copy child GetWindowText.
      pContainer->SetTabLabel(idx, strTitle);
    }
  }
  apply_cjk_ui_font(pContainer);
  apply_cjk_ui_font(pWnd);
  // Outlook faces: re-apply ASCII after font change (SetFont can refresh
  // labels from the tree root / window text on some Feature Pack builds).
  {
    const int idx = pContainer->GetTabsNum() - 1;
    if (idx >= 0) {
      pContainer->SetTabLabel(idx, strTitle);
    }
  }
  pWnd->ShowWindow(SW_SHOW);
  return true;
}

}  // namespace ui
