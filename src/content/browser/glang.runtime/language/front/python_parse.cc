// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Python3 → content::py::File. This is the only translation unit that includes
// the generated ANTLR parser. Generated C++ is not checked in.
//
// Grammars are grammars-v4 python/python3 (Bart Kiers, MIT header kept),
// with the upstream Cpp/transformGrammar.py edits already applied so the
// C++ target accepts them: `this.` → `this->`, and
//   @header {#include "Python3LexerBase.h"}
//   @header {#include "Python3ParserBase.h"}
// Super classes are not generated. They live beside the grammars and must
// be compiled in the same target that can see the generated headers:
//   src/content/browser/glang.runtime/language/front/python/Python3LexerBase.h
//   src/content/browser/glang.runtime/language/front/python/Python3LexerBase.cpp
//   src/content/browser/glang.runtime/language/front/python/Python3ParserBase.h
//   src/content/browser/glang.runtime/language/front/python/Python3ParserBase.cpp
// Python3LexerBase.cpp names token types as app_py:: because of -package.
//
// Generator: ANTLR 4.13.2, -Dlanguage=Cpp -visitor -package app_py.
// $root_gen_dir is out/Debug/gen or out/Release/gen. Output directory:
//   $root_gen_dir/g4/il.runtime/python/cpp
// A single java invocation does not work: tokenVocab is loaded beside the
// parser .g4, while -o writes Python3Lexer.tokens into the output directory.
// Pass basenames (cwd = the grammar directory) so -o stays flat; a relative
// path with directories is nested under -o. Two steps:
//
//   java -jar third_party/.tools/antlr-4.13.2-complete.jar -Dlanguage=Cpp -visitor -package app_py -o $root_gen_dir/g4/il.runtime/python/cpp Python3Lexer.g4
//   java -jar third_party/.tools/antlr-4.13.2-complete.jar -Dlanguage=Cpp -visitor -package app_py -lib $root_gen_dir/g4/il.runtime/python/cpp -o $root_gen_dir/g4/il.runtime/python/cpp Python3Parser.g4
//
// antlr_library() runs one action and does not pass -lib, so it will not
// generate this split. Names follow the grammar identifiers:
//   Python3Lexer.{h,cpp}  Python3Parser.{h,cpp}
//   Python3ParserBaseVisitor.{h,cpp}  Python3ParserVisitor.{h,cpp}
//   Python3ParserBaseListener.{h,cpp}  Python3ParserListener.{h,cpp}
// extra_include_dirs must add
//   //src/content/browser/glang.runtime/language/front/python
// so the generated lexer can include Python3LexerBase.h. This TU includes
// Python3Parser.h from the output directory. Until that header is on the
// include path, parse_python returns "python grammar not generated".

#include "content/browser/glang.runtime/language/front/parse.h"

#include <cctype>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(__has_include)
#  if __has_include("Python3Parser.h")
#    ifndef NOMINMAX
#      define NOMINMAX
#    endif
#    include "Python3Lexer.h"
#    include "Python3Parser.h"
#    include "Python3ParserBaseVisitor.h"
#    include "antlr4-runtime.h"
#    define SMARTGIS_PY_ANTLR 1
#  endif
#endif

namespace content {
namespace py {
namespace {

#if defined(SMARTGIS_PY_ANTLR)

constexpr std::size_t kSnippetMax = 80;

Loc loc_of(antlr4::ParserRuleContext* ctx) {
  Loc loc;
  if (ctx == nullptr) {
    return loc;
  }
  antlr4::Token* tok = ctx->getStart();
  if (tok == nullptr) {
    return loc;
  }
  const std::size_t line = tok->getLine();
  loc.line = line == 0 ? 1 : static_cast<uint32_t>(line);
  loc.column = static_cast<uint32_t>(tok->getCharPositionInLine() + 1);
  if (loc.column == 0) {
    loc.column = 1;
  }
  return loc;
}

std::string snippet_of(antlr4::tree::ParseTree* tree) {
  if (tree == nullptr) {
    return {};
  }
  std::string text = tree->getText();
  for (char& ch : text) {
    if (ch == '\n' || ch == '\r' || ch == '\t') {
      ch = ' ';
    }
  }
  if (text.size() > kSnippetMax) {
    text.resize(kSnippetMax);
    text.append("...");
  }
  return text;
}

bool has_direct_token(antlr4::ParserRuleContext* ctx, std::size_t type) {
  if (ctx == nullptr) {
    return false;
  }
  for (antlr4::tree::ParseTree* child : ctx->children) {
    auto* term = dynamic_cast<antlr4::tree::TerminalNode*>(child);
    if (term != nullptr && term->getSymbol() != nullptr &&
        term->getSymbol()->getType() == type) {
      return true;
    }
  }
  return false;
}

Expr unsupported_expr(Loc loc, std::string snippet) {
  Expr expr;
  expr.kind = ExprKind::kUnsupported;
  expr.loc = loc;
  expr.text = std::move(snippet);
  return expr;
}

Stmt unsupported_stmt(Loc loc) {
  Stmt stmt;
  stmt.kind = StmtKind::kUnsupported;
  stmt.loc = loc;
  return stmt;
}

void drop_leading_docstring(std::vector<Stmt>* body) {
  if (body == nullptr || body->empty()) {
    return;
  }
  const Stmt& first = body->front();
  if (first.kind == StmtKind::kExpr && first.expr != nullptr &&
      first.expr->kind == ExprKind::kConst &&
      first.expr->const_kind == ConstKind::kStr) {
    body->erase(body->begin());
  }
}

bool decimal_i64(std::string_view text, int64_t* out) {
  if (text.empty() || out == nullptr) {
    return false;
  }
  int64_t value = 0;
  for (char ch : text) {
    if (ch < '0' || ch > '9') {
      return false;
    }
    const int digit = ch - '0';
    if (value > (9223372036854775807LL - digit) / 10) {
      return false;
    }
    value = value * 10 + digit;
  }
  *out = value;
  return true;
}

// Decode one STRING token. f-strings, bytes, and numeric escapes stay unmapped.
bool decode_string_token(std::string_view token, std::string* out) {
  if (out == nullptr) {
    return false;
  }
  std::size_t i = 0;
  bool raw = false;
  while (i < token.size() &&
         std::isalpha(static_cast<unsigned char>(token[i])) != 0) {
    const char kind = static_cast<char>(
        std::tolower(static_cast<unsigned char>(token[i])));
    if (kind == 'r') {
      raw = true;
    } else if (kind != 'u') {
      return false;
    }
    ++i;
    if (i > 2) {
      return false;
    }
  }
  if (i >= token.size()) {
    return false;
  }
  const char quote = token[i];
  if (quote != '\'' && quote != '"') {
    return false;
  }
  const bool triple =
      i + 2 < token.size() && token[i + 1] == quote && token[i + 2] == quote;
  const std::size_t begin = i + (triple ? 3 : 1);
  const std::size_t mark = triple ? 3 : 1;
  if (token.size() < begin + mark) {
    return false;
  }
  const std::size_t end = token.size() - mark;
  std::string value;
  value.reserve(end - begin);
  for (std::size_t p = begin; p < end; ++p) {
    const char ch = token[p];
    if (!raw && ch == '\\') {
      if (p + 1 >= end) {
        return false;
      }
      const char esc = token[++p];
      switch (esc) {
        case '\\':
          value.push_back('\\');
          break;
        case '\'':
          value.push_back('\'');
          break;
        case '"':
          value.push_back('"');
          break;
        case 'n':
          value.push_back('\n');
          break;
        case 'r':
          value.push_back('\r');
          break;
        case 't':
          value.push_back('\t');
          break;
        case 'a':
          value.push_back('\a');
          break;
        case 'b':
          value.push_back('\b');
          break;
        case 'f':
          value.push_back('\f');
          break;
        case 'v':
          value.push_back('\v');
          break;
        case '\n':
          break;
        default:
          if (esc == 'x' || esc == 'u' || esc == 'U' || esc == 'N' ||
              (esc >= '0' && esc <= '7')) {
            return false;
          }
          value.push_back('\\');
          value.push_back(esc);
          break;
      }
    } else {
      value.push_back(ch);
    }
  }
  *out = std::move(value);
  return true;
}

Expr map_test(app_py::Python3Parser::TestContext* ctx);
Expr map_atom(app_py::Python3Parser::AtomContext* ctx);

Expr map_number(antlr4::tree::TerminalNode* node, Loc loc) {
  const std::string spelling = node == nullptr ? std::string() : node->getText();
  int64_t value = 0;
  if (!decimal_i64(spelling, &value)) {
    return unsupported_expr(loc, spelling);
  }
  Expr expr;
  expr.kind = ExprKind::kConst;
  expr.const_kind = ConstKind::kInt;
  expr.loc = loc;
  expr.int_value = value;
  return expr;
}

Expr map_bool(bool value, Loc loc) {
  Expr expr;
  expr.kind = ExprKind::kConst;
  expr.const_kind = ConstKind::kBool;
  expr.loc = loc;
  expr.bool_value = value;
  return expr;
}

Expr map_none(Loc loc) {
  Expr expr;
  expr.kind = ExprKind::kConst;
  expr.const_kind = ConstKind::kNone;
  expr.loc = loc;
  return expr;
}

Expr map_name(app_py::Python3Parser::NameContext* name, Loc loc) {
  if (name == nullptr) {
    return unsupported_expr(loc, {});
  }
  Expr expr;
  expr.kind = ExprKind::kName;
  expr.loc = loc;
  expr.text = name->getText();
  return expr;
}

Expr map_strings(app_py::Python3Parser::AtomContext* ctx) {
  const Loc loc = loc_of(ctx);
  if (ctx == nullptr) {
    return unsupported_expr(loc, {});
  }
  std::string all;
  for (antlr4::tree::TerminalNode* node : ctx->STRING()) {
    if (node == nullptr) {
      return unsupported_expr(loc, snippet_of(ctx));
    }
    std::string piece;
    if (!decode_string_token(node->getText(), &piece)) {
      return unsupported_expr(loc, snippet_of(ctx));
    }
    all += piece;
  }
  Expr expr;
  expr.kind = ExprKind::kConst;
  expr.const_kind = ConstKind::kStr;
  expr.loc = loc;
  expr.text = std::move(all);
  return expr;
}

Expr map_paren(app_py::Python3Parser::AtomContext* ctx) {
  const Loc loc = loc_of(ctx);
  if (ctx == nullptr || ctx->yield_expr() != nullptr) {
    return unsupported_expr(loc, snippet_of(ctx));
  }
  app_py::Python3Parser::Testlist_compContext* comp = ctx->testlist_comp();
  if (comp == nullptr || comp->comp_for() != nullptr ||
      !comp->star_expr().empty() || comp->test().size() != 1 ||
      has_direct_token(comp, app_py::Python3Lexer::COMMA)) {
    return unsupported_expr(loc, snippet_of(ctx));
  }
  return map_test(comp->test(0));
}

Expr map_atom(app_py::Python3Parser::AtomContext* ctx) {
  const Loc loc = loc_of(ctx);
  if (ctx == nullptr) {
    return unsupported_expr(loc, {});
  }
  if (ctx->name() != nullptr) {
    return map_name(ctx->name(), loc);
  }
  if (ctx->NUMBER() != nullptr) {
    return map_number(ctx->NUMBER(), loc);
  }
  if (!ctx->STRING().empty()) {
    return map_strings(ctx);
  }
  if (ctx->TRUE() != nullptr) {
    return map_bool(true, loc);
  }
  if (ctx->FALSE() != nullptr) {
    return map_bool(false, loc);
  }
  if (ctx->NONE() != nullptr) {
    return map_none(loc);
  }
  antlr4::Token* start = ctx->getStart();
  if (start != nullptr && start->getType() == app_py::Python3Lexer::OPEN_PAREN) {
    return map_paren(ctx);
  }
  return unsupported_expr(loc, snippet_of(ctx));
}

bool fill_args(app_py::Python3Parser::TrailerContext* trailer, Expr* call) {
  if (trailer == nullptr || call == nullptr) {
    return false;
  }
  app_py::Python3Parser::ArglistContext* args = trailer->arglist();
  if (args == nullptr) {
    return true;
  }
  for (app_py::Python3Parser::ArgumentContext* arg : args->argument()) {
    if (arg == nullptr || arg->comp_for() != nullptr) {
      return false;
    }
    const bool star = has_direct_token(arg, app_py::Python3Lexer::STAR);
    const bool power = has_direct_token(arg, app_py::Python3Lexer::POWER);
    const bool assign = has_direct_token(arg, app_py::Python3Lexer::ASSIGN);
    const auto tests = arg->test();
    if (star || power) {
      return false;
    }
    if (assign) {
      if (tests.size() != 2) {
        return false;
      }
      Expr key = map_test(tests[0]);
      if (key.kind != ExprKind::kName || key.text.empty()) {
        return false;
      }
      call->named.emplace_back(std::move(key.text), map_test(tests[1]));
    } else {
      if (tests.size() != 1) {
        return false;
      }
      call->args.push_back(map_test(tests[0]));
    }
  }
  return true;
}

Expr make_attr(Expr receiver, app_py::Python3Parser::NameContext* name, Loc loc) {
  if (receiver.kind == ExprKind::kUnsupported) {
    return receiver;
  }
  Expr expr;
  expr.kind = ExprKind::kAttribute;
  expr.loc = receiver.loc;
  if (expr.loc.line == 0) {
    expr.loc = loc;
  }
  expr.attr = name == nullptr ? std::string() : name->getText();
  if (expr.attr.empty()) {
    return unsupported_expr(loc, {});
  }
  expr.receiver = std::make_unique<Expr>(std::move(receiver));
  return expr;
}

// A call on an attribute keeps the method name in attr and the object in receiver.
Expr make_call(Expr callee, app_py::Python3Parser::TrailerContext* trailer, Loc loc) {
  if (callee.kind == ExprKind::kUnsupported) {
    return callee;
  }
  Expr call;
  call.kind = ExprKind::kCall;
  call.loc = callee.loc.line == 0 ? loc : callee.loc;
  if (callee.kind == ExprKind::kAttribute) {
    call.attr = std::move(callee.attr);
    call.receiver = std::move(callee.receiver);
  } else {
    call.receiver = std::make_unique<Expr>(std::move(callee));
  }
  if (!fill_args(trailer, &call)) {
    return unsupported_expr(call.loc, snippet_of(trailer));
  }
  return call;
}

Expr map_atom_expr(app_py::Python3Parser::Atom_exprContext* ctx) {
  const Loc loc = loc_of(ctx);
  if (ctx == nullptr || ctx->atom() == nullptr ||
      has_direct_token(ctx, app_py::Python3Lexer::AWAIT)) {
    return unsupported_expr(loc, snippet_of(ctx));
  }
  Expr cur = map_atom(ctx->atom());
  for (app_py::Python3Parser::TrailerContext* trailer : ctx->trailer()) {
    if (trailer == nullptr || trailer->subscriptlist() != nullptr) {
      return unsupported_expr(loc, snippet_of(ctx));
    }
    if (trailer->name() != nullptr) {
      cur = make_attr(std::move(cur), trailer->name(), loc);
    } else {
      cur = make_call(std::move(cur), trailer, loc);
    }
  }
  return cur;
}

Expr map_expr(app_py::Python3Parser::ExprContext* ctx) {
  const Loc loc = loc_of(ctx);
  if (ctx != nullptr && ctx->atom_expr() != nullptr && ctx->expr().empty()) {
    return map_atom_expr(ctx->atom_expr());
  }
  return unsupported_expr(loc, snippet_of(ctx));
}

Expr map_comparison(app_py::Python3Parser::ComparisonContext* ctx) {
  const Loc loc = loc_of(ctx);
  if (ctx == nullptr || ctx->expr().size() != 1) {
    return unsupported_expr(loc, snippet_of(ctx));
  }
  return map_expr(ctx->expr(0));
}

Expr map_not(app_py::Python3Parser::Not_testContext* ctx) {
  const Loc loc = loc_of(ctx);
  if (ctx == nullptr || ctx->not_test() != nullptr || ctx->comparison() == nullptr) {
    return unsupported_expr(loc, snippet_of(ctx));
  }
  return map_comparison(ctx->comparison());
}

Expr map_and(app_py::Python3Parser::And_testContext* ctx) {
  const Loc loc = loc_of(ctx);
  if (ctx == nullptr || ctx->not_test().size() != 1) {
    return unsupported_expr(loc, snippet_of(ctx));
  }
  return map_not(ctx->not_test(0));
}

Expr map_or(app_py::Python3Parser::Or_testContext* ctx) {
  const Loc loc = loc_of(ctx);
  if (ctx == nullptr || ctx->and_test().size() != 1) {
    return unsupported_expr(loc, snippet_of(ctx));
  }
  return map_and(ctx->and_test(0));
}

Expr map_test(app_py::Python3Parser::TestContext* ctx) {
  const Loc loc = loc_of(ctx);
  if (ctx == nullptr || ctx->lambdef() != nullptr || ctx->test() != nullptr ||
      ctx->or_test().size() != 1) {
    return unsupported_expr(loc, snippet_of(ctx));
  }
  return map_or(ctx->or_test(0));
}

Expr map_testlist_star(app_py::Python3Parser::Testlist_star_exprContext* ctx) {
  const Loc loc = loc_of(ctx);
  if (ctx == nullptr || !ctx->star_expr().empty() || ctx->test().size() != 1 ||
      has_direct_token(ctx, app_py::Python3Lexer::COMMA)) {
    return unsupported_expr(loc, snippet_of(ctx));
  }
  return map_test(ctx->test(0));
}

std::string dotted_text(app_py::Python3Parser::Dotted_nameContext* ctx) {
  if (ctx == nullptr) {
    return {};
  }
  std::string name;
  for (app_py::Python3Parser::NameContext* part : ctx->name()) {
    if (part == nullptr) {
      continue;
    }
    if (!name.empty()) {
      name.push_back('.');
    }
    name += part->getText();
  }
  return name;
}

Stmt map_expr_stmt(app_py::Python3Parser::Expr_stmtContext* ctx) {
  const Loc loc = loc_of(ctx);
  if (ctx == nullptr || ctx->annassign() != nullptr || ctx->augassign() != nullptr ||
      !ctx->yield_expr().empty()) {
    return unsupported_stmt(loc);
  }
  const auto lists = ctx->testlist_star_expr();
  if (lists.size() == 1) {
    Stmt stmt;
    stmt.kind = StmtKind::kExpr;
    stmt.loc = loc;
    stmt.expr = std::make_unique<Expr>(map_testlist_star(lists[0]));
    return stmt;
  }
  if (lists.size() == 2) {
    Expr target = map_testlist_star(lists[0]);
    if (target.kind == ExprKind::kName) {
      Stmt stmt;
      stmt.kind = StmtKind::kAssign;
      stmt.loc = loc;
      stmt.name = std::move(target.text);
      stmt.expr = std::make_unique<Expr>(map_testlist_star(lists[1]));
      return stmt;
    }
  }
  return unsupported_stmt(loc);
}

void append_import(app_py::Python3Parser::Import_stmtContext* ctx,
                   std::vector<Stmt>* out) {
  const Loc loc = loc_of(ctx);
  if (out == nullptr) {
    return;
  }
  app_py::Python3Parser::Import_nameContext* imported =
      ctx == nullptr ? nullptr : ctx->import_name();
  app_py::Python3Parser::Dotted_as_namesContext* names =
      imported == nullptr ? nullptr : imported->dotted_as_names();
  if (ctx == nullptr || ctx->import_from() != nullptr || names == nullptr) {
    out->push_back(unsupported_stmt(loc));
    return;
  }
  bool any = false;
  for (app_py::Python3Parser::Dotted_as_nameContext* one : names->dotted_as_name()) {
    any = true;
    if (one == nullptr || one->dotted_name() == nullptr) {
      out->push_back(unsupported_stmt(loc_of(one)));
      continue;
    }
    Stmt stmt;
    stmt.kind = StmtKind::kImport;
    stmt.loc = loc_of(one);
    stmt.name = dotted_text(one->dotted_name());
    out->push_back(std::move(stmt));
  }
  if (!any) {
    out->push_back(unsupported_stmt(loc));
  }
}

void append_stmt(app_py::Python3Parser::StmtContext* ctx, std::vector<Stmt>* out);

void append_simple(app_py::Python3Parser::Simple_stmtContext* ctx,
                   std::vector<Stmt>* out) {
  if (out == nullptr) {
    return;
  }
  const Loc loc = loc_of(ctx);
  if (ctx == nullptr) {
    out->push_back(unsupported_stmt(loc));
    return;
  }
  if (ctx->expr_stmt() != nullptr) {
    out->push_back(map_expr_stmt(ctx->expr_stmt()));
    return;
  }
  if (ctx->import_stmt() != nullptr) {
    append_import(ctx->import_stmt(), out);
    return;
  }
  out->push_back(unsupported_stmt(loc));
}

void map_block_into(app_py::Python3Parser::BlockContext* ctx, std::vector<Stmt>* body) {
  if (ctx == nullptr || body == nullptr) {
    return;
  }
  if (ctx->simple_stmts() != nullptr) {
    for (app_py::Python3Parser::Simple_stmtContext* simple :
         ctx->simple_stmts()->simple_stmt()) {
      append_simple(simple, body);
    }
  } else {
    for (app_py::Python3Parser::StmtContext* stmt : ctx->stmt()) {
      append_stmt(stmt, body);
    }
  }
  drop_leading_docstring(body);
}

Stmt map_func(app_py::Python3Parser::FuncdefContext* ctx) {
  Stmt stmt;
  stmt.kind = StmtKind::kFunc;
  stmt.loc = loc_of(ctx);
  if (ctx != nullptr && ctx->name() != nullptr) {
    stmt.name = ctx->name()->getText();
  }
  if (ctx != nullptr) {
    map_block_into(ctx->block(), &stmt.body);
  }
  return stmt;
}

Stmt map_compound(app_py::Python3Parser::Compound_stmtContext* ctx) {
  if (ctx != nullptr && ctx->funcdef() != nullptr) {
    return map_func(ctx->funcdef());
  }
  return unsupported_stmt(loc_of(ctx));
}

void append_stmt(app_py::Python3Parser::StmtContext* ctx, std::vector<Stmt>* out) {
  if (out == nullptr) {
    return;
  }
  if (ctx == nullptr) {
    out->push_back(unsupported_stmt(Loc{}));
    return;
  }
  if (ctx->simple_stmts() != nullptr) {
    for (app_py::Python3Parser::Simple_stmtContext* simple :
         ctx->simple_stmts()->simple_stmt()) {
      append_simple(simple, out);
    }
    return;
  }
  if (ctx->compound_stmt() != nullptr) {
    out->push_back(map_compound(ctx->compound_stmt()));
    return;
  }
  out->push_back(unsupported_stmt(loc_of(ctx)));
}

// Folds a successful parse tree into the subset AST. Unmapped nodes stay.
class AstFold final : public app_py::Python3ParserBaseVisitor {
 public:
  File file;

  std::any visitFile_input(app_py::Python3Parser::File_inputContext* ctx) override {
    if (ctx != nullptr) {
      for (app_py::Python3Parser::StmtContext* stmt : ctx->stmt()) {
        append_stmt(stmt, &file.body);
      }
    }
    drop_leading_docstring(&file.body);
    return {};
  }
};

// Records the first syntax error. Parser mismatches bail without a listener
// callback, so the strategy notes the current token before cancelling.
class FirstError final : public antlr4::BaseErrorListener {
 public:
  bool seen = false;
  Loc loc;
  std::string message;

  void syntaxError(antlr4::Recognizer*,
                   antlr4::Token*,
                   std::size_t line,
                   std::size_t char_position_in_line,
                   const std::string& msg,
                   std::exception_ptr) override {
    if (!seen) {
      seen = true;
      loc.line = line == 0 ? 1 : static_cast<uint32_t>(line);
      loc.column = static_cast<uint32_t>(char_position_in_line + 1);
      if (loc.column == 0) {
        loc.column = 1;
      }
      message = msg.empty() ? "syntax error" : msg;
    }
    throw antlr4::ParseCancellationException(message);
  }
};

class ReportAndBail final : public antlr4::BailErrorStrategy {
 public:
  explicit ReportAndBail(FirstError* sink) : sink_(sink) {}

  void recover(antlr4::Parser* recognizer, std::exception_ptr err) override {
    note(recognizer);
    antlr4::BailErrorStrategy::recover(recognizer, std::move(err));
  }

  antlr4::Token* recoverInline(antlr4::Parser* recognizer) override {
    note(recognizer);
    return antlr4::BailErrorStrategy::recoverInline(recognizer);
  }

 private:
  void note(antlr4::Parser* recognizer) {
    if (sink_ == nullptr || sink_->seen || recognizer == nullptr) {
      return;
    }
    sink_->seen = true;
    sink_->message = "syntax error";
    antlr4::Token* tok = recognizer->getCurrentToken();
    if (tok == nullptr) {
      return;
    }
    const std::size_t line = tok->getLine();
    sink_->loc.line = line == 0 ? 1 : static_cast<uint32_t>(line);
    sink_->loc.column = static_cast<uint32_t>(tok->getCharPositionInLine() + 1);
    if (sink_->loc.column == 0) {
      sink_->loc.column = 1;
    }
    if (tok->getType() == antlr4::Token::EOF) {
      return;
    }
    const std::string near = tok->getText();
    if (!near.empty() && near.size() <= 40) {
      sink_->message.append(" near '");
      sink_->message.append(near);
      sink_->message.push_back('\'');
    }
  }

  FirstError* sink_ = nullptr;
};

bool parse_with_antlr(std::string_view source, File* out, std::vector<Diagnostic>* diags) {
  std::string text(source);
  if (text.empty() || text.back() != '\n') {
    text.push_back('\n');
  }
  FirstError errors;
  try {
    antlr4::ANTLRInputStream input(text);
    app_py::Python3Lexer lexer(&input);
    lexer.removeErrorListeners();
    lexer.addErrorListener(&errors);
    antlr4::CommonTokenStream tokens(&lexer);
    app_py::Python3Parser parser(&tokens);
    parser.removeErrorListeners();
    parser.addErrorListener(&errors);
    parser.setErrorHandler(std::make_shared<ReportAndBail>(&errors));
    app_py::Python3Parser::File_inputContext* tree = parser.file_input();
    if (errors.seen || parser.getNumberOfSyntaxErrors() > 0 || tree == nullptr) {
      if (errors.message.empty()) {
        errors.message = "syntax error";
      }
      return detail::fail_parse(out, diags, errors.loc, std::move(errors.message));
    }
    AstFold fold;
    fold.visit(tree);
    if (diags != nullptr) {
      diags->clear();
    }
    *out = std::move(fold.file);
    return true;
  } catch (const std::exception&) {
    if (errors.message.empty()) {
      errors.message = "syntax error";
    }
    return detail::fail_parse(out, diags, errors.loc, std::move(errors.message));
  }
}

#endif  // SMARTGIS_PY_ANTLR

}  // namespace

bool parse_python(std::string_view source, File* out, std::vector<Diagnostic>* diags) {
  if (out == nullptr) {
    return detail::fail_parse(nullptr, diags, Loc{}, "python parse missing output");
  }
  out->body.clear();
#if defined(SMARTGIS_PY_ANTLR)
  return parse_with_antlr(source, out, diags);
#else
  (void)source;
  return detail::fail_parse(out, diags, Loc{}, "python grammar not generated");
#endif
}

}  // namespace py
}  // namespace content
