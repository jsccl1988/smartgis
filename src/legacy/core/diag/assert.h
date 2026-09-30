// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef _SMT_ASSERT_H
#define _SMT_ASSERT_H

#include <cstdio>
#include <cstring>

#include "legacy/core/macros/macros.h"

#ifdef _DEBUG

inline bool smt_assert(bool /*bContent*/, char* szDesc, int /*nLine*/,
                       char* /*szFile*/, bool* pBIgnoreAlways) {
  if (OpenClipboard(nullptr)) {
    char szAssert[256] = {};
    std::snprintf(szAssert, sizeof(szAssert), "Input assert info here");
    HGLOBAL hMem =
        GlobalAlloc(GHND | GMEM_DDESHARE, std::strlen(szAssert) + 1);
    if (hMem) {
      char* pMem = static_cast<char*>(GlobalLock(hMem));
      if (pMem) {
        std::memcpy(pMem, szAssert, std::strlen(szAssert) + 1);
        GlobalUnlock(hMem);
        EmptyClipboard();
        SetClipboardData(CF_TEXT, hMem);
      }
    }
    CloseClipboard();
  }

  const int nMsg = MessageBoxA(nullptr, szDesc, "SmtGis-assert",
                               MB_OKCANCEL | MB_ICONERROR);
  switch (nMsg) {
    case IDIGNORE:
      if (pBIgnoreAlways) {
        *pBIgnoreAlways = true;
      }
      return false;
    case IDOK:
      if (pBIgnoreAlways) {
        *pBIgnoreAlways = false;
      }
      return true;
    default:
      return false;
  }
}

#define SMT_ASSERT(exp, desc)                                                 \
  {                                                                           \
    static bool bIgnoreAlways = false;                                        \
    if (!bIgnoreAlways) {                                                     \
      if (smt_assert((int)(exp), desc, __LINE__, __FILE__, &bIgnoreAlways)) { \
        __debugbreak();                                                       \
      }                                                                       \
    }                                                                         \
  }
#else
#define SMT_ASSERT(exp, desc)
#endif  // _DEBUG

#endif  //_SMT_ASSERT_H
