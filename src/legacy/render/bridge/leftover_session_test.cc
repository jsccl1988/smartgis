// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/bridge/leftover_record.h"

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

#ifdef _WIN32
render::scene::LeftoverRecorder* smt_render_session() {
  HMODULE module = GetModuleHandleW(L"legacy_render_d.dll");
  if (!module) {
    module = GetModuleHandleW(L"legacy_render.dll");
  }
  if (!module) {
    return nullptr;
  }
  using SessionFn = render::scene::LeftoverRecorder* (*)();
  auto* fn = reinterpret_cast<SessionFn>(
      GetProcAddress(module, "smt_leftover_session"));
  if (!fn) {
    return nullptr;
  }
  return fn();
}
#endif

}  // namespace

int main() {
  using render::scene::leftover_session;
  using render::scene::leftover_session_is_process_wide;

#ifdef _WIN32
  expect(leftover_session_is_process_wide(),
         "render owns the process-wide leftover session");
  render::scene::LeftoverRecorder* exported = smt_render_session();
  expect(exported != nullptr, "smt_leftover_session export");
  expect(exported == &leftover_session(),
         "adapters and render share one LeftoverRecorder");
  expect(&leftover_session() == &leftover_session(),
         "leftover_session is stable");

  expect(leftover_session().begin(32, 32), "shared session begin");
  expect(leftover_session().is_open(), "session stays open across calls");
  expect(leftover_session().device() != nullptr, "shared Device");
  expect(leftover_session().list() != nullptr, "shared CommandList");
  expect(exported && exported->device() == leftover_session().device(),
         "same Device pointer from both modules");
  leftover_session().finish();
  leftover_session().release();
#else
  leftover_session();
  expect(true, "non-Windows leftover_session exists");
#endif

  if (g_fails) {
    std::fprintf(stderr, "leftover_session_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "leftover_session_test: ok\n");
  return 0;
}
