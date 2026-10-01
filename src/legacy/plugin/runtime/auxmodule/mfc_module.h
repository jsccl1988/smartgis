// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_PLUGIN_MFC_MODULE_H_
#define LEGACY_PLUGIN_MFC_MODULE_H_

#include <afxwin.h>

// Call from CWinApp::InitInstance (after CWinApp::InitInstance / AfxWinInit),
// never from the CWinApp constructor. Setting m_hCurrentInstanceHandle before
// mfcs InternalDllMain finishes trips dllmodul.cpp VERIFY/ASSERT on attach or
// detach. Only fill a still-null resource handle (afxwin1.inl).
inline void bind_plugin_dll_resources(const void* address_in_dll) {
  if (!address_in_dll) {
    return;
  }
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCWSTR>(address_in_dll), &module) ||
      module == nullptr) {
    return;
  }
  AFX_MODULE_STATE* state = AfxGetModuleState();
  if (!state) {
    return;
  }
  if (state->m_hCurrentResourceHandle == nullptr) {
    state->m_hCurrentResourceHandle = module;
  }
}

// After the last plugin DLL loads, return this thread to the exe module state
// so BCG / self-test do not keep using the plugin's CWinApp.
inline void restore_exe_mfc_module_state() {
  AFX_MODULE_STATE* app = AfxGetAppModuleState();
  if (!app) {
    return;
  }
  if (app->m_hCurrentResourceHandle == nullptr) {
    app->m_hCurrentResourceHandle = app->m_hCurrentInstanceHandle;
  }
  AfxSetModuleState(app);
}

#endif  // LEGACY_PLUGIN_MFC_MODULE_H_
