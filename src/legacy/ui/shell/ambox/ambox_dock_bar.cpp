// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "StdAfx.h"
#include "legacy/ui/shell/ambox/ambox_dock_bar.h"

#include "legacy/ui/shell/ambox/ambox.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace ui {

namespace {

bool try_decode_wide(UINT code_page, DWORD flags, const char* s,
                     std::wstring* wide) {
  const int wlen = ::MultiByteToWideChar(code_page, flags, s, -1, nullptr, 0);
  if (wlen <= 0) {
    return false;
  }
  wide->assign(static_cast<size_t>(wlen), L'\0');
  if (::MultiByteToWideChar(code_page, flags, s, -1, &(*wide)[0], wlen) <= 0) {
    return false;
  }
  if (!wide->empty() && wide->back() == L'\0') {
    wide->pop_back();
  }
  return true;
}

// Product AM plugins compile with /execution-charset:utf-8. Prefer strict
// UTF-8; fall back to CP936 for leftover MBCS stems. Never leave callers with
// ACP reinterpretation of UTF-8 bytes (classic mojibake).
bool decode_name_wide(const char* name, std::wstring* out) {
  if (!name || !name[0] || !out) {
    return false;
  }
  if (try_decode_wide(CP_UTF8, MB_ERR_INVALID_CHARS, name, out)) {
    return true;
  }
  if (try_decode_wide(936, 0, name, out)) {
    return true;
  }
  // Last resort: lenient UTF-8 (truncated mid-sequence still shows a prefix).
  return try_decode_wide(CP_UTF8, 0, name, out);
}

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

bool wide_contains_ci(const std::wstring& hay, const wchar_t* needle) {
  if (!needle || !needle[0] || hay.empty()) {
    return false;
  }
  std::wstring h = hay;
  std::wstring n = needle;
  for (auto& c : h) {
    c = static_cast<wchar_t>(::towlower(c));
  }
  for (auto& c : n) {
    c = static_cast<wchar_t>(::towlower(c));
  }
  return h.find(n) != std::wstring::npos;
}

}  // namespace

// AM / dock titles may be leftover CP936 or modern UTF-8.
CString ambox_title_for_display(const char* name) {
  if (!name || !name[0]) {
    return CString();
  }
  std::wstring wide;
  if (!decode_name_wide(name, &wide)) {
#ifdef _UNICODE
    // Avoid CString(const char*) ACP reinterpret of UTF-8 bytes.
    return CString(L"?");
#else
    return CString(name);
#endif
  }
#ifdef _UNICODE
  return CString(wide.c_str());
#else
  const UINT acp = ::GetACP();
  const UINT out_cp = (acp == 936) ? acp : 936u;
  const int n = ::WideCharToMultiByte(out_cp, 0, wide.c_str(), -1, nullptr, 0,
                                      nullptr, nullptr);
  if (n <= 0) {
    return CString("?");
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

// Outlook tab faces paint CJK as '?' even under UNICODE + YaHei. Always ASCII.
CString ambox_outlook_caption(const char* name) {
  std::wstring wide;
  decode_name_wide(name, &wide);

  if (wide_contains_ci(wide, L"dem") ||
      (name && std::strstr(name, "DEM"))) {
    return _T("DEM");
  }
  if (wide_contains_ci(wide, L"\u6253\u5370") ||  // 打印
      wide_contains_ci(wide, L"print")) {
    return _T("Print");
  }
  if (wide_contains_ci(wide, L"\u6295\u5f71") ||  // 投影
      wide_contains_ci(wide, L"proj")) {
    return _T("Projection");
  }
  if (wide_contains_ci(wide, L"\u6a21\u578b") ||  // 模型
      wide_contains_ci(wide, L"\u4e09\u7ef4\u521b\u5efa") ||  // 三维创建
      wide_contains_ci(wide, L"model")) {
    return _T("Model3D");
  }
  if (wide_contains_ci(wide, L"\u683c\u7f51") ||  // 格网
      wide_contains_ci(wide, L"grid") || wide_contains_ci(wide, L"ortho")) {
    return _T("OrthoGrid");
  }

  // Keep ASCII letters/digits from the decoded name.
  CString ascii;
  for (wchar_t wc : wide) {
    if ((wc >= L'A' && wc <= L'Z') || (wc >= L'a' && wc <= L'z') ||
        (wc >= L'0' && wc <= L'9') || wc == L' ' || wc == L'-' || wc == L'_') {
      ascii += static_cast<TCHAR>(wc);
    }
  }
  ascii.Trim();
  if (ascii.IsEmpty()) {
    ascii = _T("AM");
  }
  return ascii;
}

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
  SmtAModuleManager* pAModuleMgr = SmtAModuleManager::get_singleton_ptr();
  if (!pAModuleMgr) {
    return true;
  }

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
  apply_cjk_ui_font(pXAMBox);
  pXAMBox->UpdateAMBoxTree();

  // Outlook faces (Feature Pack) still paint raw AuxModule MBCS as '?'.
  // Use ASCII face captions; keep tree body CJK via UpdateAMBoxTree.
  CString tab = ambox_outlook_caption(pAModule->get_name());
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
    }
  }
  apply_cjk_ui_font(pContainer);
  apply_cjk_ui_font(pWnd);
  pWnd->ShowWindow(SW_SHOW);
  return true;
}

}  // namespace ui
