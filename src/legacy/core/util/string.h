// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef SMT_LEGACY_CORE_STRING_H
#define SMT_LEGACY_CORE_STRING_H

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "legacy/core/macros/macros.h"
#include "legacy/core/types/types.h"

namespace legacy_core_detail {

template <typename ItemFn>
inline void append_list_body(std::string* out, int n_count, int n_buf_length,
                             ItemFn append_item) {
  *out = "(";
  *out += std::to_string(n_count);
  *out += ":";
  int i = 0;
  for (; i < n_count; ++i) {
    const std::string item = append_item(i);
    if (out->size() + item.size() + 6 >=
        static_cast<size_t>(n_buf_length)) {
      break;
    }
    if (i > 0) {
      *out += ',';
    }
    *out += item;
  }
  if (i < n_count) {
    *out += ",...)";
  } else {
    *out += ')';
  }
}

inline void copy_to_buf(char* buf, int buf_length, const std::string& src) {
  if (buf_length <= 0) {
    return;
  }
  const size_t n =
      (std::min)(src.size(), static_cast<size_t>(buf_length - 1));
  std::memcpy(buf, src.data(), n);
  buf[n] = '\0';
}

}  // namespace legacy_core_detail

inline int integer_list_to_string(int nCount, int* pInteger,
                                  int nStingBufLength = TEMP_BUFFER_SIZE,
                                  char* szBuf = nullptr) {
  if (szBuf == nullptr || nStingBufLength <= 0 ||
      (nCount > 0 && pInteger == nullptr)) {
    return SMT_ERR_INVALID_PARAM;
  }
  std::string out;
  legacy_core_detail::append_list_body(
      &out, nCount, nStingBufLength,
      [&](int i) { return std::to_string(pInteger[i]); });
  legacy_core_detail::copy_to_buf(szBuf, nStingBufLength, out);
  return SMT_ERR_NONE;
}

inline int string_list_to_string(int nCount, char** pStrings,
                                 int nStingBufLength = TEMP_BUFFER_SIZE,
                                 char* szBuf = nullptr) {
  if (szBuf == nullptr || nStingBufLength <= 0 ||
      (nCount > 0 && pStrings == nullptr)) {
    return SMT_ERR_INVALID_PARAM;
  }
  std::string out;
  legacy_core_detail::append_list_body(&out, nCount, nStingBufLength,
                                       [&](int i) {
                                         return std::string(
                                             pStrings[i] ? pStrings[i] : "");
                                       });
  legacy_core_detail::copy_to_buf(szBuf, nStingBufLength, out);
  return SMT_ERR_NONE;
}

inline int real_list_to_string(int nCount, double* pReal,
                               int nStingBufLength = TEMP_BUFFER_SIZE,
                               char* szBuf = nullptr) {
  if (szBuf == nullptr || nStingBufLength <= 0 ||
      (nCount > 0 && pReal == nullptr)) {
    return SMT_ERR_INVALID_PARAM;
  }
  std::string out;
  legacy_core_detail::append_list_body(&out, nCount, nStingBufLength,
                                       [&](int i) {
                                         char item[40];
                                         std::snprintf(item, sizeof(item),
                                                       "%.8f", pReal[i]);
                                         return std::string(item);
                                       });
  legacy_core_detail::copy_to_buf(szBuf, nStingBufLength, out);
  return SMT_ERR_NONE;
}

inline int str_count(char** papszStrList) {
  if (papszStrList == nullptr) {
    return 0;
  }
  int n = 0;
  while (papszStrList[n] != nullptr) {
    ++n;
  }
  return n;
}

inline char** str_duplicate(char** papszStrList) {
  const int n_lines = str_count(papszStrList);
  if (n_lines == 0) {
    return nullptr;
  }
  char** out = new char*[static_cast<size_t>(n_lines) + 1];
  for (int i = 0; i < n_lines; ++i) {
    out[i] = _strdup(papszStrList[i]);
  }
  out[n_lines] = nullptr;
  return out;
}

inline uint str_tokenize(const string& str, vector<string>& tokens,
                        const string& delimiters) {
  string::size_type last_pos = str.find_first_not_of(delimiters, 0);
  string::size_type pos = str.find_first_of(delimiters, last_pos);
  while (pos != string::npos || last_pos != string::npos) {
    string tmp = str.substr(last_pos, pos - last_pos);
    if (!tmp.empty() && tmp[0] != ' ') {
      tokens.push_back(std::move(tmp));
    }
    last_pos = str.find_first_not_of(delimiters, pos);
    pos = str.find_first_of(delimiters, last_pos);
  }
  return static_cast<uint>(tokens.size());
}

#endif  // SMT_LEGACY_CORE_STRING_H
