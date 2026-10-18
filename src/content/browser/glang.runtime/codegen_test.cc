// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/language/code/verify.h"
#include "content/browser/glang.runtime/exec/eval/eval.h"
#include "content/browser/glang.runtime/language/front/parse.h"
#include "content/browser/glang.runtime/language/lower/lower.h"
#include "content/browser/glang.runtime/language/sema/check.h"
#include "content/browser/capability/host.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

bool has_text(const std::vector<content::py::Diagnostic>& diags, const char* needle) {
  for (const content::py::Diagnostic& diag : diags) {
    if (diag.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

void dump_diags(const std::vector<content::py::Diagnostic>& diags) {
  for (const content::py::Diagnostic& diag : diags) {
    std::fprintf(stderr, "  diag %u:%u %s\n", diag.loc.line, diag.loc.column,
                 diag.message.c_str());
  }
}

content::py::Expr make_name(const char* text) {
  content::py::Expr expr;
  expr.kind = content::py::ExprKind::kName;
  expr.text = text;
  return expr;
}

content::py::Expr make_str(const char* text) {
  content::py::Expr expr;
  expr.kind = content::py::ExprKind::kConst;
  expr.const_kind = content::py::ConstKind::kStr;
  expr.text = text;
  return expr;
}

content::py::Expr make_int(std::int64_t value) {
  content::py::Expr expr;
  expr.kind = content::py::ExprKind::kConst;
  expr.const_kind = content::py::ConstKind::kInt;
  expr.int_value = value;
  return expr;
}

content::py::Stmt make_import() {
  content::py::Stmt stmt;
  stmt.kind = content::py::StmtKind::kImport;
  stmt.name = "smartgis";
  return stmt;
}

content::py::Stmt make_sg_assign() {
  content::py::Expr attr;
  attr.kind = content::py::ExprKind::kAttribute;
  attr.attr = "script";
  attr.receiver = std::make_unique<content::py::Expr>(make_name("smartgis"));
  content::py::Stmt stmt;
  stmt.kind = content::py::StmtKind::kAssign;
  stmt.name = "sg";
  stmt.expr = std::make_unique<content::py::Expr>(std::move(attr));
  return stmt;
}

content::py::Expr make_sg_call(const char* method, std::vector<content::py::Expr> args) {
  content::py::Expr call;
  call.kind = content::py::ExprKind::kCall;
  call.attr = method;
  call.receiver = std::make_unique<content::py::Expr>(make_name("sg"));
  call.args = std::move(args);
  return call;
}

content::py::Stmt make_expr(content::py::Expr expr) {
  content::py::Stmt stmt;
  stmt.kind = content::py::StmtKind::kExpr;
  stmt.loc.line = expr.loc.line;
  stmt.expr = std::make_unique<content::py::Expr>(std::move(expr));
  return stmt;
}

content::py::File suite_with(std::vector<content::py::Stmt> body) {
  content::py::Stmt main;
  main.kind = content::py::StmtKind::kFunc;
  main.name = "main";
  main.body.push_back(make_sg_assign());
  for (content::py::Stmt& stmt : body) {
    main.body.push_back(std::move(stmt));
  }
  content::py::File file;
  file.body.push_back(make_import());
  file.body.push_back(std::move(main));
  return file;
}

bool lower_ok(const content::py::File& file,
              content::code::Module* module,
              std::vector<content::py::Diagnostic>* diags) {
  content::py::BoundProgram program;
  if (!content::py::check_suite(file, &program, diags)) {
    dump_diags(*diags);
    return false;
  }
  diags->clear();
  if (!content::code::lower_program(program, module, diags)) {
    dump_diags(*diags);
    return false;
  }
  return true;
}

void test_parse_if_present() {
  const char* source =
      "import smartgis\n"
      "def main():\n"
      "    sg = smartgis.script\n"
      "    sg.mark(\"a\")\n";
  content::py::File file;
  std::vector<content::py::Diagnostic> diags;
  const bool ok = content::py::parse_python(source, &file, &diags);
  if (!ok) {
    const std::string message = diags.empty() ? std::string() : diags.front().message;
    const bool skipped = message.find("not generated") != std::string::npos ||
                         message.find("grammar") != std::string::npos;
    if (!skipped) {
      dump_diags(diags);
    }
    expect(skipped, "parse failed for a reason other than a missing grammar");
    return;
  }
  expect(!file.body.empty(), "parsed suite has statements");
}

void test_sema_errors() {
  {
    content::py::File file;
    file.body.push_back(make_import());
    std::vector<content::py::Diagnostic> diags;
    content::py::BoundProgram program;
    expect(!content::py::check_suite(file, &program, &diags), "missing main fails");
    expect(has_text(diags, "main"), "missing main diagnostic");
  }
  {
    std::vector<content::py::Stmt> body;
    body.push_back(make_expr(make_sg_call("not_a_real_verb_zz", {})));
    const content::py::File file = suite_with(std::move(body));
    std::vector<content::py::Diagnostic> diags;
    content::py::BoundProgram program;
    expect(!content::py::check_suite(file, &program, &diags), "unknown verb fails");
    expect(has_text(diags, "unknown verb"), "unknown verb diagnostic");
  }
  {
    content::py::Expr call;
    call.kind = content::py::ExprKind::kCall;
    call.attr = "mark";
    call.receiver = std::make_unique<content::py::Expr>(make_name("other"));
    std::vector<content::py::Expr> args;
    args.push_back(make_str("a"));
    call.args = std::move(args);
    std::vector<content::py::Stmt> body;
    body.push_back(make_expr(std::move(call)));
    const content::py::File file = suite_with(std::move(body));
    std::vector<content::py::Diagnostic> diags;
    content::py::BoundProgram program;
    expect(!content::py::check_suite(file, &program, &diags), "non-sg call fails");
    expect(has_text(diags, "not sg"), "non-sg diagnostic");
  }
  {
    std::vector<content::py::Expr> args;
    args.push_back(make_str("dem"));
    args.push_back(make_str("leaf"));
    std::vector<content::py::Stmt> body;
    body.push_back(make_expr(make_sg_call("resolve_data", std::move(args))));
    const content::py::File file = suite_with(std::move(body));
    std::vector<content::py::Diagnostic> diags;
    content::py::BoundProgram program;
    expect(!content::py::check_suite(file, &program, &diags), "missing as_ fails");
    expect(has_text(diags, "as_"), "missing as_ diagnostic");
  }
}

void test_lower_slot_and_rclick() {
  content::code::Module slotted;
  {
    std::vector<content::py::Expr> resolve_args;
    resolve_args.push_back(make_str("dem"));
    resolve_args.push_back(make_str("leaf"));
    content::py::Expr resolve = make_sg_call("resolve_data", std::move(resolve_args));
    resolve.named.emplace_back("as_", make_str("dem"));
    std::vector<content::py::Expr> mark_args;
    mark_args.push_back(make_str("$dem"));
    std::vector<content::py::Stmt> body;
    body.push_back(make_expr(std::move(resolve)));
    body.push_back(make_expr(make_sg_call("mark", std::move(mark_args))));
    const content::py::File file = suite_with(std::move(body));
    std::vector<content::py::Diagnostic> diags;
    expect(lower_ok(file, &slotted, &diags), "lower $dem suite");
    int dem = -1;
    for (std::size_t i = 0; i < slotted.slot_names.size(); ++i) {
      if (slotted.slot_names[i] == "dem") {
        dem = static_cast<int>(i);
      }
    }
    expect(dem >= 0, "dem slot exists");
    bool saw_slot = false;
    for (const content::code::StrPart& part : slotted.str_parts) {
      if (part.is_slot && dem >= 0 && part.index == static_cast<std::uint32_t>(dem)) {
        saw_slot = true;
      }
    }
    expect(saw_slot, "$dem becomes a slot StrPart");
  }
  {
    std::vector<content::py::Expr> args;
    args.push_back(make_str("shell_client"));
    args.push_back(make_int(3));
    args.push_back(make_int(4));
    std::vector<content::py::Stmt> body;
    body.push_back(make_expr(make_sg_call("rclick", std::move(args))));
    const content::py::File file = suite_with(std::move(body));
    content::code::Module module;
    std::vector<content::py::Diagnostic> diags;
    expect(lower_ok(file, &module, &diags), "lower rclick");
    expect(!module.insts.empty() && module.insts[0].op == content::code::Op::kClick,
           "rclick becomes kClick");
    bool saw_right = false;
    for (const std::string& text : module.strings) {
      if (text == "right") {
        saw_right = true;
      }
    }
    expect(saw_right, "rclick button is right");
  }
  {
    std::vector<content::py::Stmt> body;
    body.push_back(make_expr(make_sg_call("detach_maps", {})));
    const content::py::File file = suite_with(std::move(body));
    content::code::Module module;
    std::vector<content::py::Diagnostic> diags;
    expect(lower_ok(file, &module, &diags), "lower detach_maps alias");
    expect(!module.insts.empty() && module.insts[0].op == content::code::Op::kDetachViews,
           "detach_maps is kDetachViews");
  }
}

void test_eval_stops() {
  std::vector<content::py::Expr> mark_a;
  mark_a.push_back(make_str("a"));
  std::vector<content::py::Expr> pump_args;
  pump_args.push_back(make_int(10));
  std::vector<content::py::Expr> click_args;
  click_args.push_back(make_str("shell_client"));
  click_args.push_back(make_int(1));
  click_args.push_back(make_int(2));
  std::vector<content::py::Expr> mark_b;
  mark_b.push_back(make_str("b"));
  std::vector<content::py::Stmt> body;
  body.push_back(make_expr(make_sg_call("mark", std::move(mark_a))));
  body.push_back(make_expr(make_sg_call("pump", std::move(pump_args))));
  body.push_back(make_expr(make_sg_call("click", std::move(click_args))));
  body.push_back(make_expr(make_sg_call("mark", std::move(mark_b))));
  const content::py::File file = suite_with(std::move(body));
  content::code::Module module;
  std::vector<content::py::Diagnostic> diags;
  expect(lower_ok(file, &module, &diags), "lower eval suite");

  content::CapabilityHost host;
  std::vector<std::string> marks;
  std::vector<int> pumps;
  int clicks = 0;
  host.horizon.mark = [&](const std::string& token) { marks.push_back(token); };
  host.horizon.pump = [&](int ms) { pumps.push_back(ms); };
  host.horizon.post_click = [&](int, int, int, int) {
    ++clicks;
    return false;
  };
  std::string error;
  expect(!content::code::eval_module(host, module, &error), "failing click stops eval");
  expect(marks.size() == 1 && marks[0] == "a", "first mark ran and the next did not");
  expect(pumps.size() == 1 && pumps[0] == 10, "pump ran before the failure");
  expect(clicks == 1, "click was recorded");
  expect(error.find("line") != std::string::npos, "eval error names a line");
}

content::code::Module buffer_pair(std::int32_t left, std::int32_t right) {
  content::code::Module module;
  content::code::Operand a;
  content::code::Operand b;
  a.crs_epsg = left;
  b.crs_epsg = right;
  module.operands.push_back(a);
  module.operands.push_back(b);
  content::code::Inst inst;
  inst.op = content::code::Op::kBuffer;
  inst.operand_begin = 0;
  inst.operand_count = 2;
  module.insts.push_back(inst);
  return module;
}

void test_verify_buffer() {
  std::string error;
  expect(!content::code::verify_module(buffer_pair(0, 0), &error), "crs 0 is rejected");
  expect(error.find("unknown") != std::string::npos, "unknown crs message");
  error.clear();
  expect(!content::code::verify_module(buffer_pair(4326, 3857), &error),
         "mismatched epsg is rejected");
  expect(error.find("mismatch") != std::string::npos, "mismatch message");
  error.clear();
  expect(content::code::verify_module(buffer_pair(4326, 4326), &error),
         "epsg 4326 on both operands is accepted");
  expect(error.empty(), "matching crs has no diagnostic");
}

}  // namespace

int main() {
  test_parse_if_present();
  test_sema_errors();
  test_lower_slot_and_rclick();
  test_eval_stops();
  test_verify_buffer();
  if (g_fails != 0) {
    std::fprintf(stderr, "codegen_ir_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::printf("codegen_ir_test: ok\n");
  return 0;
}
