// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// base/core/log.h includes windows.h. Parentheses stop the min/max
// macros from expanding numeric_limits::min/max (C2589).
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "content/browser/glang.runtime/language/sema/check.h"

#include "content/browser/glang.runtime/exec/op/register.h"
#include "base/core/log.h"

#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace content {
namespace py {
namespace {

constexpr std::size_t kMaxDiagnostics = 20;

struct Diags {
  std::vector<Diagnostic>* out = nullptr;
  bool failed = false;

  bool note(Loc loc, std::string message) {
    failed = true;
    if (loc.line == 0) {
      loc.line = 1;
    }
    if (loc.column == 0) {
      loc.column = 1;
    }
    if (out && out->size() < kMaxDiagnostics) {
      LOGGING(LOG_ERROR, "sema %u:%u: %s", loc.line, loc.column, message.c_str());
      Diagnostic diag;
      diag.loc = loc;
      diag.message = std::move(message);
      out->push_back(std::move(diag));
    }
    return false;
  }
};

bool is_ident_start(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_ident_cont(char c) {
  return is_ident_start(c) || (c >= '0' && c <= '9');
}

bool is_slot_name(std::string_view text) {
  if (text.empty() || !is_ident_start(text.front())) {
    return false;
  }
  for (char c : text) {
    if (!is_ident_cont(c)) {
      return false;
    }
  }
  return true;
}

int find_slot(const std::vector<std::string>& names, std::string_view name) {
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (names[i] == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool name_is(const content::code::Param& param, std::string_view name) {
  return param.name != nullptr && name == param.name;
}

enum class Sugar { kNone, kRclick, kDblclick, kKeyDown, kKeyUp };

Sugar sugar_of(std::string_view method) {
  if (method == "rclick") {
    return Sugar::kRclick;
  }
  if (method == "dblclick") {
    return Sugar::kDblclick;
  }
  if (method == "key_down") {
    return Sugar::kKeyDown;
  }
  if (method == "key_up") {
    return Sugar::kKeyUp;
  }
  return Sugar::kNone;
}

bool is_sg_assign(const Stmt& stmt) {
  if (stmt.kind != StmtKind::kAssign || stmt.name != "sg" || !stmt.expr) {
    return false;
  }
  const Expr& expr = *stmt.expr;
  if (expr.kind != ExprKind::kAttribute || expr.attr != "script" || !expr.receiver) {
    return false;
  }
  return expr.receiver->kind == ExprKind::kName && expr.receiver->text == "smartgis";
}

bool is_sg_call(const Expr& expr, std::string* method) {
  if (expr.kind != ExprKind::kCall || !expr.receiver || !method) {
    return false;
  }
  if (!expr.attr.empty()) {
    if (expr.receiver->kind == ExprKind::kName && expr.receiver->text == "sg") {
      *method = expr.attr;
      return true;
    }
    return false;
  }
  if (expr.receiver->kind == ExprKind::kAttribute && !expr.receiver->attr.empty() &&
      expr.receiver->receiver &&
      expr.receiver->receiver->kind == ExprKind::kName &&
      expr.receiver->receiver->text == "sg") {
    *method = expr.receiver->attr;
    return true;
  }
  return false;
}

bool dollars_defined(std::string_view text,
                     const std::vector<std::string>& slots,
                     Diags* diags,
                     Loc loc) {
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '$' || i + 1 >= text.size() || !is_ident_start(text[i + 1])) {
      continue;
    }
    std::size_t j = i + 1;
    while (j < text.size() && is_ident_cont(text[j])) {
      ++j;
    }
    const std::string_view name = text.substr(i + 1, j - (i + 1));
    if (find_slot(slots, name) < 0) {
      return diags->note(loc, "slot $" + std::string(name) + " is not defined");
    }
    i = j - 1;
  }
  return true;
}

void set_raw(BoundProgram* program, content::code::Operand* operand, std::string text) {
  operand->index = static_cast<std::uint32_t>(program->raw_strings.size());
  operand->aux = kRawText;
  operand->i32 = 0;
  operand->crs_epsg = 0;
  program->raw_strings.push_back(std::move(text));
}

void set_i32(content::code::Operand* operand, content::code::ArgKind kind, std::int32_t value) {
  operand->kind = kind;
  operand->i32 = value;
  operand->index = 0;
  operand->aux = 0;
  operand->crs_epsg = 0;
}

bool set_slot_ref(content::code::Operand* operand,
                  content::code::ArgKind kind,
                  std::uint32_t slot_index) {
  operand->kind = kind;
  operand->i32 = -1;
  operand->aux = 0;
  operand->index = slot_index;
  operand->crs_epsg = 0;
  return true;
}

bool fill_literal(const content::code::Param& param,
                  const Expr& expr,
                  BoundProgram* program,
                  content::code::Operand* operand,
                  Diags* diags) {
  operand->kind = param.kind;
  operand->crs_epsg = 0;
  if (expr.kind != ExprKind::kConst) {
    return diags->note(expr.loc, "expected literal");
  }
  const bool is_int = expr.const_kind == ConstKind::kInt;
  const bool is_bool = expr.const_kind == ConstKind::kBool;
  const bool is_str = expr.const_kind == ConstKind::kStr;
  const char* mismatch = "type mismatch";
  switch (param.kind) {
    case content::code::ArgKind::kI32:
      if (is_str || (!is_int && !is_bool)) {
        return diags->note(expr.loc, mismatch);
      }
      if (is_bool) {
        set_i32(operand, param.kind, expr.bool_value ? 1 : 0);
        return true;
      }
      if (expr.int_value < (std::numeric_limits<std::int32_t>::min)() ||
          expr.int_value > (std::numeric_limits<std::int32_t>::max)()) {
        return diags->note(expr.loc, "integer out of range");
      }
      set_i32(operand, param.kind, static_cast<std::int32_t>(expr.int_value));
      return true;
    case content::code::ArgKind::kBool:
      if (is_str) {
        return diags->note(expr.loc, mismatch);
      }
      if (is_bool) {
        set_i32(operand, param.kind, expr.bool_value ? 1 : 0);
        return true;
      }
      if (is_int && (expr.int_value == 0 || expr.int_value == 1)) {
        set_i32(operand, param.kind, static_cast<std::int32_t>(expr.int_value));
        return true;
      }
      return diags->note(expr.loc, mismatch);
    case content::code::ArgKind::kVk:
      if (is_bool) {
        return diags->note(expr.loc, mismatch);
      }
      if (is_int) {
        if (expr.int_value < 0 ||
            expr.int_value > (std::numeric_limits<std::int32_t>::max)()) {
          return diags->note(expr.loc, "integer out of range");
        }
        // vk_of reads kI32 directly and expands kVk as text.
        set_i32(operand, content::code::ArgKind::kI32, static_cast<std::int32_t>(expr.int_value));
        return true;
      }
      if (!is_str) {
        return diags->note(expr.loc, mismatch);
      }
      if (!dollars_defined(expr.text, program->slot_names, diags, expr.loc)) {
        return false;
      }
      set_raw(program, operand, expr.text);
      return true;
    case content::code::ArgKind::kStr:
    case content::code::ArgKind::kPath:
      if (!is_str) {
        return diags->note(expr.loc, mismatch);
      }
      if (name_is(param, "var")) {
        if (expr.text.empty()) {
          set_raw(program, operand, "");
          return true;
        }
        if (!is_slot_name(expr.text)) {
          return diags->note(expr.loc, "var is not a slot name");
        }
        const int slot = find_slot(program->slot_names, expr.text);
        if (slot < 0) {
          return diags->note(expr.loc, "slot $" + expr.text + " is not defined");
        }
        return set_slot_ref(operand, param.kind, static_cast<std::uint32_t>(slot));
      }
      if (!dollars_defined(expr.text, program->slot_names, diags, expr.loc)) {
        return false;
      }
      set_raw(program, operand, expr.text);
      return true;
  }
  return diags->note(expr.loc, mismatch);
}

bool fill_default(const content::code::Param& param,
                  BoundProgram* program,
                  content::code::Operand* operand,
                  Diags* diags,
                  Loc loc) {
  operand->kind = param.kind;
  operand->crs_epsg = 0;
  switch (param.kind) {
    case content::code::ArgKind::kI32:
    case content::code::ArgKind::kBool:
      set_i32(operand, param.kind, param.default_i32);
      return true;
    case content::code::ArgKind::kStr:
    case content::code::ArgKind::kPath:
    case content::code::ArgKind::kVk:
      if (!param.default_str) {
        if (param.required) {
          return diags->note(loc, std::string("missing argument '") +
                                      (param.name ? param.name : "?") + "'");
        }
        set_raw(program, operand, "");
        return true;
      }
      if (!dollars_defined(param.default_str, program->slot_names, diags, loc)) {
        return false;
      }
      set_raw(program, operand, param.default_str);
      return true;
  }
  return diags->note(loc, "type mismatch");
}

bool apply_sugar(Sugar sugar,
                 const content::code::Param& param,
                 BoundProgram* program,
                 content::code::Operand* operand) {
  if (sugar == Sugar::kRclick && name_is(param, "button")) {
    set_raw(program, operand, "right");
    operand->kind = param.kind;
    return true;
  }
  if (sugar == Sugar::kDblclick && name_is(param, "button")) {
    set_raw(program, operand, "left");
    operand->kind = param.kind;
    return true;
  }
  if ((sugar == Sugar::kRclick || sugar == Sugar::kDblclick) &&
      name_is(param, "clicks")) {
    set_i32(operand, param.kind, sugar == Sugar::kDblclick ? 2 : 1);
    return true;
  }
  if ((sugar == Sugar::kKeyDown || sugar == Sugar::kKeyUp) && name_is(param, "down")) {
    set_i32(operand, param.kind, sugar == Sugar::kKeyDown ? 1 : 0);
    return true;
  }
  return false;
}

const Expr* named_expr(const Expr& call, std::string_view name, int* index) {
  for (std::size_t i = 0; i < call.named.size(); ++i) {
    if (call.named[i].first == name) {
      if (index) {
        *index = static_cast<int>(i);
      }
      return &call.named[i].second;
    }
  }
  return nullptr;
}

bool parse_point(std::string_view text, std::int32_t* x, std::int32_t* y) {
  const std::size_t comma = text.find(',');
  if (comma == std::string_view::npos || !x || !y) {
    return false;
  }
  auto trim = [](std::string_view part) {
    while (!part.empty() && (part.front() == ' ' || part.front() == '\t')) {
      part.remove_prefix(1);
    }
    while (!part.empty() && (part.back() == ' ' || part.back() == '\t')) {
      part.remove_suffix(1);
    }
    return part;
  };
  const std::string_view xs = trim(text.substr(0, comma));
  const std::string_view ys = trim(text.substr(comma + 1));
  auto parse_one = [](std::string_view part, std::int32_t* out) {
    if (part.empty() || !out) {
      return false;
    }
    if (part.front() == '+') {
      part.remove_prefix(1);
    }
    std::int32_t value = 0;
    const char* begin = part.data();
    const char* end = part.data() + part.size();
    const std::from_chars_result parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
      return false;
    }
    *out = value;
    return true;
  };
  return parse_one(xs, x) && parse_one(ys, y);
}

bool check_body(const std::vector<Stmt>& body,
                bool* sg_bound,
                BoundProgram* program,
                Diags* diags);

}  // namespace

const content::code::OpDesc* lookup_method(std::string_view method) {
  content::code::ensure_ops_registered();
  if (method == "rclick" || method == "dblclick") {
    return content::code::find_op(content::code::Op::kClick);
  }
  if (method == "key_down" || method == "key_up") {
    return content::code::find_op(content::code::Op::kKeyEdge);
  }
  if (method == "path") {
    return content::code::find_op(content::code::Op::kStroke);
  }
  if (const content::code::OpDesc* desc = content::code::find_op(method)) {
    return desc;
  }
  return nullptr;
}

bool bind_method(const Expr& call,
                 std::string_view method,
                 Loc loc,
                 BoundProgram* program,
                 std::vector<Diagnostic>* diags,
                 bool implicit_slot) {
  Diags bag;
  bag.out = diags;
  if (!program) {
    return bag.note(loc, "missing program");
  }
  content::code::ensure_ops_registered();
  const Sugar sugar = sugar_of(method);
  const content::code::OpDesc* desc = lookup_method(method);
  if (!desc) {
    return bag.note(loc, "unknown verb '" + std::string(method) + "'");
  }

  BoundCall bound;
  bound.op = desc->op;
  bound.loc = loc;
  bound.slot_names_snapshot = program->slot_names;

  std::string as_name;
  bool has_as = false;
  std::vector<bool> named_used(call.named.size(), false);
  for (std::size_t i = 0; i < call.named.size(); ++i) {
    const std::string& key = call.named[i].first;
    if (key != "as" && key != "as_") {
      continue;
    }
    named_used[i] = true;
    if (has_as) {
      bag.note(call.named[i].second.loc, "duplicate as_");
      continue;
    }
    has_as = true;
    const Expr& value = call.named[i].second;
    if (value.kind != ExprKind::kConst || value.const_kind != ConstKind::kStr ||
        !is_slot_name(value.text)) {
      bag.note(value.loc, "as_ must be a slot name");
      continue;
    }
    as_name = value.text;
  }

  std::vector<bool> pos_used(call.args.size(), false);
  const int param_count = desc->params ? desc->param_count : 0;
  bound.operands.reserve(static_cast<std::size_t>(param_count));
  for (int i = 0; i < param_count; ++i) {
    const content::code::Param& param = desc->params[i];
    content::code::Operand operand;
    if (apply_sugar(sugar, param, program, &operand)) {
      if (param.positional >= 0 &&
          param.positional < static_cast<int>(call.args.size())) {
        pos_used[static_cast<std::size_t>(param.positional)] = true;
      }
      int named_index = -1;
      if (param.name && named_expr(call, param.name, &named_index)) {
        named_used[static_cast<std::size_t>(named_index)] = true;
      }
      bound.operands.push_back(operand);
      continue;
    }

    const Expr* value = nullptr;
    if (param.name) {
      int named_index = -1;
      value = named_expr(call, param.name, &named_index);
      if (value) {
        named_used[static_cast<std::size_t>(named_index)] = true;
      }
    }
    if (!value && param.positional >= 0 &&
        param.positional < static_cast<int>(call.args.size())) {
      value = &call.args[static_cast<std::size_t>(param.positional)];
      pos_used[static_cast<std::size_t>(param.positional)] = true;
    }
    if (value && param.name && param.positional >= 0 &&
        param.positional < static_cast<int>(call.args.size())) {
      int named_index = -1;
      const Expr* named = named_expr(call, param.name, &named_index);
      if (named && named == value &&
          pos_used[static_cast<std::size_t>(param.positional)] &&
          &call.args[static_cast<std::size_t>(param.positional)] != named) {
        bag.note(named->loc, std::string("duplicate argument '") + param.name + "'");
      }
    }
    if (!value) {
      if (param.required) {
        bag.note(loc, std::string("missing argument '") +
                          (param.name ? param.name : "?") + "'");
        bound.operands.push_back(operand);
        continue;
      }
      if (!fill_default(param, program, &operand, &bag, loc)) {
        bound.operands.push_back(operand);
        continue;
      }
      bound.operands.push_back(operand);
      continue;
    }
    if (!fill_literal(param, *value, program, &operand, &bag)) {
      bound.operands.push_back(operand);
      continue;
    }
    if (desc->op == content::code::Op::kRequireVar && name_is(param, "name") &&
        value->const_kind == ConstKind::kStr && !value->text.empty() &&
        value->text.find('$') == std::string::npos &&
        find_slot(program->slot_names, value->text) < 0) {
      bag.note(value->loc, "slot $" + value->text + " is not defined");
    }
    bound.operands.push_back(operand);
  }

  if (desc->op == content::code::Op::kStroke) {
    for (std::size_t i = 0; i < call.args.size(); ++i) {
      if (pos_used[i]) {
        continue;
      }
      pos_used[i] = true;
      const Expr& point = call.args[i];
      std::int32_t x = 0;
      std::int32_t y = 0;
      if (point.kind != ExprKind::kConst || point.const_kind != ConstKind::kStr ||
          !parse_point(point.text, &x, &y)) {
        bag.note(point.loc, "path point must be 'x,y'");
        continue;
      }
      content::code::Operand px;
      content::code::Operand py;
      set_i32(&px, content::code::ArgKind::kI32, x);
      set_i32(&py, content::code::ArgKind::kI32, y);
      bound.operands.push_back(px);
      bound.operands.push_back(py);
    }
  }

  for (std::size_t i = 0; i < call.args.size(); ++i) {
    if (!pos_used[i]) {
      bag.note(call.args[i].loc, "unexpected argument");
    }
  }
  for (std::size_t i = 0; i < call.named.size(); ++i) {
    if (!named_used[i]) {
      bag.note(call.named[i].second.loc,
               "unknown argument '" + call.named[i].first + "'");
    }
  }

  if (desc->produces_slot) {
    if ((!has_as || as_name.empty()) && implicit_slot) {
      as_name = "_ret";
      has_as = true;
    }
    if (!has_as || as_name.empty()) {
      bag.note(loc, "missing as_");
    } else {
      const int existing = find_slot(program->slot_names, as_name);
      if (existing >= 0) {
        bound.result_slot = existing;
      } else {
        bound.result_slot = static_cast<int>(program->slot_names.size());
      }
    }
  } else if (has_as) {
    bag.note(loc, "verb does not produce a slot");
  }

  if (bag.failed) {
    return false;
  }
  if (desc->produces_slot && bound.result_slot ==
                                 static_cast<int>(program->slot_names.size())) {
    program->slot_names.push_back(as_name);
  }
  program->calls.push_back(std::move(bound));
  return true;
}

namespace {

bool check_body(const std::vector<Stmt>& body,
                bool* sg_bound,
                BoundProgram* program,
                Diags* diags) {
  for (const Stmt& stmt : body) {
    if (is_sg_assign(stmt)) {
      *sg_bound = true;
      continue;
    }
    if (stmt.kind == StmtKind::kExpr && stmt.expr && stmt.expr->kind == ExprKind::kCall) {
      std::string method;
      if (!is_sg_call(*stmt.expr, &method)) {
        diags->note(stmt.loc, "call target is not sg");
        continue;
      }
      if (!*sg_bound) {
        diags->note(stmt.loc, "sg is not bound");
      }
      if (!bind_method(*stmt.expr, method, stmt.loc, program, diags->out)) {
        diags->failed = true;
      }
      continue;
    }
    diags->note(stmt.loc, "statement is outside the suite subset");
  }
  return !diags->failed;
}

}  // namespace

bool check_suite(const File& file, BoundProgram* out, std::vector<Diagnostic>* diags) {
  if (diags) {
    diags->clear();
  }
  Diags bag;
  bag.out = diags;
  if (!out) {
    return bag.note(Loc{}, "missing program");
  }
  *out = BoundProgram{};
  content::code::ensure_ops_registered();

  bool saw_main = false;
  bool sg_bound = false;
  for (const Stmt& stmt : file.body) {
    if (stmt.kind == StmtKind::kImport) {
      if (stmt.name != "smartgis") {
        bag.note(stmt.loc, "import is outside the suite subset");
      }
      continue;
    }
    if (stmt.kind == StmtKind::kFunc) {
      if (stmt.name != "main") {
        bag.note(stmt.loc, "only main is allowed");
        continue;
      }
      if (saw_main) {
        bag.note(stmt.loc, "duplicate main");
        continue;
      }
      saw_main = true;
      check_body(stmt.body, &sg_bound, out, &bag);
      continue;
    }
    if (is_sg_assign(stmt)) {
      sg_bound = true;
      continue;
    }
    bag.note(stmt.loc, "statement is outside the suite subset");
  }
  if (!saw_main) {
    bag.note(Loc{}, "missing main");
  }
  // bind_method writes into `diags` but not into `bag.failed`.
  if (diags && !diags->empty()) {
    bag.failed = true;
  }
  return !bag.failed;
}

}  // namespace py
}  // namespace content
