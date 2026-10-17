// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "third_party/gn/dll_search.h"

#include <windows.h>
#include <wchar.h>

// delayimp.h uses IMAGE_THUNK_DATA / DWORD and does not include windows.h.
#include <delayimp.h>

namespace {

// SDK drops keep third-party runtime DLLs in a sibling of bin/. SetDllDirectory
// replaces only the current-directory slot; the exe directory stays first.
void set_sdk_dll_directory() {
  wchar_t exe[MAX_PATH] = {};
  const DWORD n = ::GetModuleFileNameW(nullptr, exe, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return;
  }
  wchar_t* slash = wcsrchr(exe, L'\\');
  if (slash == nullptr) {
    return;
  }
  *slash = L'\0';
  wchar_t relative[MAX_PATH] = {};
  if (wcscpy_s(relative, exe) != 0 || wcscat_s(relative, L"\\..\\third_party\\bin") != 0) {
    return;
  }
  wchar_t full[MAX_PATH] = {};
  if (::GetFullPathNameW(relative, MAX_PATH, full, nullptr) == 0) {
    return;
  }
  const DWORD attr = ::GetFileAttributesW(full);
  if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    return;
  }
  ::SetDllDirectoryW(full);
}

// Delay-load runs before WinMain when a product DLL's static init touches
// GDAL/GEOS/PROJ. Set the search directory in the preload hook so that load
// and the dependency walk both see third_party/bin.
extern "C" FARPROC WINAPI dll_search_notify(unsigned notify, PDelayLoadInfo) {
  if (notify == dliNotePreLoadLibrary) {
    set_sdk_dll_directory();
  }
  return nullptr;
}

}  // namespace

extern "C" const PfnDliHook __pfnDliNotifyHook2 = dll_search_notify;

void prepare_dll_search_path() {
  set_sdk_dll_directory();
}
