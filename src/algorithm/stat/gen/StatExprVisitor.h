
// Generated from src/algorithm/stat/StatExpr.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "algorithm/stat/gen/StatExprParser.h"


namespace smtstat {

/**
 * This class defines an abstract visitor for a parse tree
 * produced by StatExprParser.
 */
class  StatExprVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by StatExprParser.
   */
    virtual std::any visitProgram(StatExprParser::ProgramContext *context) = 0;

    virtual std::any visitAssignment(StatExprParser::AssignmentContext *context) = 0;

    virtual std::any visitExpr(StatExprParser::ExprContext *context) = 0;

    virtual std::any visitPostfix(StatExprParser::PostfixContext *context) = 0;

    virtual std::any visitAtom(StatExprParser::AtomContext *context) = 0;

    virtual std::any visitArgs(StatExprParser::ArgsContext *context) = 0;


};

}  // namespace smtstat
