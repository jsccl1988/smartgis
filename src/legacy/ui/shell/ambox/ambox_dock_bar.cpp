// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "StdAfx.h"
#include "legacy/ui/shell/ambox/ambox_dock_bar.h"

#include "legacy/ui/shell/ambox/ambox.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace ui {

// AM / dock titles may be leftover CP936 or modern UTF-8. Build a CString that
// matches the process string mode (MBCS ACP or UNICODE) so Outlook tabs do not
// paint as '?'.
CString ambox_title_for_display(const char* name) {
  if (!name || !name[0]) {
    return CString();
  }

  auto try_decode = [](UINT code_page, const char* s,
                       std::wstring* out) -> bool {
    const DWORD flags =
        (code_page == CP_UTF8) ? MB_ERR_INVALID_CHARS : 0u;
    const int wlen =
        ::MultiByteToWideChar(code_page, flags, s, -1, nullptr, 0);
    if (wlen <= 0) {
      return false;
    }
    out->assign(static_cast<size_t>(wlen), L'\0');
    return ::MultiByteToWideChar(code_page, flags, s, -1, &(*out)[0], wlen) >
           0;
  };

  std::wstring wide;
  // Plugin TUs often keep /execution-charset:utf-8; leftover exe uses .936.
  // Strict UTF-8 first (MB_ERR_INVALID_CHARS), then CP936.
  if (!try_decode(CP_UTF8, name, &wide) && !try_decode(936, name, &wide)) {
    return CString(name);
  }
  // Drop the trailing L'\0' counted by MultiByteToWideChar(-1).
  if (!wide.empty() && wide.back() == L'\0') {
    wide.pop_back();
  }

#ifdef _UNICODE
  return CString(wide.c_str());
#else
  // MBCS SmartGis: keep CJK when process ACP is not 936 (en-US hosts).
  const UINT acp = ::GetACP();
  const UINT out_cp = (acp == 936) ? acp : 936u;
  const int n = ::WideCharToMultiByte(out_cp, 0, wide.c_str(), -1, nullptr, 0,
                                      nullptr, nullptr);
  if (n <= 0) {
    return CString(name);
  }
  std::string narrow(static_cast<size_t>(n), '\0');
  ::WideCharToMultiByte(out_cp, 0, wide.c_str(), -1, &narrow[0], n, nullptr,
                        nullptr);
  if (!narrow.empty() && narrow.back() == '\0') {
    narrow.pop_back();
  }
  return CString(narrow.c_str());
#endif
}

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

  // Tree keeps full CJK via ambox_title_for_display. Outlook tab faces in
  // MBCS BCGP often paint CJK as '?' even with a YaHei font — use an ASCII
  // stem for the page caption only.
  CString full = ambox_title_for_display(pAModule->get_name());
  CString ascii;
  for (int i = 0; i < full.GetLength(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(full[i]);
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9') || ch == ' ' || ch == '-' || ch == '_') {
      ascii += static_cast<TCHAR>(ch);
    }
  }
  ascii.Trim();
  if (ascii.IsEmpty()) {
    // Pure-CJK module names (地图打印 / 地图投影 / …).
    const char* raw = pAModule->get_name();
    if (raw && std::strstr(raw, "打印")) {
      ascii = _T("Print");
    } else if (raw && std::strstr(raw, "投影")) {
      ascii = _T("Projection");
    } else if (raw && (std::strstr(raw, "模型") || std::strstr(raw, "Model"))) {
      ascii = _T("Model3D");
    } else if (raw && (std::strstr(raw, "格网") || std::strstr(raw, "Grid"))) {
      ascii = _T("OrthoGrid");
    } else {
      ascii = _T("AM");
    }
  }
  CString tab = ascii;
  if (!tab.IsEmpty()) {
    const TCHAR first = tab[0];
    if ((first >= _T('A') && first <= _T('Z')) ||
        (first >= _T('a') && first <= _T('z'))) {
      CString grouped;
      grouped.Format(_T("[%c] %s"), static_cast<TCHAR>(::toupper(first)),
                     static_cast<LPCTSTR>(tab));
      tab = grouped;
    }
  }
  add_wnd(pXAMBox, tab);
  return true;
}

bool SmtAMBoxMgrDocBar::add_wnd(CWnd* pWnd, CString strTitle) {
  if (pWnd == NULL) {
    return false;
  }

  // |strTitle| is already display-encoded by CreateAMBox.
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
  // Prefer a CJK-capable UI font for Outlook page tabs (DEFAULT_GUI_FONT can
  // be a Western face under some ACP setups → CJK captions paint as '?').
  {
    LOGFONTW lf = {};
    if (::SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0)) {
      lf.lfCharSet = GB2312_CHARSET;
      wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
      HFONT hf = ::CreateFontIndirectW(&lf);
      if (!hf) {
        wcscpy_s(lf.lfFaceName, L"SimSun");
        hf = ::CreateFontIndirectW(&lf);
      }
      if (hf) {
        ::SendMessageW(pContainer->GetSafeHwnd(), WM_SETFONT,
                       reinterpret_cast<WPARAM>(hf), TRUE);
        // Outlook owns the HFONT for the bar lifetime (leak one face per page
        // is acceptable vs '?' captions); do not DeleteObject here.
      }
    }
  }
  pWnd->ShowWindow(SW_SHOW);
  return true;
}

}  // namespace ui
