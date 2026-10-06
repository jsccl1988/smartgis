// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// ANTLR 4 frontend for Interact.g4 (.il). Generated sources live under
// out/{Debug|Release}/gen/... (not checked in).

#include "app/views/il.runtime/frontend/parse.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "InteractLexer.h"
#include "InteractParser.h"
#include "antlr4-runtime.h"

namespace app {
namespace detail {
namespace {

// Decode STRING token text (outer quotes + ESC from Interact.g4).
// Must match testing/tools/loop/interact/dsl.py _unquote — otherwise inline
// JSON args=("{\"k\":\"$v\"}") keep backslashes and RapidJSON rejects them.
std::string unquote_string(const std::string& raw) {
  if (raw.size() < 2 || raw.front() != '"' || raw.back() != '"') {
    return raw;
  }
  std::string out;
  out.reserve(raw.size() - 2);
  for (size_t i = 1; i + 1 < raw.size(); ++i) {
    const char c = raw[i];
    if (c == '\\' && i + 1 + 1 < raw.size()) {
      const char n = raw[i + 1];
      if (n == '"' || n == '\\') {
        out.push_back(n);
        ++i;
        continue;
      }
      if (n == 'n') {
        out.push_back('\n');
        ++i;
        continue;
      }
      if (n == 'r') {
        out.push_back('\r');
        ++i;
        continue;
      }
      if (n == 't') {
        out.push_back('\t');
        ++i;
        continue;
      }
    }
    out.push_back(c);
  }
  return out;
}

std::string tok_text(antlr4::tree::TerminalNode* node) {
  return node ? node->getText() : std::string();
}

Point point_from_ctx(interact::InteractParser::PointContext* ctx) {
  Point p;
  if (!ctx) {
    return p;
  }
  using namespace base::tuple::literals;
  const auto ints = ctx->INT();
  if (ints.size() >= 2) {
    p["x"_t] = std::stoi(tok_text(ints[0]));
    p["y"_t] = std::stoi(tok_text(ints[1]));
  }
  return p;
}

Value value_from_ctx(interact::InteractParser::ValueContext* ctx) {
  if (!ctx) {
    return make_int(0);
  }
  if (ctx->INT()) {
    return make_int(std::stoi(tok_text(ctx->INT())));
  }
  if (ctx->STRING()) {
    return make_string(unquote_string(tok_text(ctx->STRING())));
  }
  if (ctx->IDENT()) {
    return make_ident(tok_text(ctx->IDENT()));
  }
  if (ctx->point()) {
    return make_point(point_from_ctx(ctx->point()));
  }
  if (ctx->pointList()) {
    std::vector<Point> pts;
    for (auto* pt : ctx->pointList()->point()) {
      pts.push_back(point_from_ctx(pt));
    }
    return make_point_list(std::move(pts));
  }
  return make_int(0);
}

DriverFilter driver_from_ctx(interact::InteractParser::DriverAnnoContext* ctx) {
  if (!ctx) {
    return DriverFilter::kAny;
  }
  if (ctx->ATINPROC()) {
    return DriverFilter::kInproc;
  }
  if (ctx->ATOS()) {
    return DriverFilter::kOs;
  }
  if (ctx->ATDRIVERS() && ctx->identList()) {
    DriverFilter f = DriverFilter::kAny;
    for (auto* id : ctx->identList()->IDENT()) {
      const std::string t = tok_text(id);
      if (t == "inproc") {
        f = DriverFilter::kInproc;
      } else if (t == "os") {
        f = DriverFilter::kOs;
      }
    }
    return f;
  }
  return DriverFilter::kAny;
}

std::string inject_from_ctx(interact::InteractParser::InjectAnnoContext* ctx) {
  if (!ctx || !ctx->IDENT()) {
    return {};
  }
  return tok_text(ctx->IDENT());
}

Stmt stmt_from_ctx(interact::InteractParser::StmtContext* ctx);

Stmt call_from_ctx(interact::InteractParser::CallStmtContext* ctx) {
  using namespace base::tuple::literals;
  CallStmt call;
  if (ctx && ctx->IDENT()) {
    call.name = tok_text(ctx->IDENT());
  }
  if (ctx) {
    call.drivers = driver_from_ctx(ctx->driverAnno());
    call.inject = inject_from_ctx(ctx->injectAnno());
    if (ctx->argList()) {
      for (auto* a : ctx->argList()->arg()) {
        Arg arg;
        if (a->namedArg()) {
          arg["name"_t] = tok_text(a->namedArg()->IDENT());
          arg["value"_t] = value_from_ctx(a->namedArg()->value());
        } else if (a->positionalArg()) {
          arg["value"_t] = value_from_ctx(a->positionalArg()->value());
        }
        call.args.push_back(std::move(arg));
      }
    }
  }
  Stmt s;
  s.node = std::move(call);
  return s;
}

Stmt block_from_seq(interact::InteractParser::SeqStmtContext* ctx) {
  auto block = std::make_unique<BlockStmt>();
  block->kind = BlockStmt::Kind::kSeq;
  block->drivers = driver_from_ctx(ctx->driverAnno());
  for (auto* child : ctx->stmt()) {
    block->body.push_back(stmt_from_ctx(child));
  }
  Stmt s;
  s.node = std::move(block);
  return s;
}

Stmt block_from_repeat(interact::InteractParser::RepeatStmtContext* ctx) {
  auto block = std::make_unique<BlockStmt>();
  block->kind = BlockStmt::Kind::kRepeat;
  block->repeat_count = ctx->INT() ? std::stoi(tok_text(ctx->INT())) : 1;
  block->drivers = driver_from_ctx(ctx->driverAnno());
  for (auto* child : ctx->stmt()) {
    block->body.push_back(stmt_from_ctx(child));
  }
  Stmt s;
  s.node = std::move(block);
  return s;
}

Stmt block_from_chord(interact::InteractParser::ChordStmtContext* ctx) {
  auto block = std::make_unique<BlockStmt>();
  block->kind = BlockStmt::Kind::kChord;
  block->drivers = driver_from_ctx(ctx->driverAnno());
  if (ctx->identList()) {
    for (auto* id : ctx->identList()->IDENT()) {
      block->chord_mods.push_back(tok_text(id));
    }
  }
  for (auto* child : ctx->stmt()) {
    block->body.push_back(stmt_from_ctx(child));
  }
  Stmt s;
  s.node = std::move(block);
  return s;
}

Stmt stmt_from_ctx(interact::InteractParser::StmtContext* ctx) {
  if (!ctx) {
    return {};
  }
  if (ctx->seqStmt()) {
    return block_from_seq(ctx->seqStmt());
  }
  if (ctx->repeatStmt()) {
    return block_from_repeat(ctx->repeatStmt());
  }
  if (ctx->chordStmt()) {
    return block_from_chord(ctx->chordStmt());
  }
  return call_from_ctx(ctx->callStmt());
}

class CollectErrors : public antlr4::BaseErrorListener {
 public:
  void syntaxError(antlr4::Recognizer*,
                   antlr4::Token*,
                   size_t,
                   size_t,
                   const std::string& msg,
                   std::exception_ptr) override {
    if (!message.empty()) {
      message += "; ";
    }
    message += msg;
  }
  std::string message;
};

}  // namespace

bool parse_interact_source(const std::string& src,
                           ScriptAst* out,
                           std::string* err) {
  if (!out) {
    return false;
  }
  antlr4::ANTLRInputStream input(src);
  interact::InteractLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  interact::InteractParser parser(&tokens);
  CollectErrors errors;
  lexer.removeErrorListeners();
  parser.removeErrorListeners();
  lexer.addErrorListener(&errors);
  parser.addErrorListener(&errors);
  interact::InteractParser::ScriptFileContext* tree = parser.scriptFile();
  if (!errors.message.empty() || parser.getNumberOfSyntaxErrors() > 0) {
    if (err) {
      *err = errors.message.empty() ? "syntax error" : errors.message;
    }
    return false;
  }
  out->stmts.clear();
  if (tree->stringLiteral() && tree->stringLiteral()->STRING()) {
    out->name = unquote_string(tok_text(tree->stringLiteral()->STRING()));
  }
  for (auto* s : tree->stmt()) {
    out->stmts.push_back(stmt_from_ctx(s));
  }
  return true;
}

}  // namespace detail
}  // namespace app
