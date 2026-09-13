
// Generated from src/algorithm/stat/StatExpr.g4 by ANTLR 4.13.2


#include "algorithm/stat/gen/StatExprVisitor.h"

#include "algorithm/stat/gen/StatExprParser.h"


using namespace antlrcpp;
using namespace smtstat;

using namespace antlr4;

namespace {

struct StatExprParserStaticData final {
  StatExprParserStaticData(std::vector<std::string> ruleNames,
                        std::vector<std::string> literalNames,
                        std::vector<std::string> symbolicNames)
      : ruleNames(std::move(ruleNames)), literalNames(std::move(literalNames)),
        symbolicNames(std::move(symbolicNames)),
        vocabulary(this->literalNames, this->symbolicNames) {}

  StatExprParserStaticData(const StatExprParserStaticData&) = delete;
  StatExprParserStaticData(StatExprParserStaticData&&) = delete;
  StatExprParserStaticData& operator=(const StatExprParserStaticData&) = delete;
  StatExprParserStaticData& operator=(StatExprParserStaticData&&) = delete;

  std::vector<antlr4::dfa::DFA> decisionToDFA;
  antlr4::atn::PredictionContextCache sharedContextCache;
  const std::vector<std::string> ruleNames;
  const std::vector<std::string> literalNames;
  const std::vector<std::string> symbolicNames;
  const antlr4::dfa::Vocabulary vocabulary;
  antlr4::atn::SerializedATNView serializedATN;
  std::unique_ptr<antlr4::atn::ATN> atn;
};

::antlr4::internal::OnceFlag statexprParserOnceFlag;
#if ANTLR4_USE_THREAD_LOCAL_CACHE
static thread_local
#endif
std::unique_ptr<StatExprParserStaticData> statexprParserStaticData = nullptr;

void statexprParserInitialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  if (statexprParserStaticData != nullptr) {
    return;
  }
#else
  assert(statexprParserStaticData == nullptr);
#endif
  auto staticData = std::make_unique<StatExprParserStaticData>(
    std::vector<std::string>{
      "program", "assignment", "expr", "postfix", "atom", "args"
    },
    std::vector<std::string>{
      "", "", "", "", "'='", "'+'", "'-'", "'*'", "'/'", "'^'", "'('", "')'", 
      "'{'", "'}'", "','"
    },
    std::vector<std::string>{
      "", "FIELD", "IDENT", "NUMBER", "EQ", "PLUS", "MINUS", "STAR", "SLASH", 
      "POW", "LPAREN", "RPAREN", "LBRACE", "RBRACE", "COMMA", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,15,79,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,1,0,1,0,1,0,
  	1,0,1,0,1,0,3,0,19,8,0,1,1,1,1,1,1,1,1,1,2,1,2,1,2,1,2,3,2,29,8,2,1,2,
  	1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,5,2,40,8,2,10,2,12,2,43,9,2,1,3,1,3,1,
  	3,1,3,1,3,3,3,50,8,3,5,3,52,8,3,10,3,12,3,55,9,3,1,4,1,4,1,4,3,4,60,8,
  	4,1,4,1,4,1,4,1,4,1,4,1,4,1,4,3,4,69,8,4,1,5,1,5,1,5,5,5,74,8,5,10,5,
  	12,5,77,9,5,1,5,0,1,4,6,0,2,4,6,8,10,0,2,1,0,5,6,1,0,7,8,84,0,18,1,0,
  	0,0,2,20,1,0,0,0,4,28,1,0,0,0,6,44,1,0,0,0,8,68,1,0,0,0,10,70,1,0,0,0,
  	12,13,3,2,1,0,13,14,5,0,0,1,14,19,1,0,0,0,15,16,3,4,2,0,16,17,5,0,0,1,
  	17,19,1,0,0,0,18,12,1,0,0,0,18,15,1,0,0,0,19,1,1,0,0,0,20,21,5,1,0,0,
  	21,22,5,4,0,0,22,23,3,4,2,0,23,3,1,0,0,0,24,25,6,2,-1,0,25,26,7,0,0,0,
  	26,29,3,4,2,5,27,29,3,6,3,0,28,24,1,0,0,0,28,27,1,0,0,0,29,41,1,0,0,0,
  	30,31,10,4,0,0,31,32,5,9,0,0,32,40,3,4,2,4,33,34,10,3,0,0,34,35,7,1,0,
  	0,35,40,3,4,2,4,36,37,10,2,0,0,37,38,7,0,0,0,38,40,3,4,2,3,39,30,1,0,
  	0,0,39,33,1,0,0,0,39,36,1,0,0,0,40,43,1,0,0,0,41,39,1,0,0,0,41,42,1,0,
  	0,0,42,5,1,0,0,0,43,41,1,0,0,0,44,53,3,8,4,0,45,46,5,12,0,0,46,47,5,2,
  	0,0,47,49,5,13,0,0,48,50,3,4,2,0,49,48,1,0,0,0,49,50,1,0,0,0,50,52,1,
  	0,0,0,51,45,1,0,0,0,52,55,1,0,0,0,53,51,1,0,0,0,53,54,1,0,0,0,54,7,1,
  	0,0,0,55,53,1,0,0,0,56,57,5,2,0,0,57,59,5,10,0,0,58,60,3,10,5,0,59,58,
  	1,0,0,0,59,60,1,0,0,0,60,61,1,0,0,0,61,69,5,11,0,0,62,69,5,1,0,0,63,69,
  	5,3,0,0,64,65,5,10,0,0,65,66,3,4,2,0,66,67,5,11,0,0,67,69,1,0,0,0,68,
  	56,1,0,0,0,68,62,1,0,0,0,68,63,1,0,0,0,68,64,1,0,0,0,69,9,1,0,0,0,70,
  	75,3,4,2,0,71,72,5,14,0,0,72,74,3,4,2,0,73,71,1,0,0,0,74,77,1,0,0,0,75,
  	73,1,0,0,0,75,76,1,0,0,0,76,11,1,0,0,0,77,75,1,0,0,0,9,18,28,39,41,49,
  	53,59,68,75
  };
  staticData->serializedATN = antlr4::atn::SerializedATNView(serializedATNSegment, sizeof(serializedATNSegment) / sizeof(serializedATNSegment[0]));

  antlr4::atn::ATNDeserializer deserializer;
  staticData->atn = deserializer.deserialize(staticData->serializedATN);

  const size_t count = staticData->atn->getNumberOfDecisions();
  staticData->decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    staticData->decisionToDFA.emplace_back(staticData->atn->getDecisionState(i), i);
  }
  statexprParserStaticData = std::move(staticData);
}

}

StatExprParser::StatExprParser(TokenStream *input) : StatExprParser(input, antlr4::atn::ParserATNSimulatorOptions()) {}

StatExprParser::StatExprParser(TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options) : Parser(input) {
  StatExprParser::initialize();
  _interpreter = new atn::ParserATNSimulator(this, *statexprParserStaticData->atn, statexprParserStaticData->decisionToDFA, statexprParserStaticData->sharedContextCache, options);
}

StatExprParser::~StatExprParser() {
  delete _interpreter;
}

const atn::ATN& StatExprParser::getATN() const {
  return *statexprParserStaticData->atn;
}

std::string StatExprParser::getGrammarFileName() const {
  return "StatExpr.g4";
}

const std::vector<std::string>& StatExprParser::getRuleNames() const {
  return statexprParserStaticData->ruleNames;
}

const dfa::Vocabulary& StatExprParser::getVocabulary() const {
  return statexprParserStaticData->vocabulary;
}

antlr4::atn::SerializedATNView StatExprParser::getSerializedATN() const {
  return statexprParserStaticData->serializedATN;
}


//----------------- ProgramContext ------------------------------------------------------------------

StatExprParser::ProgramContext::ProgramContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

StatExprParser::AssignmentContext* StatExprParser::ProgramContext::assignment() {
  return getRuleContext<StatExprParser::AssignmentContext>(0);
}

tree::TerminalNode* StatExprParser::ProgramContext::EOF() {
  return getToken(StatExprParser::EOF, 0);
}

StatExprParser::ExprContext* StatExprParser::ProgramContext::expr() {
  return getRuleContext<StatExprParser::ExprContext>(0);
}


size_t StatExprParser::ProgramContext::getRuleIndex() const {
  return StatExprParser::RuleProgram;
}


std::any StatExprParser::ProgramContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<StatExprVisitor*>(visitor))
    return parserVisitor->visitProgram(this);
  else
    return visitor->visitChildren(this);
}

StatExprParser::ProgramContext* StatExprParser::program() {
  ProgramContext *_localctx = _tracker.createInstance<ProgramContext>(_ctx, getState());
  enterRule(_localctx, 0, StatExprParser::RuleProgram);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(18);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 0, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(12);
      assignment();
      setState(13);
      match(StatExprParser::EOF);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(15);
      expr(0);
      setState(16);
      match(StatExprParser::EOF);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- AssignmentContext ------------------------------------------------------------------

StatExprParser::AssignmentContext::AssignmentContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* StatExprParser::AssignmentContext::FIELD() {
  return getToken(StatExprParser::FIELD, 0);
}

tree::TerminalNode* StatExprParser::AssignmentContext::EQ() {
  return getToken(StatExprParser::EQ, 0);
}

StatExprParser::ExprContext* StatExprParser::AssignmentContext::expr() {
  return getRuleContext<StatExprParser::ExprContext>(0);
}


size_t StatExprParser::AssignmentContext::getRuleIndex() const {
  return StatExprParser::RuleAssignment;
}


std::any StatExprParser::AssignmentContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<StatExprVisitor*>(visitor))
    return parserVisitor->visitAssignment(this);
  else
    return visitor->visitChildren(this);
}

StatExprParser::AssignmentContext* StatExprParser::assignment() {
  AssignmentContext *_localctx = _tracker.createInstance<AssignmentContext>(_ctx, getState());
  enterRule(_localctx, 2, StatExprParser::RuleAssignment);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(20);
    match(StatExprParser::FIELD);
    setState(21);
    match(StatExprParser::EQ);
    setState(22);
    expr(0);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExprContext ------------------------------------------------------------------

StatExprParser::ExprContext::ExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<StatExprParser::ExprContext *> StatExprParser::ExprContext::expr() {
  return getRuleContexts<StatExprParser::ExprContext>();
}

StatExprParser::ExprContext* StatExprParser::ExprContext::expr(size_t i) {
  return getRuleContext<StatExprParser::ExprContext>(i);
}

tree::TerminalNode* StatExprParser::ExprContext::PLUS() {
  return getToken(StatExprParser::PLUS, 0);
}

tree::TerminalNode* StatExprParser::ExprContext::MINUS() {
  return getToken(StatExprParser::MINUS, 0);
}

StatExprParser::PostfixContext* StatExprParser::ExprContext::postfix() {
  return getRuleContext<StatExprParser::PostfixContext>(0);
}

tree::TerminalNode* StatExprParser::ExprContext::POW() {
  return getToken(StatExprParser::POW, 0);
}

tree::TerminalNode* StatExprParser::ExprContext::STAR() {
  return getToken(StatExprParser::STAR, 0);
}

tree::TerminalNode* StatExprParser::ExprContext::SLASH() {
  return getToken(StatExprParser::SLASH, 0);
}


size_t StatExprParser::ExprContext::getRuleIndex() const {
  return StatExprParser::RuleExpr;
}


std::any StatExprParser::ExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<StatExprVisitor*>(visitor))
    return parserVisitor->visitExpr(this);
  else
    return visitor->visitChildren(this);
}


StatExprParser::ExprContext* StatExprParser::expr() {
   return expr(0);
}

StatExprParser::ExprContext* StatExprParser::expr(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  StatExprParser::ExprContext *_localctx = _tracker.createInstance<ExprContext>(_ctx, parentState);
  StatExprParser::ExprContext *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 4;
  enterRecursionRule(_localctx, 4, StatExprParser::RuleExpr, precedence);

    size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(28);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case StatExprParser::PLUS:
      case StatExprParser::MINUS: {
        setState(25);
        _la = _input->LA(1);
        if (!(_la == StatExprParser::PLUS

        || _la == StatExprParser::MINUS)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(26);
        expr(5);
        break;
      }

      case StatExprParser::FIELD:
      case StatExprParser::IDENT:
      case StatExprParser::NUMBER:
      case StatExprParser::LPAREN: {
        setState(27);
        postfix();
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    _ctx->stop = _input->LT(-1);
    setState(41);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 3, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        setState(39);
        _errHandler->sync(this);
        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 2, _ctx)) {
        case 1: {
          _localctx = _tracker.createInstance<ExprContext>(parentContext, parentState);
          pushNewRecursionContext(_localctx, startState, RuleExpr);
          setState(30);

          if (!(precpred(_ctx, 4))) throw FailedPredicateException(this, "precpred(_ctx, 4)");
          setState(31);
          match(StatExprParser::POW);
          setState(32);
          expr(4);
          break;
        }

        case 2: {
          _localctx = _tracker.createInstance<ExprContext>(parentContext, parentState);
          pushNewRecursionContext(_localctx, startState, RuleExpr);
          setState(33);

          if (!(precpred(_ctx, 3))) throw FailedPredicateException(this, "precpred(_ctx, 3)");
          setState(34);
          _la = _input->LA(1);
          if (!(_la == StatExprParser::STAR

          || _la == StatExprParser::SLASH)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(35);
          expr(4);
          break;
        }

        case 3: {
          _localctx = _tracker.createInstance<ExprContext>(parentContext, parentState);
          pushNewRecursionContext(_localctx, startState, RuleExpr);
          setState(36);

          if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
          setState(37);
          _la = _input->LA(1);
          if (!(_la == StatExprParser::PLUS

          || _la == StatExprParser::MINUS)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(38);
          expr(3);
          break;
        }

        default:
          break;
        } 
      }
      setState(43);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 3, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- PostfixContext ------------------------------------------------------------------

StatExprParser::PostfixContext::PostfixContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

StatExprParser::AtomContext* StatExprParser::PostfixContext::atom() {
  return getRuleContext<StatExprParser::AtomContext>(0);
}

std::vector<tree::TerminalNode *> StatExprParser::PostfixContext::LBRACE() {
  return getTokens(StatExprParser::LBRACE);
}

tree::TerminalNode* StatExprParser::PostfixContext::LBRACE(size_t i) {
  return getToken(StatExprParser::LBRACE, i);
}

std::vector<tree::TerminalNode *> StatExprParser::PostfixContext::IDENT() {
  return getTokens(StatExprParser::IDENT);
}

tree::TerminalNode* StatExprParser::PostfixContext::IDENT(size_t i) {
  return getToken(StatExprParser::IDENT, i);
}

std::vector<tree::TerminalNode *> StatExprParser::PostfixContext::RBRACE() {
  return getTokens(StatExprParser::RBRACE);
}

tree::TerminalNode* StatExprParser::PostfixContext::RBRACE(size_t i) {
  return getToken(StatExprParser::RBRACE, i);
}

std::vector<StatExprParser::ExprContext *> StatExprParser::PostfixContext::expr() {
  return getRuleContexts<StatExprParser::ExprContext>();
}

StatExprParser::ExprContext* StatExprParser::PostfixContext::expr(size_t i) {
  return getRuleContext<StatExprParser::ExprContext>(i);
}


size_t StatExprParser::PostfixContext::getRuleIndex() const {
  return StatExprParser::RulePostfix;
}


std::any StatExprParser::PostfixContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<StatExprVisitor*>(visitor))
    return parserVisitor->visitPostfix(this);
  else
    return visitor->visitChildren(this);
}

StatExprParser::PostfixContext* StatExprParser::postfix() {
  PostfixContext *_localctx = _tracker.createInstance<PostfixContext>(_ctx, getState());
  enterRule(_localctx, 6, StatExprParser::RulePostfix);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(44);
    atom();
    setState(53);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 5, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(45);
        match(StatExprParser::LBRACE);
        setState(46);
        match(StatExprParser::IDENT);
        setState(47);
        match(StatExprParser::RBRACE);
        setState(49);
        _errHandler->sync(this);

        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 4, _ctx)) {
        case 1: {
          setState(48);
          expr(0);
          break;
        }

        default:
          break;
        } 
      }
      setState(55);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 5, _ctx);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- AtomContext ------------------------------------------------------------------

StatExprParser::AtomContext::AtomContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* StatExprParser::AtomContext::IDENT() {
  return getToken(StatExprParser::IDENT, 0);
}

tree::TerminalNode* StatExprParser::AtomContext::LPAREN() {
  return getToken(StatExprParser::LPAREN, 0);
}

tree::TerminalNode* StatExprParser::AtomContext::RPAREN() {
  return getToken(StatExprParser::RPAREN, 0);
}

StatExprParser::ArgsContext* StatExprParser::AtomContext::args() {
  return getRuleContext<StatExprParser::ArgsContext>(0);
}

tree::TerminalNode* StatExprParser::AtomContext::FIELD() {
  return getToken(StatExprParser::FIELD, 0);
}

tree::TerminalNode* StatExprParser::AtomContext::NUMBER() {
  return getToken(StatExprParser::NUMBER, 0);
}

StatExprParser::ExprContext* StatExprParser::AtomContext::expr() {
  return getRuleContext<StatExprParser::ExprContext>(0);
}


size_t StatExprParser::AtomContext::getRuleIndex() const {
  return StatExprParser::RuleAtom;
}


std::any StatExprParser::AtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<StatExprVisitor*>(visitor))
    return parserVisitor->visitAtom(this);
  else
    return visitor->visitChildren(this);
}

StatExprParser::AtomContext* StatExprParser::atom() {
  AtomContext *_localctx = _tracker.createInstance<AtomContext>(_ctx, getState());
  enterRule(_localctx, 8, StatExprParser::RuleAtom);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(68);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case StatExprParser::IDENT: {
        enterOuterAlt(_localctx, 1);
        setState(56);
        match(StatExprParser::IDENT);
        setState(57);
        match(StatExprParser::LPAREN);
        setState(59);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 1134) != 0)) {
          setState(58);
          args();
        }
        setState(61);
        match(StatExprParser::RPAREN);
        break;
      }

      case StatExprParser::FIELD: {
        enterOuterAlt(_localctx, 2);
        setState(62);
        match(StatExprParser::FIELD);
        break;
      }

      case StatExprParser::NUMBER: {
        enterOuterAlt(_localctx, 3);
        setState(63);
        match(StatExprParser::NUMBER);
        break;
      }

      case StatExprParser::LPAREN: {
        enterOuterAlt(_localctx, 4);
        setState(64);
        match(StatExprParser::LPAREN);
        setState(65);
        expr(0);
        setState(66);
        match(StatExprParser::RPAREN);
        break;
      }

    default:
      throw NoViableAltException(this);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ArgsContext ------------------------------------------------------------------

StatExprParser::ArgsContext::ArgsContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<StatExprParser::ExprContext *> StatExprParser::ArgsContext::expr() {
  return getRuleContexts<StatExprParser::ExprContext>();
}

StatExprParser::ExprContext* StatExprParser::ArgsContext::expr(size_t i) {
  return getRuleContext<StatExprParser::ExprContext>(i);
}

std::vector<tree::TerminalNode *> StatExprParser::ArgsContext::COMMA() {
  return getTokens(StatExprParser::COMMA);
}

tree::TerminalNode* StatExprParser::ArgsContext::COMMA(size_t i) {
  return getToken(StatExprParser::COMMA, i);
}


size_t StatExprParser::ArgsContext::getRuleIndex() const {
  return StatExprParser::RuleArgs;
}


std::any StatExprParser::ArgsContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<StatExprVisitor*>(visitor))
    return parserVisitor->visitArgs(this);
  else
    return visitor->visitChildren(this);
}

StatExprParser::ArgsContext* StatExprParser::args() {
  ArgsContext *_localctx = _tracker.createInstance<ArgsContext>(_ctx, getState());
  enterRule(_localctx, 10, StatExprParser::RuleArgs);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(70);
    expr(0);
    setState(75);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == StatExprParser::COMMA) {
      setState(71);
      match(StatExprParser::COMMA);
      setState(72);
      expr(0);
      setState(77);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

bool StatExprParser::sempred(RuleContext *context, size_t ruleIndex, size_t predicateIndex) {
  switch (ruleIndex) {
    case 2: return exprSempred(antlrcpp::downCast<ExprContext *>(context), predicateIndex);

  default:
    break;
  }
  return true;
}

bool StatExprParser::exprSempred(ExprContext *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 0: return precpred(_ctx, 4);
    case 1: return precpred(_ctx, 3);
    case 2: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

void StatExprParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  statexprParserInitialize();
#else
  ::antlr4::internal::call_once(statexprParserOnceFlag, statexprParserInitialize);
#endif
}
