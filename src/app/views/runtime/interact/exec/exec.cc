// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/interact/exec/exec.h"

#include <windows.h>

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "app/views/runtime/interact/exec/document.h"
#include "app/views/runtime/interact/exec/horizon.h"
#include "app/views/runtime/interact/exec/input.h"
#include "app/views/runtime/interact/exec/plugin.h"
#include "app/views/runtime/interact/exec/showcase.h"
#include "app/views/runtime/interact/host/host.h"
#include "app/views/runtime/interact/io/os_inject.h"

namespace app {
namespace detail {
namespace {

using LaneFn = std::optional<bool> (*)(content::CapabilityHost&,
                                      const CallStmt&,
                                      VarMap*);

// First lane that recognizes the verb wins. Unknown names are skipped so a
// newer script can run on an older shell without aborting the suite.
constexpr LaneFn kLanes[] = {
    try_exec_horizon_call,  try_exec_document_call, try_exec_plugin_call,
    try_exec_showcase_call, try_exec_input_call,
};

bool exec_call(content::CapabilityHost& host, const CallStmt& c, VarMap* vars) {
  if (!driver_ok(c.drivers)) {
    return true;
  }
  for (const LaneFn lane : kLanes) {
    if (const std::optional<bool> hit = lane(host, c, vars)) {
      return *hit;
    }
  }
  std::fprintf(stderr, "interact-dsl: skip unknown call '%s'\n", c.name.c_str());
  return true;
}

}  // namespace

bool exec_stmt(content::CapabilityHost& host, const Stmt& stmt, VarMap* vars) {
  if (const CallStmt* call = as_call(stmt)) {
    return exec_call(host, *call, vars);
  }
  const BlockStmt* block = as_block(stmt);
  if (!block) {
    return false;
  }
  const BlockStmt& b = *block;
  if (!driver_ok(b.drivers)) {
    return true;
  }
  if (b.kind == BlockStmt::Kind::kSeq) {
    for (const Stmt& s : b.body) {
      if (!exec_stmt(host, s, vars)) {
        if (const CallStmt* call = as_call(s)) {
          std::fprintf(stderr, "interact-dsl: seq step failed call=%s\n",
                       call->name.c_str());
          std::fflush(stderr);
        }
        return false;
      }
    }
    return true;
  }
  if (b.kind == BlockStmt::Kind::kRepeat) {
    for (int i = 0; i < b.repeat_count; ++i) {
      for (const Stmt& s : b.body) {
        if (!exec_stmt(host, s, vars)) {
          return false;
        }
      }
    }
    return true;
  }
  if (b.kind == BlockStmt::Kind::kChord) {
    HWND hwnd = host_hwnd(host);
    std::vector<WORD> mods;
    for (const std::string& m : b.chord_mods) {
      const WORD vk = vk_from_name(m);
      if (vk) {
        mods.push_back(vk);
      }
    }
    for (WORD vk : mods) {
      if (hwnd) {
        PostMessageW(hwnd, WM_KEYDOWN, vk, 0);
      }
    }
    bool ok = true;
    for (const Stmt& s : b.body) {
      if (!exec_stmt(host, s, vars)) {
        ok = false;
        break;
      }
    }
    for (auto it = mods.rbegin(); it != mods.rend(); ++it) {
      if (hwnd) {
        PostMessageW(hwnd, WM_KEYUP, *it, 0);
      }
    }
    return ok;
  }
  return true;
}

}  // namespace detail
}  // namespace app
