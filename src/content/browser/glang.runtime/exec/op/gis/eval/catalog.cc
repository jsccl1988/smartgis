// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/gis/eval/eval.h"

#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/op/gis/pack/pack.h"
#include "gis/analysis/ops/runner/ops_runner.h"

#include <string>
#include <string_view>

namespace content {
namespace code {
namespace detail {
namespace {

std::string json_escape(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
      out.push_back(c);
    } else if (c == '\n') {
      out += "\\n";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

}  // namespace

bool eval_list_analysis(content::CapabilityHost&,
                        const Module&,
                        const Inst& inst,
                        SlotTable& slots,
                        std::string* error) {
  std::string out = "[";
  bool first = true;
  for (const gis::detail::BuiltinOpDesc& desc : gis::detail::builtin_op_catalog()) {
    if (!first) {
      out += ',';
    }
    first = false;
    out += "{\"id\":\"";
    out += json_escape(desc.id ? desc.id : "");
    out += "\",\"title\":\"";
    out += json_escape(desc.title ? desc.title : "");
    out += "\"}";
  }
  out += ']';
  return put_result(inst, slots, out, error);
}

}  // namespace detail
}  // namespace code
}  // namespace content
