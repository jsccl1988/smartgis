// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "StdAfx.h"
#include "legacy/ui/shell/ambox/title.h"

#include <cstring>
#include <string>

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

CString ambox_outlook_caption(const char* name) {
  std::wstring wide;
  decode_name_wide(name, &wide);

  if (wide_contains_ci(wide, L"dem") || (name && std::strstr(name, "DEM"))) {
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
  if (wide_contains_ci(wide, L"\u7f16\u8f91") ||  // 编辑
      wide_contains_ci(wide, L"edit")) {
    return _T("Edit");
  }
  if (wide_contains_ci(wide, L"\u7cfb\u7edf") ||  // 系统
      wide_contains_ci(wide, L"system") || wide_contains_ci(wide, L"sys")) {
    return _T("System");
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

}  // namespace ui
