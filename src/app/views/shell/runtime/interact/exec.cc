// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/interact/exec.h"

#include <windows.h>

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "app/views/shell/runtime/interact/exec_input.h"
#include "app/views/shell/runtime/interact/exec_shell.h"
#include "app/views/shell/runtime/interact/host_util.h"
#include "app/views/shell/runtime/interact/os_inject.h"

namespace app {
namespace detail {
namespace {

bool exec_call(content::CapabilityHost& host, const CallStmt& c, VarMap* vars) {
  if (!driver_ok(c.drivers)) {
    return true;
  }
  if (const std::optional<bool> shell = try_exec_shell_call(host, c, vars)) {
    return *shell;
  }
  if (const std::optional<bool> input = try_exec_input_call(host, c, vars)) {
    return *input;
  }
  std::fprintf(stderr, "interact-dsl: skip unknown call '%s'\n", c.name.c_str());
  return true;
}

}  // namespace

bool exec_stmt(content::CapabilityHost& host, const Stmt& stmt, VarMap* vars) {
  if (stmt.kind == Stmt::Kind::kCall) {
    return exec_call(host, stmt.call, vars);
  }
  if (!stmt.block) {
    return false;
  }
  const BlockStmt& b = *stmt.block;
  if (!driver_ok(b.drivers)) {
    return true;
  }
  if (b.kind == BlockStmt::Kind::kSeq) {
    for (const Stmt& s : b.body) {
      if (!exec_stmt(host, s, vars)) {
        if (s.kind == Stmt::Kind::kCall) {
          std::fprintf(stderr, "interact-dsl: seq step failed call=%s\n",
                       s.call.name.c_str());
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
