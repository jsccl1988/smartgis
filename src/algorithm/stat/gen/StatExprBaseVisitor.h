
// Generated from src/algorithm/stat/StatExpr.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "algorithm/stat/gen/StatExprVisitor.h"


namespace smtstat {

/**
 * This class provides an empty implementation of StatExprVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  StatExprBaseVisitor : public StatExprVisitor {
public:

  virtual std::any visitProgram(StatExprParser::ProgramContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignment(StatExprParser::AssignmentContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExpr(StatExprParser::ExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPostfix(StatExprParser::PostfixContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAtom(StatExprParser::AtomContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArgs(StatExprParser::ArgsContext *ctx) override {
    return visitChildren(ctx);
  }


};

}  // namespace smtstat
