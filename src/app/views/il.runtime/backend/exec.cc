// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/exec.h"

#include <cstdio>
#include <vector>

#include "app/views/il.runtime/ir/api.h"

namespace app {
namespace detail {

bool eval_code(content::CapabilityHost& host, const Code& code) {
  if (code.kind == Code::Kind::kNop) {
    return true;
  }
  if (code.kind == Code::Kind::kRun) {
    return code.run ? code.run(host) : false;
  }
  if (code.kind == Code::Kind::kSeq) {
    for (const Code& s : code.body) {
      if (!eval_code(host, s)) {
        std::fprintf(stderr, "interact-dsl: seq step failed call=%s\n",
                     s.name.c_str());
        std::fflush(stderr);
        return false;
      }
    }
    return true;
  }
  if (code.kind == Code::Kind::kRepeat) {
    for (int i = 0; i < code.repeat_count; ++i) {
      for (const Code& s : code.body) {
        if (!eval_code(host, s)) {
          return false;
        }
      }
    }
    return true;
  }
  if (code.kind == Code::Kind::kChord) {
    std::vector<unsigned> mods;
    for (const std::string& m : code.chord_mods) {
      const unsigned vk = ir::vk_from_name(host, m);
      if (vk) {
        mods.push_back(vk);
      }
    }
    for (unsigned vk : mods) {
      ir::post_key(host, vk, true);
    }
    bool ok = true;
    for (const Code& s : code.body) {
      if (!eval_code(host, s)) {
        ok = false;
        break;
      }
    }
    for (auto it = mods.rbegin(); it != mods.rend(); ++it) {
      ir::post_key(host, *it, false);
    }
    return ok;
  }
  return true;
}

}  // namespace detail
}  // namespace app
