// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "StdAfx.h"
#include "legacy/ui/shell/ambox/ambox_dock_bar.h"

#include "legacy/ui/shell/ambox/ambox.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

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

BEGIN_MESSAGE_MAP(SmtAMBoxMgrDocBar, CBCGPOutlookBar)
END_MESSAGE_MAP()

SmtAMBoxMgrDocBar::SmtAMBoxMgrDocBar() = default;

SmtAMBoxMgrDocBar::~SmtAMBoxMgrDocBar() {
  for (CWnd* wnd : m_vWndPtrs) {
    if (wnd) {
      wnd->DestroyWindow();
      SMT_SAFE_DELETE(wnd);
    }
  }
  m_vWndPtrs.clear();
}

bool SmtAMBoxMgrDocBar::UpdateAMBoxs(void) {
  SmtAModuleManager* pAModuleMgr = SmtAModuleManager::get_singleton_ptr();
  if (!pAModuleMgr) {
    return true;
  }

  // Sort by name so Outlook pages group alphabetically (IA wave 3).
  std::vector<std::pair<std::string, SmtAuxModule*>> modules;
  const int count = pAModuleMgr->get_a_module_count();
  modules.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; i++) {
    SmtAuxModule* mod = pAModuleMgr->get_a_module(i);
    if (!mod) {
      continue;
    }
    const char* name = mod->get_name();
    modules.emplace_back(name ? name : "", mod);
  }
  std::sort(modules.begin(), modules.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  int id = 1;
  for (const auto& entry : modules) {
    CreateAMBox(entry.second, id++);
  }
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
  pXAMBox->UpdateAMBoxTree();

  // Prefix first letter so Outlook pages read as letter groups.
  CString title = pAModule->get_name();
  if (!title.IsEmpty()) {
    const TCHAR letter = static_cast<TCHAR>(::toupper(title[0]));
    CString grouped;
    grouped.Format(_T("[%c] %s"), letter, static_cast<LPCTSTR>(title));
    title = grouped;
  }
  add_wnd(pXAMBox, title);
  return true;
}

bool SmtAMBoxMgrDocBar::add_wnd(CWnd* pWnd, CString strTitle) {
  if (pWnd == NULL) {
    return false;
  }

  const CString title = ambox_title_for_display(strTitle.GetString());
  m_vWndPtrs.push_back(pWnd);

  CBCGPOutlookWnd* pContainer =
      DYNAMIC_DOWNCAST(CBCGPOutlookWnd, GetUnderlyingWindow());
  if (pContainer == NULL) {
    TRACE0("Cannot get outlook bar container\n");
    return false;
  }

  pContainer->AddControl(
      pWnd, title, 0, TRUE,
      CBRS_BCGP_FLOAT | CBRS_BCGP_AUTOHIDE | CBRS_BCGP_RESIZE);
  pWnd->ShowWindow(SW_SHOW);
  return true;
}

}  // namespace ui
