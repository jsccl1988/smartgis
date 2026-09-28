// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/flycube/pipeline/hlsl.h"

#ifdef SMT_HAS_FLYCUBE
#include <cstring>
#include <fstream>
#include <string>
#include <windows.h>
#endif

namespace render {
namespace rhi {
namespace detail {

#ifdef SMT_HAS_FLYCUBE

bool file_exists(const char* path) {
  return path && path[0] && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

bool copy_if_needed(const char* src, const char* dest) {
  if (file_exists(dest)) {
    return true;
  }
  return file_exists(src) && CopyFileA(src, dest, FALSE) != 0;
}

// FlyCube CompileShader aborts if dxcompiler.dll is not next to the exe.
bool ensure_dxc_beside_exe() {
  char exe[MAX_PATH];
  if (GetModuleFileNameA(nullptr, exe, MAX_PATH) == 0) {
    return false;
  }
  char* slash = strrchr(exe, '\\');
  if (!slash) {
    return false;
  }
  slash[1] = 0;
  const std::string dir(exe);
  const std::string dest_compiler = dir + "dxcompiler.dll";
  const std::string dest_dxil = dir + "dxil.dll";
  static const char* kCompiler[] = {
      "C:\\Program Files (x86)\\Windows Kits\\10\\Redist\\D3D\\x64\\"
      "dxcompiler.dll",
      "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.26100.0\\x64\\"
      "dxcompiler.dll",
      nullptr,
  };
  static const char* kDxil[] = {
      "C:\\Program Files (x86)\\Windows Kits\\10\\Redist\\D3D\\x64\\dxil.dll",
      "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.26100.0\\x64\\"
      "dxil.dll",
      nullptr,
  };
  bool compiler_ok = file_exists(dest_compiler.c_str());
  for (int i = 0; !compiler_ok && kCompiler[i]; ++i) {
    compiler_ok = copy_if_needed(kCompiler[i], dest_compiler.c_str());
  }
  bool dxil_ok = file_exists(dest_dxil.c_str());
  for (int i = 0; !dxil_ok && kDxil[i]; ++i) {
    dxil_ok = copy_if_needed(kDxil[i], dest_dxil.c_str());
  }
  return compiler_ok && dxil_ok;
}

bool write_temp_hlsl(const char* name, const char* source, std::string* path) {
  char dir[MAX_PATH];
  const DWORD n = GetTempPathA(MAX_PATH, dir);
  if (n == 0 || n >= MAX_PATH || !path) {
    return false;
  }
  *path = std::string(dir) + name;
  std::ofstream out(path->c_str(), std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out << source;
  return static_cast<bool>(out);
}

#endif  // SMT_HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render
