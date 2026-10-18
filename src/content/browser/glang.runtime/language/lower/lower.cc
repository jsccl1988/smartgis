// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/language/lower/lower.h"

#include "base/core/log.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace content {
namespace code {
namespace {

bool is_ident_start(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_ident_cont(char c) {
  return is_ident_start(c) || (c >= '0' && c <= '9');
}

void add_diag(std::vector<content::py::Diagnostic>* diags,
              content::py::Loc loc,
              std::string message) {
  LOGGING(LOG_ERROR, "lower %u:%u: %s", loc.line, loc.column, message.c_str());
  if (!diags) {
    return;
  }
  content::py::Diagnostic diag;
  diag.loc = loc;
  diag.message = std::move(message);
  diags->push_back(std::move(diag));
}

std::uint32_t intern(Module* module, std::string text) {
  const auto index = static_cast<std::uint32_t>(module->strings.size());
  module->strings.push_back(std::move(text));
  return index;
}

int find_slot(const std::vector<std::string>& names, std::string_view name) {
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (names[i] == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace

bool lower_program(const content::py::BoundProgram& in,
                   Module* out,
                   std::vector<content::py::Diagnostic>* diags) {
  if (diags) {
    diags->clear();
  }
  if (!out) {
    if (diags) {
      add_diag(diags, {}, "missing module");
    }
    return false;
  }
  *out = Module{};
  out->slot_names = in.slot_names;

  for (const content::py::BoundCall& call : in.calls) {
    if (call.result_slot < -1 ||
        (call.result_slot >= 0 &&
         static_cast<std::size_t>(call.result_slot) >= out->slot_names.size())) {
      add_diag(diags, call.loc, "result slot is out of range");
      return false;
    }
    Inst inst;
    inst.op = call.op;
    inst.loc.line = call.loc.line == 0 ? 1 : call.loc.line;
    inst.loc.column = call.loc.column == 0 ? 1 : call.loc.column;
    inst.result_slot = static_cast<std::int16_t>(call.result_slot);
    inst.operand_begin = static_cast<std::uint32_t>(out->operands.size());
    if (call.operands.size() > 0xFFFF) {
      add_diag(diags, call.loc, "too many operands");
      return false;
    }
    for (const Operand& source : call.operands) {
      Operand packed = source;
      if (packed.aux == content::py::kRawText) {
        if (packed.index >= in.raw_strings.size()) {
          add_diag(diags, call.loc, "raw string index is out of range");
          return false;
        }
        const std::string& raw = in.raw_strings[packed.index];
        bool has_slot = false;
        for (std::size_t i = 0; i < raw.size(); ++i) {
          if (raw[i] == '$' && i + 1 < raw.size() && is_ident_start(raw[i + 1])) {
            has_slot = true;
            break;
          }
        }
        if (!has_slot) {
          packed.index = intern(out, raw);
          packed.aux = 0;
          if (packed.i32 < 0) {
            packed.i32 = 0;
          }
        } else {
          const auto begin = static_cast<std::uint32_t>(out->str_parts.size());
          std::string literal;
          auto flush = [&]() {
            if (literal.empty()) {
              return;
            }
            StrPart part;
            part.is_slot = false;
            part.index = intern(out, std::move(literal));
            literal.clear();
            out->str_parts.push_back(part);
          };
          for (std::size_t i = 0; i < raw.size();) {
            if (raw[i] != '$' || i + 1 >= raw.size() || !is_ident_start(raw[i + 1])) {
              literal.push_back(raw[i]);
              ++i;
              continue;
            }
            std::size_t j = i + 1;
            while (j < raw.size() && is_ident_cont(raw[j])) {
              ++j;
            }
            const int slot = find_slot(in.slot_names, raw.substr(i + 1, j - (i + 1)));
            if (slot < 0) {
              add_diag(diags, call.loc,
                       "slot $" + raw.substr(i + 1, j - (i + 1)) + " is not defined");
              return false;
            }
            flush();
            StrPart part;
            part.is_slot = true;
            part.index = static_cast<std::uint32_t>(slot);
            out->str_parts.push_back(part);
            i = j;
          }
          flush();
          const auto count =
              static_cast<std::uint32_t>(out->str_parts.size()) - begin;
          if (count == 0 || count > 0xFFFE) {
            add_diag(diags, call.loc, "string part count is out of range");
            return false;
          }
          packed.index = begin;
          packed.aux = static_cast<std::uint16_t>(count);
          packed.i32 = 0;
        }
      }
      out->operands.push_back(packed);
    }
    inst.operand_count = static_cast<std::uint16_t>(call.operands.size());
    out->insts.push_back(inst);
  }
  if (diags) {
    diags->clear();
  }
  return true;
}

}  // namespace code
}  // namespace content
