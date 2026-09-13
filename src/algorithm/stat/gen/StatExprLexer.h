
// Generated from src/algorithm/stat/StatExpr.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"


namespace smtstat {


class  StatExprLexer : public antlr4::Lexer {
public:
  enum {
    FIELD = 1, IDENT = 2, NUMBER = 3, EQ = 4, PLUS = 5, MINUS = 6, STAR = 7, 
    SLASH = 8, POW = 9, LPAREN = 10, RPAREN = 11, LBRACE = 12, RBRACE = 13, 
    COMMA = 14, WS = 15
  };

  explicit StatExprLexer(antlr4::CharStream *input);

  ~StatExprLexer() override;


  std::string getGrammarFileName() const override;

  const std::vector<std::string>& getRuleNames() const override;

  const std::vector<std::string>& getChannelNames() const override;

  const std::vector<std::string>& getModeNames() const override;

  const antlr4::dfa::Vocabulary& getVocabulary() const override;

  antlr4::atn::SerializedATNView getSerializedATN() const override;

  const antlr4::atn::ATN& getATN() const override;

  // By default the static state used to implement the lexer is lazily initialized during the first
  // call to the constructor. You can call this function if you wish to initialize the static state
  // ahead of time.
  static void initialize();

private:

  // Individual action functions triggered by action() above.

  // Individual semantic predicate functions triggered by sempred() above.

};

}  // namespace smtstat
