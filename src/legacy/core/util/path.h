// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef SMT_LEGACY_CORE_PATH_H
#define SMT_LEGACY_CORE_PATH_H

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "legacy/core/macros/macros.h"

inline void split_file_name(const char* fullname, char* path, char* fileName,
                            char* title, char* ext) {
  const std::filesystem::path p(fullname ? fullname : "");
  if (title != nullptr) {
    const std::string stem = p.stem().string();
    std::memcpy(title, stem.c_str(), stem.size() + 1);
  }
  if (ext != nullptr) {
    const std::string extension = p.extension().string();
    std::memcpy(ext, extension.c_str(), extension.size() + 1);
  }
  if (path != nullptr) {
    const std::filesystem::path parent = p.parent_path();
    std::string dir = parent.empty() ? std::string() : parent.string();
    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/') {
      dir.push_back('\\');
    }
    std::memcpy(path, dir.c_str(), dir.size() + 1);
  }
  if (fileName != nullptr) {
    const std::string name = p.filename().string();
    std::memcpy(fileName, name.c_str(), name.size() + 1);
  }
}

inline void get_parent_directory(const char* szCurDir, char* szParentDir,
                                 int iParent = 1) {
  if (szParentDir == nullptr) {
    return;
  }
  if (iParent < 1) {
    const char* src = szCurDir ? szCurDir : "";
    std::memcpy(szParentDir, src, std::strlen(src) + 1);
    return;
  }

  std::filesystem::path cur(szCurDir ? szCurDir : "");
  int hops = iParent + 1;
  while (hops > 0 && !cur.empty() && cur.has_parent_path() &&
         cur != cur.root_path()) {
    std::filesystem::path parent = cur.parent_path();
    if (parent == cur) {
      break;
    }
    cur = parent;
    --hops;
  }
  std::string out = cur.string();
  if (!out.empty() && out.back() != '\\' && out.back() != '/') {
    out.push_back('\\');
  } else if (out.empty()) {
    out = "\\";
  }
  std::memcpy(szParentDir, out.c_str(), out.size() + 1);
}

inline string get_app_path(void) {
  char module_path[MAX_PATH] = {};
  GetModuleFileNameA(nullptr, module_path, MAX_PATH);
  const std::filesystem::path dir =
      std::filesystem::path(module_path).parent_path();
  std::string out = dir.string();
  if (!out.empty() && out.back() != '\\' && out.back() != '/') {
    out.push_back('\\');
  }
  return out;
}

inline string get_app_temp_path(void) { return get_app_path() + "temp\\"; }

inline long create_all_path_directory(string strDirPath) {
  if (strDirPath.size() <= 3) {
    return SMT_ERR_FAILURE;
  }
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(strDirPath), ec);
  return ec ? SMT_ERR_FAILURE : SMT_ERR_NONE;
}

inline long delete_directory(string strDirName) {
  std::string raw = strDirName;
  while (!raw.empty() && (raw.back() == '\\' || raw.back() == '/')) {
    raw.pop_back();
  }
  if (raw.empty()) {
    return SMT_ERR_FAILURE;
  }
  std::error_code ec;
  const auto removed =
      std::filesystem::remove_all(std::filesystem::path(raw), ec);
  if (ec || removed == 0) {
    return SMT_ERR_FAILURE;
  }
  return SMT_ERR_NONE;
}

inline long get_temp_name(string& strName) {
  SYSTEMTIME time = {};
  ::GetSystemTime(&time);
  char buf[MAX_NAME_LENGTH] = {};
  std::snprintf(buf, MAX_NAME_LENGTH, "%s_%u%u%u%u", strName.c_str(),
                static_cast<unsigned>(time.wHour),
                static_cast<unsigned>(time.wMinute),
                static_cast<unsigned>(time.wSecond),
                static_cast<unsigned>(time.wMilliseconds));
  strName = buf;
  return SMT_ERR_NONE;
}

#endif  // SMT_LEGACY_CORE_PATH_H
